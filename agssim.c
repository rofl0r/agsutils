#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "ags_cpu.h"
#include "regusage.h"
#include "hbmap.h"
#include "version.h"
#define ADS ":::AGSSim " VERSION " by rofl0r:::"

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

static struct text_segment {
	int *code;
	size_t len;
	size_t capa;
} text;

struct label_ref {
	char *name;
	unsigned insno;
};
tglist(struct label_ref) *label_refs;
static void add_label_ref(char *name, unsigned insno) {
	struct label_ref new = {.name = strdup(name), .insno = insno};
	tglist_add(label_refs, new);
}
static void resolve_label(char* name, unsigned insno) {
	size_t i;
	for(i=0; i<tglist_getsize(label_refs); ) {
		struct label_ref *l = &tglist_get(label_refs, i);
		if(!strcmp(l->name, name)) {
			free(l->name);
			text.code[l->insno] = insno;
			tglist_delete(label_refs, i);
		} else ++i;
	}
}
/* label_map */
hbmap(char*, unsigned, 32) *label_map;
static unsigned *get_label_offset(char* name) {
	return hbmap_get(label_map, name);
}
static int add_label(char* name, int insno) {
	char* tmp = strdup(name);
	return hbmap_insert(label_map, tmp, insno) != -1;
}
static int strptrcmp(const void *a, const void *b) {
	const char * const *x = a;
	const char * const *y = b;
	return strcmp(*x, *y);
}
static unsigned string_hash(const char* s) {
	uint_fast32_t h = 0;
	while (*s) {
		h = 16*h + *s++;
		h ^= h>>24 & 0xf0;
	}
	return h & 0xfffffff;
}
static void init_labels() {
	label_map = hbmap_new(strptrcmp, string_hash, 32);
	label_refs = tglist_new();
}

/* TODO: move duplicate code from Assembler.c into separate TU */
static int get_reg(char* regname) {
	int i = AR_NULL + 1;
	for(; i < AR_MAX; i++)
		if(strcmp(regnames[i], regname) == 0)
			return i;
	return AR_NULL;
}

static size_t mnemolen[SCMD_MAX];
static int mnemolen_initdone = 0;

static void init_mnemolen(void) {
	size_t i = 0;
	for(; i< SCMD_MAX; i++)
		mnemolen[i] = strlen(opcodes[i].mnemonic);
	mnemolen_initdone = 1;
}

static unsigned find_insn(char* sym) {
	if(!mnemolen_initdone) init_mnemolen();
	size_t i = 0, l = strlen(sym);
	for(; i< SCMD_MAX; i++)
		if(l == mnemolen[i] && memcmp(sym, opcodes[i].mnemonic, l) == 0)
			return i;
	return 0;
}

#include "StringEscape.h"
/* expects a pointer to the first char after a opening " in a string,
 * converts the string into convbuf, and returns the length of that string */
static size_t get_length_and_convert(char* x, char* end, char* convbuf, size_t convbuflen) {
	size_t result = 0;
	char* e = x + strlen(x);
	assert(e > x && e < end && *e == 0);
	e--;
	while(isspace(*e)) e--;
	if(*e != '"') return (size_t) -1;
	*e = 0;
	result = unescape(x, convbuf, convbuflen);
	return result;
}

/* sets lets char in arg to 0, and advances pointer till the next argstart */
static char* finalize_arg(char **p, char* pend, char* convbuf, size_t convbuflen) {
	if(**p == '"') {
		convbuf[0] = '"';
		size_t l= get_length_and_convert(*p + 1, pend, convbuf+1, convbuflen - 1);
		if(l == (size_t) -1) return 0;
		convbuf[l+1] = '"';
		convbuf[l+2] = 0;
		*p = 0; /* make it crash if its accessed again, since a string should always be the last arg */
		return convbuf;
	} else {
		char* ret = *p;
		while(*p < pend && **p != ',' && !isspace(**p)) (*p)++;
		assert(*p < pend);
		**p = 0; (*p)++;
		while(*p < pend && isspace(**p)) (*p)++;
		assert(*p < pend);
		return ret;
	}
}

