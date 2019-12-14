#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

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

#define ALIGN(X, A) ((X+(A-1)) & -(A))

#define BREAKPOINT_FLAG (1<<31)
#define OPCODE_MASK (~(BREAKPOINT_FLAG))

static int interactive;

static struct mem {
	unsigned char *mem;
	size_t capa;
	size_t ltext;
	size_t lstack;
	size_t lheap;
} mem;

#define EIP registers[AR_NULL].i

#define VM_SIGILL 1
#define VM_SIGSEGV 2
#define VM_SIGABRT 3
static int vm_return;
static void vm_signal(int sig, int param) {
	switch(sig) {
		case VM_SIGILL:
			dprintf(2, "illegal instruction at IP %u\n", EIP);
			break;
		case VM_SIGSEGV:
			dprintf(2, "segmentation fault: invalid access at %u\n", EIP);
			break;
		case VM_SIGABRT:
			dprintf(2, "aborted (assertlte check failed at IP %u)\n", EIP);
			break;
		default:
			dprintf(2, "unknown signal\n");
	}
	vm_return = 1;
}

#define memory (mem.mem)
#define text memory
#define text_end ALIGN(mem.ltext, 4096)
#define stack_mem (mem.mem+text_end)
#define heap_mem (mem.mem+text_end+mem.lstack)

struct label_ref {
	char *name;
	unsigned insoff;
};
tglist(struct label_ref) *label_refs;
static void add_label_ref(char *name, unsigned insoff) {
	struct label_ref new = {.name = strdup(name), .insoff = insoff};
	tglist_add(label_refs, new);
}
static void resolve_label(char* name, unsigned insoff) {
	size_t i;
	for(i=0; i<tglist_getsize(label_refs); ) {
		struct label_ref *l = &tglist_get(label_refs, i);
		if(!strcmp(l->name, name)) {
			free(l->name);
			memcpy(text+l->insoff, &insoff, 4);
			tglist_delete(label_refs, i);
		} else ++i;
	}
}
/* label_map */
hbmap(char*, unsigned, 32) *label_map;
static unsigned *get_label_offset(char* name) {
	return hbmap_get(label_map, name);
}
static int add_label(char* name, int insoff) {
	char* tmp = strdup(name);
	return hbmap_insert(label_map, tmp, insoff) != -1;
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
} registers[MAX(AR_MAX, 256)];

static int canread(int index, int cnt) {
	return index >= 0 && index+cnt < mem.capa;
}
static int canwrite(int index, int cnt) {
	return index >= text_end && index+cnt < mem.capa;
}

#define ALIGN(X, A) ((X+(A-1)) & -(A))

static int vm_init_stack(unsigned size) {
	if(mem.lstack) return 1;
	unsigned want = ALIGN(size, 4096);
	unsigned char *p = realloc(mem.mem, mem.capa+want);
	if(!p) {
		dprintf(2, "error: could not allocate stack!\n");
		return 0;
	}
	mem.mem = p;
	mem.lstack = want;
	mem.capa += want;
	registers[AR_SP].i = text_end;
	return 1;
}

static int grow_text(size_t req) {
	/* add 4 more slots than strictly necessary so we can access
	 * at least 1 full-length insn past text end without crash */
	req += 4*sizeof(int);
	size_t need = mem.ltext + req;
	if(need > mem.capa-mem.lheap-mem.lstack) {
		if(mem.lstack) {
			dprintf(2, "error: cannot enlarge text segment once execution started!\n");
			return 0;
		}
		size_t want = ALIGN(need, 4096);
		unsigned char *p = realloc(mem.mem, want);
		if(!p) {
			dprintf(2, "error: allocating memory failed!\n");
			return 0;
		}
		mem.mem = p;
		mem.capa = want;
	}
	return 1;
}

