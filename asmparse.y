%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
extern int yylex();
extern int yylineno;
extern void yyerror(char*);
int yydebug = 1;
extern const char *yyfilename;
extern int yyfatalerrors;
void yylex_reset(FILE *f, const char *fn);
%}

%code requires {
#include "ags_cpu.h"
#include "tglist.h"

struct variable {
	int type;
	int elems;
	int value;
	int export;
	char* name;
};

struct instruction {
	int tok[4];
	char *strdata;
};

struct basicblock {
	int isfunc;
	char *label;
	tglist(struct instruction) *insns;
};

tglist(struct variable) *vars;
tglist(struct basicblock) *blocks;
#ifndef complain
#define complain(fmt, args...) do { fprintf(stderr, "%d: " fmt, yylineno, ## args); exit(1); } while(0)
#endif

#define tok2scmd(tok) ((tok - KW_SCMD_ADD) + SCMD_ADD)
#define tok2reg(tok) ((tok - RN_AR_SP) + AR_SP)

}

%union {
	int i;
	char *s;
}

%code {
static void opcheck(int tok, int nargs, int nregs) {
	const struct opcode_info *i = &opcodes[tok2scmd(tok)];
	if(i->argcount != nargs)
		complain("mnemonic '%s' requires %d args!\n", i->mnemonic, i->argcount);
	if(i->regcount != nregs)
		complain("mnemonic '%s' requires %d register args!\n", i->mnemonic, i->regcount);
}

static void add_block(char *name, int isfunc) {
	struct basicblock b = {
		.isfunc = isfunc,
		.label = name,
		.insns = tglist_new(),
	};
	tglist_add(blocks, b);
}

#define INS(A, B, C, D, E) add_insn((int[]){A, B, C, D}, E)
static void add_insn(int toks[4], char *strdata) {
	struct instruction i = {
		.tok = {toks[0], toks[1], toks[2], toks[3]},
		.strdata = strdata,
	};
	size_t l = tglist_getsize(blocks);
	if(l) --l;
	tglist_add(tglist_get(blocks, l).insns, i);
}

static void add_var(char *id, int type, int nelems, int value, int export)
{
	struct variable v = {
		.name = strdup(id),
		.elems = nelems,
		.value = value,
		.type = type,
		.export = export,
	};
	tglist_add(vars, v);
}
}

%token <i> NUMBER
%token <s> ID STRING
%token <s> FN_I FN_E LABEL

%token <i> SECTION_DATA SECTION_TEXT SECTION_STRINGS SECTION_FIXUPS
%token <i> SECTION_IMPORTS SECTION_EXPORTS SECTION_SECTIONS

%token <i> D_EXPORT D_INT D_SHORT D_CHAR