static struct rval {
	union {
		int i;
		float f;
	};
	enum RegisterUsage ru;
} registers[AR_MAX];

static unsigned char stack_mem[1000*4];
#define memory stack_mem

static int canread(int index, int cnt) {
	return index >= 0 && index+cnt < sizeof(memory)/sizeof(memory[0]);
}

static void grow_text(size_t req) {
	if(text.len + req > text.capa) {
		text.code = realloc(text.code, (text.capa+1024)*sizeof(int));
		text.capa += 1024;
	}
}

static void append_code(int *code, size_t cnt) {
	grow_text(cnt+1);
	size_t i;
	for(i = 0; i < cnt; i++) {
		text.code[text.len++] = code[i];
	}
	text.code[text.len] = 0;
}

static void vm_reset_register_usage() {
	size_t i;
	for(i = AR_NULL + 1; i < AR_MAX; i++)
		registers[i].ru = RU_NONE;
}

static void vm_init() {
	size_t i;
	/* initialize registers to an easily recognisable junk value */
	for(i = AR_NULL + 1; i < AR_MAX; i++) {
		registers[i].i = 2222222222;
	}
	vm_reset_register_usage();
	registers[AR_SP].i = 0;
	registers[AR_NULL].i = 0;
	/* set up EIP so vm_state() doesn't crash */
	grow_text(16);
	/* put NULL insn as first instruction so VM doesn't execute
	   random garbage in mem */
	text.code[0] = 0;
}

static inline int consume_int(int **eip) {
	*eip = *eip+1;
	return **eip;
}

static void change_reg_usage(int regno, enum RegisterAccess ra) {
	registers[regno].ru = get_reg_usage(regno, registers[regno].ru, ra);
}

static void vm_update_register_usage(int *eip) {
	const struct regaccess_info *ri = &regaccess_info[*eip];
	if(ri->ra_reg1) change_reg_usage(eip[1], ri->ra_reg1);
	if(ri->ra_reg2) change_reg_usage(eip[2], ri->ra_reg2);
	if(ri->ra_mar) change_reg_usage(AR_MAR, ri->ra_mar);
	if(ri->ra_sp) change_reg_usage(AR_SP, ri->ra_sp);
}

static void write_mem1(int off, int val) {
	unsigned char *m = (void*) memory;
	m[off] = val&0xff;
}
static void write_mem2(int off, int val) {
	unsigned short *m = (void*) memory;
	m[off/2] = val&0xffff;
}
static void write_mem(int off, int val) {
	int *m = (void*) memory;
	m[off/4] = val;
}

static int read_mem(int off) {
	int *m = (void*) memory;
	return m[off/4];
}

#define CODE_INT(X) eip[X]
#define CODE_FLOAT(X) ((float*)eip)[X]
#define REGI(X) registers[CODE_INT(X)].i
#define REGF(X) registers[CODE_INT(X)].f