static int append_code(int *code, size_t cnt) {
	if(!grow_text((cnt+1)*4)) return 0;
	size_t i;
	for(i = 0; i < cnt; i++) {
		memcpy(text+mem.ltext, &code[i], 4);
		mem.ltext += 4;
	}
	memcpy(text+mem.ltext, "\0\0\0\0", 4);
	return 1;
}

static void vm_reset_register_usage() {
	size_t i;
	for(i = AR_NULL + 1; i < AR_MAX; i++)
		registers[i].ru = RU_NONE;
}

static void vm_init() {
	size_t i;
	/* initialize registers to an easily recognisable junk value */
	for(i = AR_NULL + 1; i < ARRAY_SIZE(registers); i++) {
		registers[i].i = -1;
	}
	vm_reset_register_usage();
	registers[AR_SP].i = -1;
	registers[AR_NULL].i = 0;
	int was_null = text == 0;
	/* set up EIP so vm_state() doesn't crash */
	grow_text(16);
	/* put NULL insn as first instruction so VM doesn't execute
	   random garbage in mem */
	if(was_null) memcpy(text, "\0\0\0\0", 4);
}

static inline int consume_int(int **eip) {
	*eip = *eip+1;
	return **eip;
}

static void change_reg_usage(int regno, enum RegisterAccess ra) {
	if(regno >= AR_MAX) {
		vm_signal(VM_SIGSEGV, 0);
		return;
	}
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
	unsigned char *m = memory+off;
	*m = val&0xff;
}
static void write_mem2(int off, int val) {
	unsigned short *m = (void*) (memory+off);
	*m = val&0xffff;
}
static void write_mem(int off, int val) {
	int *m = (void*) (memory+off);
	*m = val;
}

static int read_mem(int off) {
	int ret;
	memcpy(&ret, memory+off, 4);
	return ret;
}

static int vm_push(int value) {
	if(!canwrite(registers[AR_SP].i, 4)) return 0;
	write_mem(registers[AR_SP].i, value);
	registers[AR_SP].i += 4;
	return 1;
}

static int vm_pop(int *value) {
	if((int) registers[AR_SP].i >= 4) {
		registers[AR_SP].i -= 4;
		*value = read_mem(registers[AR_SP].i);
		return 1;
	}
	return 0;
}

static int vm_syscall(void) {
	int ret,
	scno = registers[AR_AX].i,
	arg1 = registers[AR_BX].i,
	arg2 = registers[AR_CX].i,
	arg3 = registers[AR_DX].i;
	/* we follow linux x86_64 syscall numbers for simplicity */
	switch(scno) {
	case 0: /* SYS_read (fd, buf, size) */
		/* fall-through */
	case 1: /* SYS_write (fd, buf, size) */
		if(!canread(arg2, arg3)) return -EFAULT;
		if(scno == 0)
			ret = read(arg1, ((char*)memory)+arg2, arg3);
		else
			ret = write(arg1, ((char*)memory)+arg2, arg3);
		if(ret == -1) return -errno;
		return ret;
	case 60: /* SYS_exit (exitcode) */
		exit(arg1);
	default: return -ENOSYS;
	}
}

static int label_check() {
	if(tglist_getsize(label_refs)) {
		dprintf(2, "error: unresolved label refs!\n");
		size_t i; struct label_ref *l;
		for(i=0; i<tglist_getsize(label_refs); ++i) {
			l = &tglist_get(label_refs, i);
			dprintf(2, "%s@%u\n", l->name, l->insoff);
		}
		return 0;
	}
	return 1;
}

#define CODE_INT(X) eip[X]
#define CODE_FLOAT(X) ((float*)eip)[X]
#define REGI(X) registers[CODE_INT(X)&0xff].i
#define REGF(X) registers[CODE_INT(X)&0xff].f