%token <i> KW_SCMD_ADD KW_SCMD_SUB KW_SCMD_REGTOREG KW_SCMD_WRITELIT KW_SCMD_RET
%token <i> KW_SCMD_LITTOREG KW_SCMD_MEMREAD KW_SCMD_MEMWRITE KW_SCMD_MULREG
%token <i> KW_SCMD_DIVREG KW_SCMD_ADDREG KW_SCMD_SUBREG KW_SCMD_BITAND
%token <i> KW_SCMD_BITOR KW_SCMD_ISEQUAL KW_SCMD_NOTEQUAL KW_SCMD_GREATER
%token <i> KW_SCMD_LESSTHAN KW_SCMD_GTE KW_SCMD_LTE KW_SCMD_AND KW_SCMD_OR
%token <i> KW_SCMD_CALL KW_SCMD_MEMREADB KW_SCMD_MEMREADW KW_SCMD_MEMWRITEB
%token <i> KW_SCMD_MEMWRITEW KW_SCMD_JZ KW_SCMD_PUSHREG KW_SCMD_POPREG KW_SCMD_JMP
%token <i> KW_SCMD_MUL KW_SCMD_CALLEXT KW_SCMD_PUSHREAL KW_SCMD_SUBREALSTACK
%token <i> KW_SCMD_LINENUM KW_SCMD_CALLAS KW_SCMD_THISBASE KW_SCMD_NUMFUNCARGS
%token <i> KW_SCMD_MODREG KW_SCMD_XORREG KW_SCMD_NOTREG KW_SCMD_SHIFTLEFT
%token <i> KW_SCMD_SHIFTRIGHT KW_SCMD_CALLOBJ KW_SCMD_CHECKBOUNDS
%token <i> KW_SCMD_MEMWRITEPTR KW_SCMD_MEMREADPTR KW_SCMD_MEMZEROPTR
%token <i> KW_SCMD_MEMINITPTR KW_SCMD_LOADSPOFFS KW_SCMD_CHECKNULL KW_SCMD_FADD
%token <i> KW_SCMD_FSUB KW_SCMD_FMULREG KW_SCMD_FDIVREG KW_SCMD_FADDREG
%token <i> KW_SCMD_FSUBREG KW_SCMD_FGREATER KW_SCMD_FLESSTHAN KW_SCMD_FGTE
%token <i> KW_SCMD_FLTE KW_SCMD_ZEROMEMORY KW_SCMD_CREATESTRING KW_SCMD_STRINGSEQUAL
%token <i> KW_SCMD_STRINGSNOTEQ KW_SCMD_CHECKNULLREG KW_SCMD_LOOPCHECKOFF
%token <i> KW_SCMD_MEMZEROPTRND KW_SCMD_JNZ KW_SCMD_DYNAMICBOUNDS KW_SCMD_NEWARRAY
%token <i> KW_SCMD_NEWUSEROBJECT

%token <i> RN_AR_NULL RN_AR_SP RN_AR_MAR RN_AR_AX RN_AR_BX RN_AR_CX RN_AR_OP RN_AR_DX
/* these are only used in the parser */
%token <i> FIXUP_LOCAL FIXUP_GLOBAL

%type <i> mnemonic register type export exportornot

%%

module: nlsornot dataornot textornot stringsecornot fixupsecornot importsecornot exportsecornot sectionsecornot

fixupsecornot: fixupsec
 |
 ;
fixupsec: SECTION_FIXUPS nls fixupsornot
fixupsornot: fixups
 |
 ;
fixups: fixup
 | fixups fixup
 ;
fixup: ID ':' NUMBER nls

sectionsecornot: sectionsec
 |
 ;
sectionsec: SECTION_SECTIONS nls sectionsornot
sectionsornot: sections
 |
 ;
sections: section
 | sections section
 ;
section: STRING '=' NUMBER nls

exportsecornot: exportsec
 |
 ;
exportsec: SECTION_EXPORTS nls exportsornot

exportsornot: exports
 |
 ;
exports: export
 | exports export
 ;
export: NUMBER STRING '\n' NUMBER ':' NUMBER nls

importsecornot: importsec
 |
 ;
importsec: SECTION_IMPORTS nls importsornot
importsornot: imports
 |
 ;
imports: import
 | imports import
 ;
import: NUMBER STRING nls

stringsecornot: stringsec
 |
 ;
stringsec: SECTION_STRINGS nls stringsornot
stringsornot: strings
 |
 ;
strings: string
 | strings string
 ;
string: STRING nls

dataornot: data
 |
 ;

data: SECTION_DATA nls vardeclsornot

vardeclsornot: vardecls
 |
 ;

vardecls: vardecl
 | vardecls vardecl

vardecl: exportornot type ID nls			{ add_var($3, $2, 1, 0, $1); }
 | exportornot type ID '=' NUMBER nls			{ add_var($3, $2, 1, $5, $1); }
 | exportornot type '[' NUMBER ']' ID nls		{ add_var($6, $2, $4, 0, $1); }
 | exportornot type '[' NUMBER ']' ID '=' NUMBER nls	{ add_var($6, $2, $4, $8, $1); }
 ;

type : D_INT
 | D_SHORT
 | D_CHAR
 ;

exportornot: export			{ $$ = 1; }
 |					{ $$ = 0; }
 ;