static int vm_step(int run_context) {
	if(tglist_getsize(label_refs)) {
		dprintf(2, "error: unresolved label refs!\n");
		size_t i; struct label_ref *l;
		for(i=0; i<tglist_getsize(label_refs); ++i) {
			l = &tglist_get(label_refs, i);
			dprintf(2, "%s@%u\n", l->name, l->insno);
		}
		return 0;
	}
	/* we use register AR_NULL as instruction pointer */
#define EIP registers[AR_NULL].i
	int *eip = &text.code[EIP];
	int eip_inc = 1 + opcodes[*eip].argcount;
	int tmp, val;
	if(!run_context) vm_reset_register_usage();
	vm_update_register_usage(eip);

	switch(*eip) {
		case 0:
			/* don't modify IP */
			dprintf(2, "no code at IP %u.\n", EIP);
			return 0;
		case SCMD_ADD:
			REGI(1) += CODE_INT(2);
			break;
		case SCMD_SUB:
			REGI(1) -= CODE_INT(2);
			break;
		case SCMD_REGTOREG:
			REGI(2) = REGI(1);
			break;
		case SCMD_LITTOREG:
			REGI(1) = CODE_INT(2);
			break;
		case SCMD_MULREG:
			REGI(1) *= REGI(2);
			break;
		case SCMD_DIVREG:
			REGI(1) /= REGI(2);
			break;
		case SCMD_ADDREG:
			REGI(1) += REGI(2);
			break;
		case SCMD_SUBREG:
			REGI(1) -= REGI(2);
			break;
		case SCMD_BITAND:
			REGI(1) &= REGI(2);
			break;
		case SCMD_BITOR:
			REGI(1) &= REGI(2);
			break;
		case SCMD_ISEQUAL:
			REGI(1) = !!(REGI(1) == REGI(2));
			break;
		case SCMD_NOTEQUAL:
			REGI(1) = !!(REGI(1) != REGI(2));
			break;
		case SCMD_GREATER:
			REGI(1) = !!(REGI(1) > REGI(2));
			break;
		case SCMD_LESSTHAN:
			REGI(1) = !!(REGI(1) < REGI(2));
			break;
		case SCMD_GTE:
			REGI(1) = !!(REGI(1) >= REGI(2));
			break;
		case SCMD_LTE:
			REGI(1) = !!(REGI(1) <= REGI(2));
			break;
		case SCMD_AND:
			REGI(1) = !!(REGI(1) && REGI(2));
			break;
		case SCMD_OR:
			REGI(1) = !!(REGI(1) || REGI(2));
			break;
		case SCMD_LOADSPOFFS:
			registers[AR_MAR].i = registers[AR_SP].i - CODE_INT(1);
			break;
		case SCMD_PUSHREG:
			if(canread(registers[AR_SP].i, 4)) {
				write_mem(registers[AR_SP].i, REGI(1));
				registers[AR_SP].i += 4;
			} else goto oob;
			break;
		case SCMD_POPREG:
			if((int) registers[AR_SP].i >= 4) {
				registers[AR_SP].i -= 4;
				REGI(1) = read_mem(registers[AR_SP].i);
			} else goto oob;
			break;
		case SCMD_MUL:
			REGI(1) *= CODE_INT(2);
			break;
		case SCMD_THISBASE:
		case SCMD_LINENUM:
			break;
		case SCMD_MODREG:
			REGI(1) %= REGI(2);
			break;
		case SCMD_XORREG:
			REGI(1) ^= REGI(2);
			break;
		case SCMD_NOTREG:
			REGI(1) = !REGI(2);
			break;
		case SCMD_SHIFTLEFT:
			REGI(1) <<= REGI(2);
			break;
		case SCMD_SHIFTRIGHT:
			REGI(1) >>= REGI(2);
			break;
		case SCMD_FADD:
			REGF(1) += CODE_FLOAT(2);
			break;
		case SCMD_FSUB:
			REGF(1) -= CODE_FLOAT(2);
			break;
		case SCMD_FMULREG:
			REGF(1) *= REGF(2);
			break;
		case SCMD_FDIVREG:
			REGF(1) /= REGF(2);
			break;
		case SCMD_FADDREG:
			REGF(1) += REGF(2);
			break;
		case SCMD_FSUBREG:
			REGF(1) -= REGF(2);
			break;
		case SCMD_FGREATER:
			REGI(1) = !!(REGF(1) > REGF(2));
			break;
		case SCMD_FLESSTHAN:
			REGI(1) = !!(REGF(1) < REGF(2));
			break;
		case SCMD_FGTE:
			REGI(1) = !!(REGF(1) >= REGF(2));
			break;
		case SCMD_FLTE:
			REGI(1) = !!(REGF(1) <= REGF(2));
			break;
		case SCMD_WRITELIT:
			tmp = CODE_INT(1);
			if(tmp <= 0 || tmp > 4 || tmp == 3) {
				dprintf(2, "VM: invalid memcpy use at IP %u\n", EIP);
				break;
			}
			val = CODE_INT(2);
			goto mwrite;
		case SCMD_MEMWRITE:
			tmp = 4;
			val = REGI(1);
			goto mwrite;
		case SCMD_MEMWRITEW:
			tmp = 2;
			val = REGI(1);
			goto mwrite;
		case SCMD_MEMWRITEB:
			tmp = 1;
			val = REGI(1);
		mwrite:
			if(canread(registers[AR_MAR].i, tmp)) {
				switch(tmp) {
				case 4:	write_mem (registers[AR_MAR].i, val); break;
				case 2:	write_mem2(registers[AR_MAR].i, val); break;
				case 1:	write_mem1(registers[AR_MAR].i, val); break;
				}
			} else {
		oob:
				dprintf(2, "info: caught OOB access at IP %u\n", EIP);
			}
			break;
		case SCMD_MEMREAD:
			tmp = 4;
			goto mread;
		case SCMD_MEMREADW:
			tmp = 2;
			goto mread;
		case SCMD_MEMREADB:
			tmp = 1;
		mread:
			if(canread(registers[AR_MAR].i, tmp)) {
				int val = memory[registers[AR_MAR].i];
				switch(tmp) {
				case 4:	REGI(1) = val; break;
				case 2:	REGI(1) = val & 0xffff; break;
				case 1:	REGI(1) = val & 0xff; break;
				}
			} else goto oob;
			break;
		case SCMD_JZ:
			if(registers[AR_AX].i == 0) goto jump;
			break;
		case SCMD_JNZ:
			if(registers[AR_AX].i == 0) break;
			/* fall through */
		case SCMD_JMP:
		jump:
			tmp = CODE_INT(1);
		jump_tmp:
			if((unsigned)tmp <= text.len)
				registers[AR_NULL].i = tmp;
			else dprintf(2, "error: caught invalid jump to %u at IP %u\n", tmp, EIP);
			eip_inc = 0;
			break;
		case SCMD_CALL:
			tmp = REGI(1);
			write_mem(registers[AR_SP].i, registers[AR_NULL].i + eip_inc);
			registers[AR_SP].i += 4;
			goto jump_tmp;
		case SCMD_RET:
			registers[AR_SP].i -= 4;
			tmp = read_mem(registers[AR_SP].i);
			goto jump_tmp;
		case SCMD_NEWARRAY:
		case SCMD_DYNAMICBOUNDS:
		case SCMD_MEMZEROPTRND:
		case SCMD_LOOPCHECKOFF:
		case SCMD_CHECKNULLREG:
		case SCMD_STRINGSNOTEQ:
		case SCMD_STRINGSEQUAL:
		case SCMD_CREATESTRING:
		case SCMD_ZEROMEMORY:
		case SCMD_CHECKNULL:
		case SCMD_MEMINITPTR:
		case SCMD_MEMZEROPTR:
		case SCMD_MEMREADPTR:
		case SCMD_MEMWRITEPTR:
		case SCMD_CHECKBOUNDS:
		case SCMD_CALLOBJ:
		case SCMD_NUMFUNCARGS:
		case SCMD_CALLAS:
		case SCMD_SUBREALSTACK:
		case SCMD_PUSHREAL:
		case SCMD_CALLEXT:
		default:
			dprintf(2, "info: %s not implemented yet\n", opcodes[*eip].mnemonic);
			{
				size_t i, l = opcodes[*eip].argcount;
				for(i = 0; i < l; i++) ++(*eip);
			}
			break;
	}
	registers[AR_NULL].i += eip_inc;
	return 1;
}

