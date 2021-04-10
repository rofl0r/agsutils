D [0-9]
L [a-z]
U [A-Z]
A [A-Za-z]
AN [A-Za-z0-9]
AU [A-Za-z_]
ANU [A-Za-z0-9_]

%{
#define YY_SKIP_YYWRAP
#define yylex asmlex

#define yywrap() (1)
void yyerror(char*);
//#define YYSTYPE long long int
//extern YYSTYPE  yylval;
#include "y.tab.h"
#include <stdlib.h>

#define BISON
#include "scmd_tok.c"
#include "regname_tok.c"

#define USE_STRDUP
#ifdef USE_STRDUP
#define STRDUP(X) strdup(X)
#else
#define STRDUP(X) (X)
#endif

#define IRET(X) do{yylval.i = X; return X;}while(0)
#define SRET(X) do{yylval.s = STRDUP(yytext); return X;}while(0)
#define NRET(X) do{yylval.i = atoi(yytext); return X;}while(0)

/* XXX */
YYSTYPE yylval;
const char *yyfilename;
int yyfatalerrors;

%}

%%
\"([^\n"\\]|\\['"?\\abfnrtv]|\\[0-7]{1,3}|\\[Xx][0-9a-fA-F]+)*\"	SRET(STRING);

[\t ]+		;

[#;].*\n	{ /* ignore comments */ goto newline; }

\n		{ newline:; ++yylineno; return 10; }

"label"{ANU}+	SRET(LABEL);

{AU}{ANU}*"$"{D}+	|
{AU}{ANU}*"::"{ANU}+"$"{D}+	{ SRET(FN_I); }

{AU}{ANU}*("::"{ANU}+)?"^"{D}+	|
{AU}{ANU}*"::"{ANU}+		{ SRET(FN_E); }

-?{D}+		{ NRET(NUMBER);}

{AU}{ANU}*	SRET(ID);

[=:@,\.\[\]]	return yytext[0];

.		{ yyerror("unknown character/token"); }

%%


static int section = 0;
static int ltok = 0;
static int last_mnemonic = 0;

#undef yylex
int yylex() {

	int ret = asmlex();
	if(ret == '\n') {
		ltok = 0;
		return ret;
	} else if (ltok==0 && ret == '.') goto set_section;
	switch(section) {
	case 0: if(ret != '.') yyerror("expected section start symbol '.'!");
set_section:
		ret = asmlex();
		if(ret != ID) yyerror("expected section name!");
		if(!strcmp(yytext, "data")) section = SECTION_DATA;
		else if(!strcmp(yytext, "text")) section = SECTION_TEXT;
		else if(!strcmp(yytext, "imports")) section = SECTION_IMPORTS;
		else if(!strcmp(yytext, "exports")) section = SECTION_EXPORTS;
		else if(!strcmp(yytext, "strings")) section = SECTION_STRINGS;
		else if(!strcmp(yytext, "sections")) section = SECTION_SECTIONS;
		else if(!strcmp(yytext, "fixups")) section = SECTION_FIXUPS;
		if(!section) yyerror("unknown section name!");
		return section;
	case SECTION_DATA:
		/* FIXME variable names here can't be one of the 4 keywords */
		if (ret == ID) {
			if(!strcmp(yytext, "export")) IRET(D_EXPORT);
			else if(!strcmp(yytext, "int")) IRET(D_INT);
			else if(!strcmp(yytext, "short")) IRET(D_SHORT);
			else if(!strcmp(yytext, "char")) IRET(D_CHAR);
		}
		break;
	case SECTION_TEXT:
		if(ret == ',') break;
		++ltok;
		if (ret == ID) {
			if (ltok == 1) {
				int tmp;
				if((tmp = KW_SCMD_find_keyword(yytext, yyleng)))
				{
					last_mnemonic = tok2scmd(tmp);
					IRET(tmp);
				}
			} else {
				const struct opcode_info *i = &opcodes[last_mnemonic];
				if(i->regcount >= ltok-1) {
					int tmp;
					if((tmp = RN_find_keyword(yytext, yyleng)))
						IRET(tmp);
				}
			}
			return ID;
		} else if (ret == LABEL) {
			/* user may haven chosen a variable name matching a label */
			if(ltok == 1 || last_mnemonic == SCMD_JMP ||
			   last_mnemonic == SCMD_JZ || last_mnemonic == SCMD_JNZ)
				return ret;
			return ID;
		}
	}
	return ret;
}

void yyerror(char *s) {
	printf("%s:%d %s at `%s`\n",
		yyfilename==0?"stdin":yyfilename,
		yylineno,
		s,
		yytext[0] == 10 ? "<newline>" : yytext);
	if(yyfatalerrors) abort();
}

void yylex_reset(FILE *f, const char *fn) {
	yyfilename = fn;
	yylineno = 1;
#ifdef YY_FLUSH_BUFFER
	YY_FLUSH_BUFFER;
#endif
	yyin = f;
	if(yytext) yytext[0] = 0;
	section = 0;
	ltok = 0;
	last_mnemonic = 0;
}

#ifdef LDEBUG
int main() {
	while(1) {
		int n = yylex();
		if(n == EOF) break;
		printf("%d\n", n);
		if(feof(yyin)) break;
	}
}
#endif