export: D_EXPORT;

textornot: text
 |
 ;

text: SECTION_TEXT nls funcs

nlsornot: nls
 |
 ;

funcs: func
 | funcs func

func: fndecl asmstats

nls: '\n'
 | nls '\n'

fndecl: FN_I ':' nls			{ add_block($1, 1); }

asmstats: asmstat
 | asmstats asmstat

asmstat: mnemonic nls			{ opcheck($1, 0, 0); INS($1, 0, 0, 0, 0);}
 | mnemonic register nls		{ opcheck($1, 1, 1); INS($1, $2, 0, 0, 0);}
 | mnemonic LABEL nls			{ opcheck($1, 1, 0); INS($1, LABEL, 0, 0, $2);}
 | mnemonic NUMBER nls			{ opcheck($1, 1, 0); INS($1, $2, 0, 0, 0);}
 | mnemonic NUMBER ',' NUMBER nls	{ opcheck($1, 2, 0); INS($1, $2, $4, 0, 0);}
 | mnemonic register ',' register nls	{ opcheck($1, 2, 2); INS($1, $2, $4, 0, 0);}
 | mnemonic register ',' NUMBER nls	{ opcheck($1, 2, 1); INS($1, $2, $4, 0, 0);}
 | mnemonic register ',' STRING nls	{ opcheck($1, 2, 1); INS($1, $2, STRING, 0, $4);}
 | mnemonic register ',' FN_I nls	{ opcheck($1, 2, 1); INS($1, $2, FN_I, 0, $4);}
 | mnemonic register ',' FN_E nls	{ opcheck($1, 2, 1); INS($1, $2, FN_E, 0, $4);}
 | mnemonic register ',' '@' ID nls	{ opcheck($1, 2, 1); INS($1, $2, FIXUP_LOCAL, 0, $5);}
 | mnemonic register ',' ID nls		{ opcheck($1, 2, 1); INS($1, $2, FIXUP_GLOBAL, 0, $4);}
 | mnemonic register ',' NUMBER ',' NUMBER nls { opcheck($1, 3, 1); INS($1, $2, $4, $6, 0);} /* the odd-ball newarr insn */
 | LABEL ':' nls			{ add_block($1, 0); }
 ;

register:
   RN_AR_SP		{ $$ = $1; }
 | RN_AR_MAR		{ $$ = $1; }
 | RN_AR_OP		{ $$ = $1; }
 | RN_AR_AX		{ $$ = $1; }
 | RN_AR_BX		{ $$ = $1; }
 | RN_AR_CX		{ $$ = $1; }
 | RN_AR_DX		{ $$ = $1; }
 ;

