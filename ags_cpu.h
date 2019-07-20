#ifndef AGSCPU_H
#define AGSCPU_H

// virtual CPU commands
#define SCMD_ADD          1     // reg1 += arg2
#define SCMD_SUB          2     // reg1 -= arg2
#define SCMD_REGTOREG     3     // reg2 = reg1
#define SCMD_WRITELIT     4     // m[MAR] = arg2 (copy arg1 bytes)
#define SCMD_RET          5     // return from subroutine
#define SCMD_LITTOREG     6     // set reg1 to literal value arg2
#define SCMD_MEMREAD      7     // reg1 = m[MAR]
#define SCMD_MEMWRITE     8     // m[MAR] = reg1
#define SCMD_MULREG       9     // reg1 *= reg2
#define SCMD_DIVREG       10    // reg1 /= reg2
#define SCMD_ADDREG       11    // reg1 += reg2
#define SCMD_SUBREG       12    // reg1 -= reg2
#define SCMD_BITAND       13    // bitwise  reg1 & reg2
#define SCMD_BITOR        14    // bitwise  reg1 | reg2
#define SCMD_ISEQUAL      15    // reg1 == reg2   reg1=1 if true, =0 if not
#define SCMD_NOTEQUAL     16    // reg1 != reg2
#define SCMD_GREATER      17    // reg1 > reg2
#define SCMD_LESSTHAN     18    // reg1 < reg2
#define SCMD_GTE          19    // reg1 >= reg2
#define SCMD_LTE          20    // reg1 <= reg2
#define SCMD_AND          21    // (reg1!=0) && (reg2!=0) -> reg1
#define SCMD_OR           22    // (reg1!=0) || (reg2!=0) -> reg1
#define SCMD_CALL         23    // jump to subroutine at reg1
#define SCMD_MEMREADB     24    // reg1 = m[MAR] (1 byte)
#define SCMD_MEMREADW     25    // reg1 = m[MAR] (2 bytes)
#define SCMD_MEMWRITEB    26    // m[MAR] = reg1 (1 byte)
#define SCMD_MEMWRITEW    27    // m[MAR] = reg1 (2 bytes)
#define SCMD_JZ           28    // jump if ax==0 to arg1
#define SCMD_PUSHREG      29    // m[sp]=reg1; sp++
#define SCMD_POPREG       30    // sp--; reg1=m[sp]
#define SCMD_JMP          31    // jump to arg1
#define SCMD_MUL          32    // reg1 *= arg2
#define SCMD_CALLEXT      33    // call external (imported) function reg1
#define SCMD_PUSHREAL     34    // push reg1 onto real stack
#define SCMD_SUBREALSTACK 35
#define SCMD_LINENUM      36    // debug info - source code line number
#define SCMD_CALLAS       37    // call external script function
#define SCMD_THISBASE     38    // current relative address
#define SCMD_NUMFUNCARGS  39    // number of arguments for ext func call
#define SCMD_MODREG       40    // reg1 %= reg2
#define SCMD_XORREG       41    // reg1 ^= reg2
#define SCMD_NOTREG       42    // reg1 = !reg1
#define SCMD_SHIFTLEFT    43    // reg1 = reg1 << reg2
#define SCMD_SHIFTRIGHT   44    // reg1 = reg1 >> reg2
#define SCMD_CALLOBJ      45    // op = reg1 (set "this" argument for next function call)
#define SCMD_CHECKBOUNDS  46    // check reg1 is between 0 and arg2
#define SCMD_MEMWRITEPTR  47    // m[MAR] = reg1 (adjust ptr addr)
#define SCMD_MEMREADPTR   48    // reg1 = m[MAR] (adjust ptr addr)
#define SCMD_MEMZEROPTR   49    // m[MAR] = 0    (blank ptr)
#define SCMD_MEMINITPTR   50    // m[MAR] = reg1 (like memwrite4, but doesn't remove reference/free the old pointer)
#define SCMD_LOADSPOFFS   51    // MAR = SP - arg1 (optimization for local var access)
#define SCMD_CHECKNULL    52    // error if MAR==0
#define SCMD_FADD         53    // reg1 += arg2 (float,int)
#define SCMD_FSUB         54    // reg1 -= arg2 (float,int)
#define SCMD_FMULREG      55    // reg1 *= reg2 (float)
#define SCMD_FDIVREG      56    // reg1 /= reg2 (float)
#define SCMD_FADDREG      57    // reg1 += reg2 (float)
#define SCMD_FSUBREG      58    // reg1 -= reg2 (float)
#define SCMD_FGREATER     59    // reg1 > reg2 (float)
#define SCMD_FLESSTHAN    60    // reg1 < reg2 (float)
#define SCMD_FGTE         61    // reg1 >= reg2 (float)
#define SCMD_FLTE         62    // reg1 <= reg2 (float)
#define SCMD_ZEROMEMORY   63    // m[MAR]..m[MAR+(arg1-1)] = 0
#define SCMD_CREATESTRING 64    // reg1 = new String(reg1)
#define SCMD_STRINGSEQUAL 65    // (char*)reg1 == (char*)reg2   reg1=1 if true, =0 if not
#define SCMD_STRINGSNOTEQ 66    // (char*)reg1 != (char*)reg2
#define SCMD_CHECKNULLREG 67    // error if reg1 == NULL
#define SCMD_LOOPCHECKOFF 68    // no loop checking for this function
#define SCMD_MEMZEROPTRND 69    // m[MAR] = 0    (blank ptr, no dispose if = ax)
#define SCMD_JNZ          70    // jump to arg1 if ax!=0
#define SCMD_DYNAMICBOUNDS 71   // check reg1 is between 0 and m[MAR-4]
#define SCMD_NEWARRAY     72    // reg1 = new array of reg1 elements, each of size arg2 (arg3=managed type?)
#define SCMD_NEWUSEROBJECT 73   // reg1 = new user object of arg1 size

