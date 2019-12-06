#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "ags_cpu.h"
#include "version.h"
#define ADS ":::AGSSim " VERSION " by rofl0r:::"

enum RegisterAccess {
	RA_NONE = 0,
	RA_READ = 1 << 0,
	RA_WRITE = 1 << 1,
	RA_READWRITE = 1 << 2,
};

struct regaccess_info {
	/* enum RegisterAccess */ unsigned char ra_reg1;
	/* enum RegisterAccess */ unsigned char ra_reg2;
} __attribute__((packed));

static const struct regaccess_info regaccess_info[] = {
	[0] = {RA_NONE, RA_NONE},
	[SCMD_ADD] = {RA_READWRITE, RA_NONE},
	[SCMD_SUB] = {RA_READWRITE, RA_NONE},
	[SCMD_REGTOREG] = {RA_READ, RA_WRITE},
	[SCMD_WRITELIT] = {RA_NONE, RA_NONE}, // TODO
	[SCMD_RET] = {RA_NONE, RA_NONE},
	[SCMD_LITTOREG] = {RA_WRITE, RA_NONE},
	[SCMD_MEMREAD] = {RA_WRITE, RA_NONE},
	[SCMD_MEMWRITE] = {RA_READ, RA_NONE},
	[SCMD_MULREG] = {RA_READWRITE, RA_READ},
	[SCMD_DIVREG] = {RA_READWRITE, RA_READ},
	[SCMD_ADDREG] = {RA_READWRITE, RA_READ},
	[SCMD_SUBREG] = {RA_READWRITE, RA_READ},
	[SCMD_BITAND] = {RA_READWRITE, RA_READ},
	[SCMD_BITOR] = {RA_READWRITE, RA_READ},
	[SCMD_ISEQUAL] = {RA_READWRITE, RA_READ},
	[SCMD_NOTEQUAL] = {RA_READWRITE, RA_READ},
	[SCMD_GREATER] = {RA_READWRITE, RA_READ},
	[SCMD_LESSTHAN] = {RA_READWRITE, RA_READ},
	[SCMD_GTE] = {RA_READWRITE, RA_READ},
	[SCMD_LTE] = {RA_READWRITE, RA_READ},
	[SCMD_AND] = {RA_READWRITE, RA_READ}, /*logical*/
	[SCMD_OR] = {RA_READWRITE, RA_READ},
	[SCMD_CALL] = {RA_READ, RA_NONE},
	[SCMD_MEMREADB] = {RA_WRITE, RA_NONE},
	[SCMD_MEMREADW] = {RA_WRITE, RA_NONE},
	[SCMD_MEMWRITEB] = {RA_READ, RA_NONE},
	[SCMD_MEMWRITEW] = {RA_READ, RA_NONE},
	[SCMD_JZ] = {RA_READ, RA_NONE},
	[SCMD_PUSHREG] = {RA_READ, RA_NONE},
	[SCMD_POPREG] = {RA_WRITE, RA_NONE},
	[SCMD_JMP] = {RA_READ, RA_NONE},
	[SCMD_MUL] = {RA_READWRITE, RA_NONE},
	[SCMD_CALLEXT] = {RA_READ, RA_NONE},
	[SCMD_PUSHREAL] = {RA_READ, RA_NONE},
	[SCMD_SUBREALSTACK] = {RA_READ, RA_NONE},
	[SCMD_LINENUM] = {RA_NONE, RA_NONE},
	[SCMD_CALLAS] = {RA_READ, RA_NONE},
	[SCMD_THISBASE] = {RA_NONE, RA_NONE},
	[SCMD_NUMFUNCARGS] = {RA_NONE, RA_NONE},
	[SCMD_MODREG] = {RA_READWRITE, RA_READ},
	[SCMD_XORREG] = {RA_READWRITE, RA_READ},
	[SCMD_NOTREG] = {RA_READWRITE, RA_READ},
	[SCMD_SHIFTLEFT] = {RA_READWRITE, RA_READ},
	[SCMD_SHIFTRIGHT] = {RA_READWRITE, RA_READ},
	[SCMD_CALLOBJ] = {RA_READ, RA_NONE},
	[SCMD_CHECKBOUNDS] = {RA_READ, RA_NONE},
	[SCMD_MEMWRITEPTR] = {RA_NONE, RA_NONE}, //TODO
	[SCMD_MEMREADPTR] = {RA_NONE, RA_NONE}, //TODO
	[SCMD_MEMZEROPTR] = {RA_NONE, RA_NONE},
	[SCMD_MEMINITPTR] = {RA_NONE, RA_NONE}, //TODO
	[SCMD_LOADSPOFFS] = {RA_NONE, RA_NONE},
	[SCMD_CHECKNULL] = {RA_NONE, RA_NONE},
	[SCMD_FADD] = {RA_READWRITE, RA_NONE},
	[SCMD_FSUB] = {RA_READWRITE, RA_NONE},
	[SCMD_FMULREG] = {RA_READWRITE, RA_READ},
	[SCMD_FDIVREG] = {RA_READWRITE, RA_READ},
	[SCMD_FADDREG] = {RA_READWRITE, RA_READ},
	[SCMD_FSUBREG] = {RA_READWRITE, RA_READ},
	[SCMD_FGREATER] = {RA_READWRITE, RA_READ},
	[SCMD_FLESSTHAN] = {RA_READWRITE, RA_READ},
	[SCMD_FGTE] = {RA_READWRITE, RA_READ},
	[SCMD_FLTE] = {RA_READWRITE, RA_READ},
	[SCMD_ZEROMEMORY] = {RA_NONE, RA_NONE},
	[SCMD_CREATESTRING] = {RA_NONE, RA_NONE}, //TODO
	[SCMD_STRINGSEQUAL] = {RA_READWRITE, RA_READ},
	[SCMD_STRINGSNOTEQ] = {RA_READWRITE, RA_READ},
	[SCMD_CHECKNULLREG] = {RA_NONE, RA_NONE}, //TODO
	[SCMD_LOOPCHECKOFF] = {RA_NONE, RA_NONE},
	[SCMD_MEMZEROPTRND] = {RA_NONE, RA_NONE},
	[SCMD_JNZ] = {RA_NONE, RA_NONE},
	[SCMD_DYNAMICBOUNDS] = {RA_NONE, RA_NONE}, //TODO
	[SCMD_NEWARRAY] = {RA_NONE, RA_NONE}, //TODO
};

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

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