mnemonic:
   KW_SCMD_ADD		{ $$ = $1; }
 | KW_SCMD_SUB		{ $$ = $1; }
 | KW_SCMD_REGTOREG	{ $$ = $1; }
 | KW_SCMD_WRITELIT	{ $$ = $1; }
 | KW_SCMD_RET		{ $$ = $1; }
 | KW_SCMD_LITTOREG	{ $$ = $1; }
 | KW_SCMD_MEMREAD	{ $$ = $1; }
 | KW_SCMD_MEMWRITE	{ $$ = $1; }
 | KW_SCMD_MULREG	{ $$ = $1; }
 | KW_SCMD_DIVREG	{ $$ = $1; }
 | KW_SCMD_ADDREG	{ $$ = $1; }
 | KW_SCMD_SUBREG	{ $$ = $1; }
 | KW_SCMD_BITAND	{ $$ = $1; }
 | KW_SCMD_BITOR	{ $$ = $1; }
 | KW_SCMD_ISEQUAL	{ $$ = $1; }
 | KW_SCMD_NOTEQUAL	{ $$ = $1; }
 | KW_SCMD_GREATER	{ $$ = $1; }
 | KW_SCMD_LESSTHAN	{ $$ = $1; }
 | KW_SCMD_GTE		{ $$ = $1; }
 | KW_SCMD_LTE		{ $$ = $1; }
 | KW_SCMD_AND		{ $$ = $1; }
 | KW_SCMD_OR		{ $$ = $1; }
 | KW_SCMD_CALL		{ $$ = $1; }
 | KW_SCMD_MEMREADB	{ $$ = $1; }
 | KW_SCMD_MEMREADW	{ $$ = $1; }
 | KW_SCMD_MEMWRITEB	{ $$ = $1; }
 | KW_SCMD_MEMWRITEW	{ $$ = $1; }
 | KW_SCMD_JZ		{ $$ = $1; }
 | KW_SCMD_PUSHREG	{ $$ = $1; }
 | KW_SCMD_POPREG	{ $$ = $1; }
 | KW_SCMD_JMP		{ $$ = $1; }
 | KW_SCMD_MUL		{ $$ = $1; }
 | KW_SCMD_CALLEXT	{ $$ = $1; }
 | KW_SCMD_PUSHREAL	{ $$ = $1; }
 | KW_SCMD_SUBREALSTACK	{ $$ = $1; }
 | KW_SCMD_LINENUM	{ $$ = $1; }
 | KW_SCMD_CALLAS	{ $$ = $1; }
 | KW_SCMD_THISBASE	{ $$ = $1; }
 | KW_SCMD_NUMFUNCARGS	{ $$ = $1; }
 | KW_SCMD_MODREG	{ $$ = $1; }
 | KW_SCMD_XORREG	{ $$ = $1; }
 | KW_SCMD_NOTREG	{ $$ = $1; }
 | KW_SCMD_SHIFTLEFT	{ $$ = $1; }
 | KW_SCMD_SHIFTRIGHT	{ $$ = $1; }
 | KW_SCMD_CALLOBJ	{ $$ = $1; }
 | KW_SCMD_CHECKBOUNDS	{ $$ = $1; }
 | KW_SCMD_MEMWRITEPTR	{ $$ = $1; }
 | KW_SCMD_MEMREADPTR	{ $$ = $1; }
 | KW_SCMD_MEMZEROPTR	{ $$ = $1; }
 | KW_SCMD_MEMINITPTR	{ $$ = $1; }
 | KW_SCMD_LOADSPOFFS	{ $$ = $1; }
 | KW_SCMD_CHECKNULL	{ $$ = $1; }
 | KW_SCMD_FADD		{ $$ = $1; }
 | KW_SCMD_FSUB		{ $$ = $1; }
 | KW_SCMD_FMULREG	{ $$ = $1; }
 | KW_SCMD_FDIVREG	{ $$ = $1; }
 | KW_SCMD_FADDREG	{ $$ = $1; }
 | KW_SCMD_FSUBREG	{ $$ = $1; }
 | KW_SCMD_FGREATER	{ $$ = $1; }
 | KW_SCMD_FLESSTHAN	{ $$ = $1; }
 | KW_SCMD_FGTE		{ $$ = $1; }
 | KW_SCMD_FLTE		{ $$ = $1; }
 | KW_SCMD_ZEROMEMORY	{ $$ = $1; }
 | KW_SCMD_CREATESTRING	{ $$ = $1; }
 | KW_SCMD_STRINGSEQUAL	{ $$ = $1; }
 | KW_SCMD_STRINGSNOTEQ	{ $$ = $1; }
 | KW_SCMD_CHECKNULLREG	{ $$ = $1; }
 | KW_SCMD_LOOPCHECKOFF	{ $$ = $1; }
 | KW_SCMD_MEMZEROPTRND	{ $$ = $1; }
 | KW_SCMD_JNZ		{ $$ = $1; }
 | KW_SCMD_DYNAMICBOUNDS	{ $$ = $1; }
 | KW_SCMD_NEWARRAY		{ $$ = $1; }
 | KW_SCMD_NEWUSEROBJECT	{ $$ = $1; }
 ;


%%