#define SCMD_MAX 74

struct opcode_info {
	const char* mnemonic;
	const unsigned char argcount;
	const unsigned char regcount;
};

static const struct opcode_info opcodes[] = {
	[0] = {"NULL", 0, 0},
	[SCMD_ADD] = {"addi", 2, 1},
	[SCMD_SUB] = {"subi", 2, 1},
	[SCMD_REGTOREG] = {"mr", 2, 2},
	[SCMD_WRITELIT] = {"memcpy", 2, 0},
	[SCMD_RET] = {"ret", 0, 0},
	[SCMD_LITTOREG] = {"li", 2, 1},
	[SCMD_MEMREAD] = {"memread4", 1, 1},
	[SCMD_MEMWRITE] = {"memwrite4", 1, 1},
	[SCMD_MULREG] = {"mul", 2, 2},
	[SCMD_DIVREG] = {"div", 2, 2},
	[SCMD_ADDREG] = {"add", 2, 2},
	[SCMD_SUBREG] = {"sub", 2, 2},
	[SCMD_BITAND] = {"and", 2, 2},
	[SCMD_BITOR] = {"or", 2, 2},
	[SCMD_ISEQUAL] = {"cmpeq", 2, 2},
	[SCMD_NOTEQUAL] = {"cmpne", 2, 2},
	[SCMD_GREATER] = {"gt", 2, 2},
	[SCMD_LESSTHAN] = {"lt", 2, 2},
	[SCMD_GTE] = {"gte", 2, 2},
	[SCMD_LTE] = {"lte", 2, 2},
	[SCMD_AND] = {"land", 2, 2}, /*logical*/
	[SCMD_OR] = {"lor", 2, 2},
	[SCMD_CALL] = {"call", 1, 1},
	[SCMD_MEMREADB] = {"memread1", 1, 1},
	[SCMD_MEMREADW] = {"memread2", 1, 1},
	[SCMD_MEMWRITEB] = {"memwrite1", 1, 1},
	[SCMD_MEMWRITEW] = {"memwrite2", 1, 1},
	[SCMD_JZ] = {"jzi", 1, 0},
	[SCMD_PUSHREG] = {"push", 1, 1},
	[SCMD_POPREG] = {"pop", 1, 1},
	[SCMD_JMP] = {"jmpi", 1, 0},
	[SCMD_MUL] = {"muli", 2, 1},
	[SCMD_CALLEXT] = {"farcall", 1, 1},
	[SCMD_PUSHREAL] = {"farpush", 1, 1},
	[SCMD_SUBREALSTACK] = {"farsubsp", 1, 0},
	[SCMD_LINENUM] = {"sourceline", 1, 0},
	[SCMD_CALLAS] = {"callscr", 1, 1},
	[SCMD_THISBASE] = {"thisaddr", 1, 0},
	[SCMD_NUMFUNCARGS] = {"setfuncargs", 1, 0},
	[SCMD_MODREG] = {"mod", 2, 2},
	[SCMD_XORREG] = {"xor", 2, 2},
	[SCMD_NOTREG] = {"not", 1, 1},
	[SCMD_SHIFTLEFT] = {"shl", 2, 2},
	[SCMD_SHIFTRIGHT] = {"shr", 2, 2},
	[SCMD_CALLOBJ] = {"callobj", 1, 1},
	[SCMD_CHECKBOUNDS] = {"assertlte", 2, 1},
	[SCMD_MEMWRITEPTR] = {"ptrset", 1, 1},
	[SCMD_MEMREADPTR] = {"ptrget", 1, 1},
	[SCMD_MEMZEROPTR] = {"ptrzero", 0, 0},
	[SCMD_MEMINITPTR] = {"ptrinit", 1, 1},
	[SCMD_LOADSPOFFS] = {"ptrstack", 1, 0},
	[SCMD_CHECKNULL] = {"ptrassert", 0, 0},
	[SCMD_FADD] = {"faddi", 2, 1},
	[SCMD_FSUB] = {"fsubi", 2, 1},
	[SCMD_FMULREG] = {"fmul", 2, 2},
	[SCMD_FDIVREG] = {"fdiv", 2, 2},
	[SCMD_FADDREG] = {"fadd", 2, 2},
	[SCMD_FSUBREG] = {"fsub", 2, 2},
	[SCMD_FGREATER] = {"fgt", 2, 2},
	[SCMD_FLESSTHAN] = {"flt", 2, 2},
	[SCMD_FGTE] = {"fgte", 2, 2},
	[SCMD_FLTE] = {"flte", 2, 2},
	[SCMD_ZEROMEMORY] = {"zeromem", 1, 0},
	[SCMD_CREATESTRING] = {"newstr", 1, 1},
	[SCMD_STRINGSEQUAL] = {"streq", 2, 2},
	[SCMD_STRINGSNOTEQ] = {"strne", 2, 2},
	[SCMD_CHECKNULLREG] = {"assert", 1, 1},
	[SCMD_LOOPCHECKOFF] = {"loopcheckoff", 0, 0},
	[SCMD_MEMZEROPTRND] = {"ptrzerond", 0, 0},
	[SCMD_JNZ] = {"jnzi", 1, 0},
	[SCMD_DYNAMICBOUNDS] = {"dynamicbounds", 1, 1},
	[SCMD_NEWARRAY] = {"newarr", 3, 1},
	[SCMD_NEWUSEROBJECT] = {"newuserobject", 2, 1},
};

/* these are called e.g. SREG_OP in upstream */
enum ags_reg {
	AR_NULL = 0,
	AR_SP,  /* stack ptr */
	AR_MAR, /* memory address register, i.e. holding pointer for mem* funcs */
	AR_AX,  /* 4 GPRs */
	AR_BX,
	AR_CX,
	AR_OP,  /* object pointer for member func calls, i.e. "this".
		   ags engine only sets it via SCMD_CALLOBJ, otherwise, it is only ever pushed/popped */
	AR_DX,
	AR_MAX
};

static const char *regnames[AR_MAX] = { 
	[AR_NULL] = "null",
	[AR_SP] = "sp",
	[AR_MAR] = "mar",
	[AR_AX] = "ax",
	[AR_BX] = "bx",
	[AR_CX] = "cx",
	[AR_OP] = "op",
	[AR_DX] = "dx",
};


#endif