static inline char *int_to_str(int value, char* out) {
	sprintf(out, "%d", value);
	return out;
}

static void vm_state() {
	static const char ru_strings[][3] = {
		[RU_NONE] = {0},
		[RU_READ] = {'R', 0},
		[RU_WRITE] = {'W', 0},
		[RU_WRITE_AFTER_READ] = {'R', 'W', 0},
	};
	static const char regorder[] = {
		0, AR_MAR, AR_OP, AR_SP, -1,
		AR_AX, AR_BX, AR_CX, AR_DX, -1, -1};
	size_t i, j;
	for(j=0; j < ARRAY_SIZE(regorder)-1; ++j) {
		i = regorder[j];
		if(i == -1) printf("\n");
		else {
			printf("%-3s: %-2s %-11d", i == 0 ? "eip" : regnames[i], ru_strings[registers[i].ru], registers[i].i);
			if(regorder[j+1] != -1) printf(" ");
		}
	}

	for(	i = MIN(registers[AR_SP].i+2*4, sizeof(stack_mem)/4);
		i >= MAX(registers[AR_SP].i-2*4, 0);
		i-=4) {
		printf("SL %s %3zu %d\n", i == registers[AR_SP].i ? ">" : " ", i, read_mem(i));
		if(i == 0) break;
	}

	int *eip = &text.code[registers[AR_NULL].i];
	char arg1buf[32], arg2buf[32];
	const char *arg1 = opcodes[*eip].argcount == 0 ? "" : \
		(opcodes[*eip].regcount > 0 ? regnames[eip[1]] : int_to_str(eip[1], arg1buf));
	const char *arg2 = opcodes[*eip].argcount < 2 ? "" : \
		(opcodes[*eip].regcount > 1 ? regnames[eip[2]] : int_to_str(eip[2], arg2buf));
	printf(" > %s %s %s\n", opcodes[*eip].mnemonic, arg1, arg2);
}