/*
statement:
	expr			{ solution = yyval; }

expr:	expr '+' expr		{ $$ = $1 + $3; }
 |	expr '-' expr		{ $$ = $1 - $3; }
 |	expr '*' expr		{ $$ = $1 * $3; }
 |	LPAREN expr RPAREN	{ $$ = $2; }
 |	'-' expr %prec UMINUS	{ $$ = 0 - $2; }
 |	NUMBER			
 ;

*/

static void print_insn(struct instruction *i) {
	if(i->tok[0] == 0) return;
	int op = tok2scmd(i->tok[0]);
	const struct opcode_info *oi = &opcodes[op];
	printf("\t%s ", oi->mnemonic);
	switch(oi->argcount) {
	case 0: goto end;
	case 1:
		if(i->strdata && i->tok[1] == LABEL)
			printf("%s", i->strdata);
		else if(oi->regcount == 1)
			printf("%s", regnames[tok2reg(i->tok[1])]);
		else printf("%d", i->tok[1]);
		break;
	case 2:
		if(oi->regcount == 2)
			printf("%s, %s", regnames[tok2reg(i->tok[1])], regnames[tok2reg(i->tok[2])]);
		else if(oi->regcount == 0)
			printf("%d, %d", i->tok[1], i->tok[2]);
		else if(oi->regcount == 1) {
			printf("%s, ", regnames[tok2reg(i->tok[1])]);
			if(!i->strdata)
				printf("%d", i->tok[2]);
			else switch(i->tok[2]) {
			case FIXUP_LOCAL:
				printf("@");
				/* fall through */
			case STRING:
			case FN_I:
			case FN_E:
			case FIXUP_GLOBAL:
				printf("%s", i->strdata);
				break;
			default: assert(0);
			}
		}
		break;
	case 3:
		printf("%s, %d, %d", regnames[tok2reg(i->tok[1])], i->tok[2], i->tok[3]);
		break;
	default: assert(0);
	}
end:
	printf("\n");
};