static struct text_segment {
	int *code;
	size_t len;
	size_t capa;
} text;

enum RegisterUsage {
	RU_NONE = 0,
	RU_READ = 1 << 0,
	RU_WRITE = 1 << 1,
	RU_WRITE_AFTER_READ = 1 << 2,
};

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
	grow_text(cnt);
	size_t i;
	for(i = 0; i < cnt; i++) {
		text.code[text.len++] = code[i];
	}
}

static void vm_init() {
	size_t i;
	/* initialize registers to an easily recognisable junk value */
	for(i = AR_NULL + 1; i < AR_MAX; i++) {
		registers[i].i = 2222222222;
		registers[i].ru = RU_NONE;
	}
	registers[AR_SP].i = 0;
	registers[AR_NULL].i = 0;
	/* set up EIP so vm_state() doesn't crash */
	grow_text(16);
}

static inline int consume_int(int **eip) {
	*eip = *eip+1;
	return **eip;
}

static void change_reg_usage(int regno, enum RegisterAccess ra) {
	enum RegisterUsage ru = registers[regno].ru;
	switch(ra) {
	case RA_READ:
		if(ru == RU_NONE || ru == RU_READ) ru = RU_READ;
		else if(ru == RU_WRITE);
		else if(ru == RU_WRITE_AFTER_READ);
		break;
	case RA_WRITE:
		if(ru == RU_NONE || ru == RU_WRITE) ru = RU_WRITE;
		else if(ru == RU_READ) ru = RU_WRITE_AFTER_READ;
		else if(ru == RU_WRITE_AFTER_READ);
		break;
	case RA_READWRITE:
		if(ru == RU_NONE || ru == RU_READ) ru = RU_WRITE_AFTER_READ;
		else if(ru == RU_WRITE);
		else if(ru == RU_WRITE_AFTER_READ);
		break;
	}
	registers[regno].ru = ru;
}

static void vm_update_register_usage(int *eip) {
	const struct regaccess_info *ri = &regaccess_info[*eip];
	if(ri->ra_reg1) change_reg_usage(eip[1], ri->ra_reg1);
	if(ri->ra_reg2) change_reg_usage(eip[2], ri->ra_reg2);
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

static void vm_step() {
	/* we use register AR_NULL as instruction pointer */
	int *eip = &text.code[registers[AR_NULL].i];
	int eip_inc = 1 + opcodes[*eip].argcount;
	int tmp, val;
	vm_update_register_usage(eip);

	switch(*eip) {
		case 0:
			/* don't modify IP */
			dprintf(2, "no code at IP.\n");
			return;
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
			write_mem(registers[AR_SP].i, REGI(1));
			registers[AR_SP].i += 4;
			break;
		case SCMD_POPREG:
			registers[AR_SP].i -= 4;
			REGI(1) = read_mem(registers[AR_SP].i);
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
				dprintf(2, "invalid memcpy use\n");
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
				dprintf(2, "info: caught OOB memwrite\n");
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
			} else {
				dprintf(2, "info: caught OOB memread\n");
			}
			break;
		case SCMD_NEWARRAY:
		case SCMD_DYNAMICBOUNDS:
		case SCMD_JNZ:
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
		case SCMD_JMP:
		case SCMD_JZ:
		case SCMD_CALL:
		case SCMD_RET:
		default:
			dprintf(2, "info: %s not implemented yet\n", opcodes[*eip].mnemonic);
			{
				size_t i, l = opcodes[*eip].argcount;
				for(i = 0; i < l; i++) ++(*eip);
			}
			break;
	}
	registers[AR_NULL].i += eip_inc;
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
	size_t i;
	for(i=0; i< AR_MAX; i++)
		printf("%s: %2s %d\n", i == 0 ? "eip" : regnames[i], ru_strings[registers[i].ru], registers[i].i);

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
		vm_step();
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
		case UC_STEP: vm_step(); break;
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
	vm_init();
	printf(ADS " - type !h for help\n");
	while(fgets(buf, sizeof buf, stdin)) {
		int code[3];
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
			// we currently ignore that
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
				if(instr == SCMD_REGTOREG) {
					/* fix reversed order of arguments */
					int dst = value;
					sym = p;
					while(p < pend && *p != ',' && !isspace(*p)) p++;
					assert(p < pend);
					*p = 0;
					value=get_reg(sym);
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
							dprintf(2, "error: function refs not implemented yet\n");
							goto loop_footer;
						}
						break;
					/*
					case SCMD_JMP: case SCMD_JZ: case SCMD_JNZ:
						add_label_ref(a, sym, pos);
						break;
					*/
					default:
						value = atoi(sym);
				}
			}
			code[pos++] = value;
		}
		append_code(code, pos);
loop_footer: ;
	}
}

