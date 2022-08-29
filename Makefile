prefix=/usr/local
bindir=$(prefix)/bin

PROGS_SRCS = \
	agstract.c \
	agspack.c \
	agscriptxtract.c \
	agssemble.c \
	agsdisas.c \
	agssim.c \
	agsprite.c \
	agsalphahack.c \
	agsalphainfo.c \
	agsinject.c

PROGS_OBJS =  $(PROGS_SRCS:.c=.o)
CPROGS = $(PROGS_SRCS:.c=$(EXE_EXT))
PROGS = $(CPROGS) agsoptimize agsex

LIB_SRCS = \
	ags_cpu.c \
	ByteArray.c \
	Clib32.c \
	DataFile.c \
	Script.c \
	File.c \
	List.c \
	MemGrow.c \
	RoomFile.c \
	StringEscape.c

SPRITE_SRCS = \
	defpal.c \
	lzw.c \
	SpriteFile.c

SPRITE_OBJS =  $(SPRITE_SRCS:.c=.o)

ASM_SRCS = \
	Assembler.c \
	hsearch.c \
	preproc.c \
	tokenizer.c

ASM_OBJS =  $(ASM_SRCS:.c=.o)

LIB_OBJS =  $(LIB_SRCS:.c=.o)

CFLAGS_WARN = -Wall -Wextra -Wno-unknown-pragmas -Wno-sign-compare -Wno-switch -Wno-unused -Wno-pointer-sign -Wno-empty-body -Wno-type-limits

GEN_FILES = scmd_tok.h scmd_tok.c scmd_tok.shilka regname_tok.h regname_tok.c regname_tok.shilka


ifeq ($(WINBLOWS),1)
CPPFLAGS=-DNO_MMAN
CC=gcc
EXE_EXT=.exe
RM_F=del
else
RM_F=rm -f
endif

-include config.mak

ifeq ($(HOSTCC),)
HOSTCC = $(CC)
endif

ifeq ($(SHILKA),)
SHILKA = ./minishilka$(EXE_EXT)
endif

all: $(PROGS)

$(PROGS_OBJS): $(LIB_OBJS)

agssemble$(EXE_EXT): agssemble.o $(LIB_OBJS) $(ASM_OBJS)
agsprite$(EXE_EXT): agsprite.o $(LIB_OBJS) $(SPRITE_OBJS)

minishilka$(EXE_EXT): minishilka.c
	$(HOSTCC) -g3 -O0 $< -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_WARN) -o $@ -c $<

%$(EXE_EXT): %.o $(LIB_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_WARN) -o $@ $^ $(LDFLAGS)

kw_search.h: scmd_tok.h scmd_tok.c regname_tok.h regname_tok.c

Assembler.o: kw_search.h

scmd_tok.h: ags_cpu.h
	awk 'BEGIN{print("#ifndef BISON");} /#define SCMD_/{print $$1 " KW_" $$2 " (KW_TOK_SCMD_BASE + " $$3 ")";}END{print("#endif");}' < ags_cpu.h > $@

scmd_tok.shilka: ags_cpu.h
	awk 'BEGIN{printf "%%type short\n%%%%\n";}/[\t ]\[SCMD_/{w=substr($$3,3,length($$3)-4);s=length(w)>=8?"":"\t";print w s "\t{return KW_" substr($$1,2,length($$1)-2) ";}" ;}END{print "%other\t\t{return 0;}";}' < ags_cpu.h > $@

scmd_tok.c: $(SHILKA)
scmd_tok.c: scmd_tok.shilka
	$(HOSTRUN) $(SHILKA) -inline -strip -pKW_SCMD_ -no-definitions $<

regname_tok.h: ags_cpu.h
	awk '/[\t ]\[AR_/{r=substr($$1,2,length($$1)-2);printf("#define RN_%s\t(RN_TOK_BASE + %s)\n",r,r);}' < ags_cpu.h > $@

regname_tok.shilka: ags_cpu.h
	awk 'BEGIN{printf "%%type short\n%%%%\n";}/[\t ]\[AR_/{r=substr($$1,2,length($$1)-2);s=substr($$3,2,length($$3)-3);printf("%s\t{return RN_%s;}\n",s,r);}END{print("%other\t\t{return 0;}");}' < ags_cpu.h > $@

regname_tok.c: $(SHILKA)
regname_tok.c: regname_tok.shilka
	$(HOSTRUN) $(SHILKA) -inline -strip -pRN_ -no-definitions $<

lex.yy.c: scmd_tok.c regname_tok.c
lex.yy.c: asmlex.l
	$(LEX) $<

y.tab.h: y.tab.c
y.tab.c: asmparse.y
	$(YACC) -d $<

asmparse: y.tab.c lex.yy.c ags_cpu.o
	$(CC) -o $@ $^

rcb:
	make -f Makefile.binary FNAME=agstract
	make -f Makefile.binary FNAME=agspack
	make -f Makefile.binary FNAME=agscriptxtract
	make -f Makefile.binary FNAME=agssemble
	make -f Makefile.binary FNAME=agsdisas
	make -f Makefile.binary FNAME=agsinject
	make -f Makefile.binary FNAME=agssim

clean:
	$(RM_F) $(CPROGS) minishilka$(EXE_EXT) $(LIB_OBJS) $(PROGS_OBJS) $(ASM_OBJS) $(SPRITE_OBJS)
	$(RM_F) $(GEN_FILES)
	$(RM_F) *.out *.o *.rcb *.exe

install: $(PROGS:%=$(DESTDIR)$(bindir)/%)

$(DESTDIR)$(bindir)/%: %
	install -D -m 755 $< $@

.PHONY: all clean