static int vm_step(int run_context) {
	/* we use register AR_NULL as instruction pointer */
	int *eip = (void*)(text + EIP);
	unsigned op = *eip;
	if(interactive) {
		// breakpoints can be set only in interactive mode
		op &= OPCODE_MASK;
		if(op >= SCMD_MAX) {
			vm_signal(VM_SIGILL, 0);
			return 0;
		}

		if(*eip & BREAKPOINT_FLAG) {
			*eip &= ~BREAKPOINT_FLAG;
			return 0;
		}
		if(!run_context) vm_reset_register_usage();
		vm_update_register_usage(eip);
	} else if(op >= SCMD_MAX) {
		vm_signal(VM_SIGILL, 0);
		return 0;
	}
	int eip_inc = 1 + opcodes[op].argcount;
	int tmp, val;

	switch(op) {
		case 0:
			/* don't modify IP */
			if(!run_context)
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
			REGI(1) |= REGI(2);
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
			if(!vm_push(REGI(1))) goto oob;
			break;
		case SCMD_POPREG:
			if(!vm_pop(&REGI(1))) goto oob;
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
		case SCMD_ZEROMEMORY:
			tmp = CODE_INT(1);
			if(canwrite(registers[AR_MAR].i, tmp)) {
				memset(((char*)memory)+registers[AR_MAR].i,0,tmp);
			} else goto oob;
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
			if(canwrite(registers[AR_MAR].i, tmp)) {
				switch(tmp) {
				case 4:	write_mem (registers[AR_MAR].i, val); break;
				case 2:	write_mem2(registers[AR_MAR].i, val); break;
				case 1:	write_mem1(registers[AR_MAR].i, val); break;
				}
			} else {
		oob:
				vm_signal(VM_SIGSEGV, 0);
				return 0;
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
				int val;
				memcpy(&val, memory+registers[AR_MAR].i, 4);
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
			if((unsigned)tmp < text_end && !(tmp&3))
				registers[AR_NULL].i = tmp;
			else {
				vm_signal(VM_SIGSEGV, tmp);
				return 0;
			}
			eip_inc = 0;
			break;
		case SCMD_CALL:
			if(!vm_push(registers[AR_NULL].i + eip_inc*4)) goto oob;
			tmp = REGI(1);
			goto jump_tmp;
		case SCMD_RET:
			registers[AR_SP].i -= 4;
			tmp = read_mem(registers[AR_SP].i);
			goto jump_tmp;
		case SCMD_CALLAS:
			/* we re-purpose "callscr" mnemonic to mean syscall,
			   as it is unused in ags-emitted bytecode.
			   using it is unportable, it works only in agssim.
			   the register arg for callscr instruction is ignored.
			   the arguments are passed in regs ax,bx,cx,dx,op
			   in this order, where the first arg is the syscall
			   number. return value is put in ax. */
			registers[AR_AX].i = vm_syscall();
			break;
		case SCMD_CHECKBOUNDS:
			if(REGI(1) > CODE_INT(2)) vm_signal(VM_SIGABRT, 0);
			break;
		case SCMD_NEWARRAY:
		case SCMD_DYNAMICBOUNDS:
		case SCMD_MEMZEROPTRND:
		case SCMD_LOOPCHECKOFF:
		case SCMD_CHECKNULLREG:
		case SCMD_STRINGSNOTEQ:
		case SCMD_STRINGSEQUAL:
		case SCMD_CREATESTRING:
		case SCMD_CHECKNULL:
		case SCMD_MEMINITPTR:
		case SCMD_MEMZEROPTR:
		case SCMD_MEMREADPTR:
		case SCMD_MEMWRITEPTR:
		case SCMD_CALLOBJ:
		case SCMD_NUMFUNCARGS:
		case SCMD_SUBREALSTACK:
		case SCMD_PUSHREAL:
		case SCMD_CALLEXT:
			dprintf(2, "info: %s not implemented yet\n", opcodes[*eip].mnemonic);
			{
				size_t i, l = opcodes[*eip].argcount;
				for(i = 0; i < l; i++) ++(*eip);
			}
			break;
		default:
			vm_signal(VM_SIGILL, 0);
			return 0;
	}
	registers[AR_NULL].i += eip_inc*4;
	return 1;
}

static inline char *int_to_str(int value, char* out) {
	sprintf(out, "%d", value);
	return out;
}

static int* get_next_ip(int *eip, int off) {
	int *ret = eip, i, op;
	for(i=0; i<off; ++i) {
		op = *ret & OPCODE_MASK;
		if(op < SCMD_MAX)
			ret+=1+opcodes[op].argcount;
		else
			++ret;
	}
	return ret;
}

static const char *get_regname(unsigned regno) {
	if(regno < AR_MAX) return regnames[regno];
	return "INVALID";
}

static void vm_state() {
	if(!interactive) return;
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
	char stackview[5][24];
	stackview[2][0] = 0;
	stackview[3][0] = 0;
	stackview[4][0] = 0;
	for(j=0,i = MIN(registers[AR_SP].i+2*4, text_end+mem.lstack);
		i >= MAX(registers[AR_SP].i-2*4, text_end);
		i-=4, ++j) {
		sprintf(stackview[j],
			"SL %s %3zu %d", i == registers[AR_SP].i ? ">" : " ", i, read_mem(i));
		if(i <= 0) break;
	}
	int *eip = (void*)(text + registers[AR_NULL].i), wasnull = 0;
	for(i = 0; i<5; i++) {
		char a1b[32], a2b[32], a3b[32], inst[48];
		if(i > 1) {
			int *nip = get_next_ip(eip, i-2),
			op = *nip & OPCODE_MASK;
			if(op < SCMD_MAX) {
				const char *arg1 = opcodes[op].argcount == 0 ? "" : \
				(opcodes[op].regcount > 0 ? get_regname(nip[1]) : int_to_str(nip[1], a1b));
				const char *arg2 = opcodes[op].argcount < 2 ? "" : \
				(opcodes[op].regcount > 1 ? get_regname(nip[2]) : int_to_str(nip[2], a2b));
				const char *arg3 = opcodes[op].argcount < 3 ? "" : \
				(opcodes[op].regcount > 2 ? get_regname(nip[3]) : int_to_str(nip[2], a3b));
				if(op == SCMD_REGTOREG) {
					const char* tmp = arg1;
					arg1 = arg2; arg2 = tmp;
				}
				if(!wasnull)
					sprintf(inst, " %s %s %s %s", i==2?">":" ", opcodes[op].mnemonic, arg1, arg2);
				else inst[0] = 0;
			} else {
				sprintf(inst, "%d", *nip);
			}
			if(!op) wasnull = 1;
		} else inst[0] = 0;
		printf("%-52s %s\n", inst, stackview[i]);
	}
}

void vm_run(void) {
	if(!label_check()) return;
	while(1) {
		if(!vm_step(1)) break;
	}
}

static int usage(int fd, char *a0) {
	dprintf(fd,
		"%s [OPTIONS] [file.s] - simple ags vm simulator\n"
		"implements the ALU and a small stack\n"
		"useful to examine how a chunk of code modifies VM state\n"
		"OPTIONS:\n"
		"-i : interpreter mode - don't print anything, run and exit\n"
		"by default, mode is interactive, sporting the following commands:\n"
		"!i - reset VM state and IP\n"
		"!s - single-step\n"
		"!n - step-over\n"
		"!r - run\n"
		"!b ADDR - set a breakpoint on ADDR (address or label)\n"
	, a0);
	return 1;
}

static int lastcommand;
enum UserCommand {
	UC_STEP = 1,
	UC_NEXT, /* step-over */
	UC_BP,
	UC_RUN,
	UC_INIT,
	UC_QUIT,
	UC_HELP,
};
static void execute_user_command_i(int uc, char* param) {
	switch(uc) {
		case UC_STEP: if(label_check()) vm_step(0); break;
		case UC_BP:  {
			int addr, *ptr;
			if(isdigit(param[0]))
				addr = atoi(param);
			else {
				ptr = get_label_offset(param);
				if(!ptr) {
					dprintf(2, "label %s not found!\n", param);
					return;
				}
				addr = *ptr;
			}
			if(addr >= text_end) {
				dprintf(2, "breakpoint offset %d out of bounds\n", addr);
				return;
			}
			int insn;
			memcpy(&insn, text+addr, 4);
			insn |= BREAKPOINT_FLAG;
			memcpy(text+addr, &insn, 4);
		}
		return;
		case UC_NEXT: *get_next_ip((void*)(text+EIP), 1) |= BREAKPOINT_FLAG;
				/* fall-through */
		case UC_RUN : vm_run(); break;
		case UC_INIT: vm_init(); break;
		case UC_QUIT: exit(0); break;
		case UC_HELP: usage(1, "agssim"); break;
	}
	lastcommand = uc;
	vm_state();
}
static void execute_user_command(char *cmd) {
	if(!vm_init_stack(16384)) return;
	int uc = 0;
	char *param = cmd;
	while(!isspace(*param)) param++;
	while(isspace(*param)) param++;
	if(0) ;
	else if(!strcmp(cmd, "s")) uc = UC_STEP;
	else if(!strcmp(cmd, "r")) uc = UC_RUN;
	else if(!strcmp(cmd, "i")) uc = UC_INIT;
	else if(!strcmp(cmd, "q")) uc = UC_QUIT;
	else if(!strcmp(cmd, "h")) uc = UC_HELP;
	else if(!strcmp(cmd, "n")) uc = UC_NEXT;
	else if(*cmd == 'b') uc = UC_BP;
	else {
		dprintf(2, "unknown command\n");
		return;
	}
	execute_user_command_i(uc, param);
}

int main(int argc, char** argv) {
	int c;
	interactive = 1;
	FILE *in = stdin;
	while((c = getopt(argc, argv, "i")) != EOF) switch(c) {
	case 'i': interactive = 0; break;
	default: return usage(2, argv[0]);
	}
	if(argv[optind]) in = fopen(argv[optind], "r");
	if(!in) {
		dprintf(2, "error opening %s\n", argv[optind]);
		return 1;
	}
	char buf[1024], *sym;
	char convbuf[sizeof(buf)]; /* to convert escaped string into non-escaped version */
	int lineno = 0;
	init_labels();
	vm_init();
	if(interactive) printf(ADS " - type !h for help\n");
mainloop:
	while(fgets(buf, sizeof buf, in)) {
		int code[4];
		size_t pos = 0;
		lineno++;
		char* p = buf, *pend = buf + sizeof buf;
		if(*p == '\n' && lastcommand) {
			execute_user_command_i(lastcommand, "");
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
			resolve_label(sym, mem.ltext);
			unsigned *loff = get_label_offset(sym);
			if(loff) dprintf(2, "warning: label %s overwritten\n", sym);
			add_label(sym, mem.ltext);
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
							size_t l = strlen(sym)-1, tl = mem.ltext;
							if(!append_code((int[2]){SCMD_JMP, tl+8+ALIGN(l, 4)}, 2)) goto loop_footer;
							char*p = sym+1;
							--l;
							while((ssize_t)l >= 0) {
								int x = 0;
								memcpy(&x, p, l>=4?4:l);
								if(!append_code(&x, 1)) goto loop_footer;
								l -= 4;
								p += 4;
							}
							value = tl+8;
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
							add_label_ref(sym, mem.ltext+pos*4);
							value = -1;
						} else value = *loff;
						} break;
					default:
						if(!isdigit(sym[0])) {
							dprintf(2, "line %zu: error: expected number\n", lineno);
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
	if(!interactive) execute_user_command("r");
	else if(in != stdin) {
		in = stdin;
		goto mainloop;
	}
	return vm_return;
}