void vm_run(void) {
	while(1) {
		int *eip = &text.code[registers[AR_NULL].i];
		if(!*eip) break;
		if(!vm_step(1)) break;
	}
}

static int usage(int fd, char *a0) {
	dprintf(fd,
		"%s - simple ags vm simulator\n"
		"implements the ALU and a small stack\n"
		"useful to examine how a chunk of code modifies VM state\n"
		"not implemented: memory access apart from stack, jumps, functions\n"
		"supply the assembly code via stdin, then type one of the following\n"
		"commands:\n"
		"!i - reset VM state and IP\n"
		"!s - single-step\n"
		"!r - run\n"
	, a0);
	return 1;
}

static int lastcommand;
enum UserCommand {
	UC_STEP = 1,
	UC_RUN,
	UC_INIT,
	UC_QUIT,
	UC_HELP,
};
static void execute_user_command_i(int uc) {
	switch(uc) {
		case UC_STEP: vm_step(0); break;
		case UC_RUN : vm_run(); break;
		case UC_INIT: vm_init(); break;
		case UC_QUIT: exit(0); break;
		case UC_HELP: usage(1, "agssim"); break;
	}
	lastcommand = uc;
	vm_state();
}
static void execute_user_command(char *cmd) {
	int uc = 0;
	if(0) ;
	else if(!strcmp(cmd, "s")) uc = UC_STEP;
	else if(!strcmp(cmd, "r")) uc = UC_RUN;
	else if(!strcmp(cmd, "i")) uc = UC_INIT;
	else if(!strcmp(cmd, "q")) uc = UC_QUIT;
	else if(!strcmp(cmd, "h")) uc = UC_HELP;
	else {
		dprintf(2, "unknown command\n");
		return;
	}
	execute_user_command_i(uc);
}