static int print_insns() {
	int count = 0;
	size_t i, nb = tglist_getsize(blocks);
	for(i = 0; i < nb; ++i) {
		struct basicblock *b = &tglist_get(blocks, i);
		printf("%s:\n", b->label);
		size_t j, ni = tglist_getsize(b->insns);
		for(j = 0; j < ni; ++j, ++count) {
			struct instruction *ins = &tglist_get(b->insns, j);
			print_insn(ins);
		}
	}
	return count;
}
static int opt_pushpop() {
	int count = 0;
	size_t i, nb = tglist_getsize(blocks);
	for(i = 0; i < nb; ++i) {
		struct basicblock *b = &tglist_get(blocks, i);
		size_t j, ni = tglist_getsize(b->insns);
		if(ni < 2) continue;
		for(j = 0; j < ni-1; ++j) {
			struct instruction *ins1 = &tglist_get(b->insns, j);
			struct instruction *ins2 = &tglist_get(b->insns, j+1);
			if(ins1->tok[0] == KW_SCMD_PUSHREG
			&& ins2->tok[0] == KW_SCMD_POPREG) {
				if(ins1->tok[1] == ins2->tok[1]) {
					/* push ax; pop ax -> */
					ins1->tok[0] = 0, ins2->tok[0] = 0;
					count += 2;
				} else {
					/* push ax; pop bx -> mr bx, ax */
					ins2->tok[0] = KW_SCMD_REGTOREG;
					ins2->tok[2] = ins1->tok[1];
					ins1->tok[0] = 0;
					++count;
					if(j+2<ni) {
						struct instruction *ins3 = &tglist_get(b->insns, j+2);
						if(ins3->tok[0] == KW_SCMD_REGTOREG
						&& ins3->tok[1] == ins2->tok[2]
						&& ins3->tok[2] == ins2->tok[1]) {
					/* push ax; pop mar; mr ax, mar -> mr mar, ax */
							ins3->tok[0] = 0;
							++count;
						}
					}
				}
			}
		}
	}
	return count;
}
static int opt_pushlipop() {
	int count = 0;
	size_t i, nb = tglist_getsize(blocks);
	for(i = 0; i < nb; ++i) {
		struct basicblock *b = &tglist_get(blocks, i);
		size_t j, ni = tglist_getsize(b->insns);
		if(ni < 3) continue;
		for(j = 0; j < ni-2; ++j) {
			struct instruction *ins1 = &tglist_get(b->insns, j);
			struct instruction *ins2 = &tglist_get(b->insns, j+1);
			struct instruction *ins3 = &tglist_get(b->insns, j+2);
			if(ins1->tok[0] == KW_SCMD_PUSHREG
			&& ins2->tok[0] == KW_SCMD_LITTOREG
			&& ins3->tok[0] == KW_SCMD_POPREG) {
				if(ins1->tok[1] == ins3->tok[1]
				&& ins1->tok[1] != ins2->tok[1]) {
				/* push ax; li bx, 1; pop ax -> li bx, 1 */
					ins1->tok[0] = 0, ins3->tok[0] = 0;
					count += 2;
				} else if(ins1->tok[1] == ins2->tok[1]) {
				/* this one is called "ll - load literal in py optimizer */
				/* push ax; li ax, 1; pop bx -> mr bx, ax; li ax, 1 */
					ins1->tok[0] = KW_SCMD_REGTOREG;
					ins1->tok[1] = ins3->tok[1];
					ins1->tok[2] = ins2->tok[1];
					ins3->tok[0] = 0;
					++count;
				}
			}
		}
	}
	return count;
}
#include "regusage.h"
static int is_reg_overwritten(int reg, struct basicblock *b, size_t first, size_t count)
{
	size_t i, j;
	for(i=first; i<count; ++i) {
		struct instruction *ins = &tglist_get(b->insns, i);
		int op = tok2scmd(ins->tok[0]);
		if(op == SCMD_JMP) break; /* we can't see what happens past uncond. jump */
		/* if the func returns we don't need to save the reg, so we can classify
		   it as being discarded/overwritten */
		else if(op == SCMD_RET) return 1;
		const struct opcode_info *oi = &opcodes[op];
		for(j=0;j<oi->regcount;++j) if(tok2reg(ins->tok[1+j]) == reg) {
			const struct regaccess_info *ri = &regaccess_info[op];
			unsigned char rub[2], *ru = &ri->ra_reg1;
			if(op == SCMD_REGTOREG) {
				/* since the tokens in our block represent the textual
				   representation, but the regusage data binary, we need
				   to swap the values here, as 'mr' is the oddball
				   instruction with switched registers. */
				rub[0] = ru[1];
				rub[1] = ru[0];
				ru = rub;
			}
			switch(ru[j]) {
			case RA_READ: case RA_READWRITE:
				return 0;
			case RA_WRITE:
				return 1;
			}
		}
	}
	return 0;
}