int main(int argc, char** argv) {
	if(argc != 1) return usage(2, argv[0]);
	char buf[1024], *sym;
	char convbuf[sizeof(buf)]; /* to convert escaped string into non-escaped version */
	int lineno = 0;
	init_labels();
	vm_init();
	printf(ADS " - type !h for help\n");
	while(fgets(buf, sizeof buf, stdin)) {
		int code[4];
		size_t pos = 0;
		lineno++;
		char* p = buf, *pend = buf + sizeof buf;
		if(*p == '\n' && lastcommand) {
			execute_user_command_i(lastcommand);
			continue;
		}
		if(*p == '#' || *p == ';') continue;
		if(*p == '!') {
			char *n = strchr(p, '\n');
			if(n) *n = 0;
			execute_user_command(p+1);
			continue;
		}
		while(isspace(*p) && p < pend) p++;
		assert(p < pend);
		if(!*p) continue;
		char* sym = p;
		while(!isspace(*p) && p < pend) p++;
		*p = 0; p++;
		size_t l = strlen(sym);
		if(l > 1 && sym[l-1] == ':') {
			// functionstart or label
			sym[l-1] = 0;
			resolve_label(sym, text.len);
			unsigned *loff = get_label_offset(sym);
			if(loff) dprintf(2, "warning: label %s overwritten\n");
			add_label(sym, text.len);
			continue;
		}
		unsigned instr = find_insn(sym);
		if(!instr) {
			dprintf(2, "line %zu: error: unknown instruction '%s'\n", lineno, sym);
			continue;
		}
		code[pos++] = instr;
		size_t arg;
		for(arg = 0; arg < opcodes[instr].argcount; arg++) {
			sym = finalize_arg(&p, pend, convbuf, sizeof(convbuf));
			if(sym == 0) {
				dprintf(2, "line %zu: error: expected \"\n", lineno);
				goto loop_footer;
			}
			int value = 0;
			if(arg < opcodes[instr].regcount) {
				value=get_reg(sym);
				if(value == AR_NULL) {
			needreg_err:
					dprintf(2, "line %zu: error: expected register name!\n", lineno);
					goto loop_footer;
				}
				if(instr == SCMD_REGTOREG) {
					/* fix reversed order of arguments */
					int dst = value;
					sym = p;
					while(p < pend && *p != ',' && !isspace(*p)) p++;
					assert(p < pend);
					*p = 0;
					value=get_reg(sym);
					if(value == AR_NULL) goto needreg_err;
					code[pos++] = value;
					code[pos++] = dst;
					break;
				}
			} else {
				switch(instr) {
					case SCMD_LITTOREG:
						/* immediate can be function name, string, 
							* variable name, stack fixup, or numeric value */
						if(sym[0] == '"') {
							dprintf(2, "error: string handling not implemented\n");
							goto loop_footer;
						} else if(sym[0] == '@') {
							dprintf(2, "error: global variable handling not implemented\n");
							goto loop_footer;
						} else if(sym[0] == '.') {
							if(memcmp(sym+1, "stack", 5)) {
								dprintf(2, "error: expected stack\n");
								goto loop_footer;;
							}
							dprintf(2, "error: stack fixup not implemented\n");
							goto loop_footer;
						} else if(isdigit(sym[0]) || sym[0] == '-') {
							if(sym[0] == '-') assert(isdigit(sym[1]));
							value = atoi(sym);
						} else {
							goto label_ref;
						}
						break;
					case SCMD_JMP: case SCMD_JZ: case SCMD_JNZ: {
					label_ref:;
						unsigned *loff = get_label_offset(sym);
						if(!loff) {
							add_label_ref(sym, text.len+pos);
							value = -1;
						} else value = *loff;
						} break;
					default:
						if(!isdigit(sym[0])) {
							dprintf(2, "error: expected number\n");
							goto loop_footer;
						}
						value = atoi(sym);
				}
			}
			code[pos++] = value;
		}
		append_code(code, pos);
loop_footer: ;
	}
}