static int opt_loadnegfloat() {
	int count = 0;
	size_t i, nb = tglist_getsize(blocks);
	for(i = 0; i < nb; ++i) {
		struct basicblock *b = &tglist_get(blocks, i);
		size_t j, ni = tglist_getsize(b->insns);
		if(ni < 4) continue;
		for(j = 0; j < ni-3; ++j) {
			struct instruction *ins1 = &tglist_get(b->insns, j);
			struct instruction *ins2 = &tglist_get(b->insns, j+1);
			struct instruction *ins3 = &tglist_get(b->insns, j+2);
			struct instruction *ins4 = &tglist_get(b->insns, j+3);
			if(ins1->tok[0] == KW_SCMD_LITTOREG
			&& ins2->tok[0] == KW_SCMD_LITTOREG
			&& ins3->tok[0] == KW_SCMD_FSUBREG
			&& ins4->tok[0] == KW_SCMD_REGTOREG
			&& ins2->tok[2] == 0
			&& ins3->tok[1] == ins2->tok[1]
			&& ins3->tok[2] == ins1->tok[1]
			&& ins4->tok[1] == ins1->tok[1]
			&& ins4->tok[2] == ins2->tok[1]
			) {
				float f;
				memcpy(&f, &ins1->tok[2], 4);
				f = 0 - f;
				memcpy(&ins1->tok[2], &f, 4);
				/* scan rest of block whether reg of ins2 is later discarded */
				if(is_reg_overwritten(tok2reg(ins2->tok[1]), b, j+4, ni)) {
					ins2->tok[0] = 0;
					++count;
				} else
					memcpy(&ins2->tok[2], &f, 4);
				ins3->tok[0] = 0;
				ins4->tok[0] = 0;
				count += 2;
			}
		}
	}
	return count;
}
static int opt_loadnegint() {
	int count = 0;
	size_t i, nb = tglist_getsize(blocks);
	for(i = 0; i < nb; ++i) {
		struct basicblock *b = &tglist_get(blocks, i);
		size_t j, ni = tglist_getsize(b->insns);
		if(ni < 4) continue;
		for(j = 0; j < ni-3; ++j) {
			struct instruction *ins1 = &tglist_get(b->insns, j);
			struct instruction *ins2 = &tglist_get(b->insns, j+1);
			struct instruction *ins3 = &tglist_get(b->insns, j+2);
			struct instruction *ins4 = &tglist_get(b->insns, j+3);
			if(ins1->tok[0] == KW_SCMD_LITTOREG
			&& ins2->tok[0] == KW_SCMD_LITTOREG
			&& ins3->tok[0] == KW_SCMD_SUBREG
			&& ins4->tok[0] == KW_SCMD_REGTOREG
			&& ins2->tok[2] == 0
			&& ins3->tok[1] == ins2->tok[1]
			&& ins3->tok[2] == ins1->tok[1]
			&& ins4->tok[1] == ins1->tok[1]
			&& ins4->tok[2] == ins2->tok[1]
			) {
				int x = -ins1->tok[2];
				ins1->tok[2] = x;
				/* scan rest of block whether reg of ins2 is later discarded */
				if(is_reg_overwritten(tok2reg(ins2->tok[1]), b, j+4, ni)) {
					ins2->tok[0] = 0;
					++count;
				} else
					ins2->tok[2] = x;
				ins3->tok[0] = 0;
				ins4->tok[0] = 0;
				count += 2;
			}
		}
	}
	return count;
}

static void discard_removed_insns() {
	int count = 0;
	size_t i, j, nb = tglist_getsize(blocks);
	for(i = 0; i < nb; ++i) {
		struct basicblock *b = &tglist_get(blocks, i);
		for(j = 0; j < tglist_getsize(b->insns); ) {
			struct instruction *ins = &tglist_get(b->insns, j);
			if(ins->tok[0] == 0) tglist_delete(b->insns, j);
			else ++j;
		}
	}
}

static void optimize() {
	dprintf(2, "loadnegfloat: removed %d insns\n", opt_loadnegfloat());
	dprintf(2, "loadnegint: removed %d insns\n", opt_loadnegint());
	discard_removed_insns();
	dprintf(2, "pushpop: removed %d insns\n", opt_pushpop());
	discard_removed_insns();
	dprintf(2, "pushlipop: removed %d insns\n", opt_pushlipop());
}

static int parse_one(char *fn) {
	vars = tglist_new();
	blocks = tglist_new();
	int ret = yyparse();
	printf("%s: %s\n", fn, (const char*[]){"FAIL", "OK"}[ret==0]);
	optimize();
	print_insns();
	tglist_free(vars);
	tglist_free(blocks);
	return ret;
}

int main(int argc, char **argv) {
#if YYDEBUG
	yydebug = 1;
#endif
	yyfatalerrors = 0;
	int errs = 0;
	if(argc == 1) return parse_one("stdin");
	else while(*(++argv)) {
		FILE* f;
		f = fopen(*argv, "r");
		if(!f) {
			complain("error opening %s!\n", *argv);
			continue;
		}
		yylex_reset(f, *argv);
		if(parse_one(*argv)) errs++;
		fclose(f);
	}
	return !!errs;
}
