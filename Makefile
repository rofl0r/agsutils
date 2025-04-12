# suppress built-in rules for % (file without extension, our PROGS on non-win)
.SUFFIXES:

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
	miniz_tinfl.c \
	SpriteFile.c

CPP_SRCS = \
	preproc.c \
	tokenizer.c

ASM_SRCS = \
	hsearch.c \
	Assembler.c

CFLAGS_WARN = -Wall -Wextra -Wno-unknown-pragmas -Wno-sign-compare -Wno-switch -Wno-unused -Wno-pointer-sign -Wno-empty-body -Wno-type-limits

GEN_FILES = scmd_tok.h scmd_tok.c scmd_tok.shilka regname_tok.h regname_tok.c regname_tok.shilka

-include config.mak

TOOLCHAIN := $(shell $(CC) -dumpmachine || echo 'unknown')

# mingw doesn't support fmemopen, open_memstream etc used in preprocessor
ifneq ($(findstring mingw,$(TOOLCHAIN)),mingw)
ASM_SRCS += $(CPP_SRCS)
endif

ifeq ($(findstring mingw,$(TOOLCHAIN)),mingw)
WIN=1
CPPFLAGS += -D__USE_MINGW_ANSI_STDIO=1 -D_FILE_OFFSET_BITS=64
ASM_CPPFLAGS = -DDISABLE_CPP
endif
ifeq ($(findstring cygwin,$(TOOLCHAIN)),cygwin)
WIN=1
endif

# set this if you're on a windows shell - i.e. neither msys nor cygwin
ifeq ($(WINBLOWS),1)
WIN=1
RM_F=del
else
RM_F=rm -f
endif

OBJ_EXT=.o

ifdef WIN
EXE_EXT=.exe
OBJ_EXT=.obj
endif

ASM_OBJS =  $(ASM_SRCS:.c=$(OBJ_EXT))
LIB_OBJS =  $(LIB_SRCS:.c=$(OBJ_EXT))
PROGS_OBJS =  $(PROGS_SRCS:.c=$(OBJ_EXT))
SPRITE_OBJS =  $(SPRITE_SRCS:.c=$(OBJ_EXT))

CPROGS = $(PROGS_SRCS:.c=$(EXE_EXT))
PROGS = $(CPROGS) agsoptimize agsex

ifeq ($(HOSTCC),)
HOSTCC = $(CC)
endif

ifeq ($(SHILKA),)
SHILKA = ./minishilka$(EXE_EXT)
endif

all: $(PROGS)

.SECONDARY: $(PROGS_OBJS)

Debug:
	$(MAKE) CFLAGS="-g3 -O0" all

agssemble$(EXE_EXT): agssemble$(OBJ_EXT) $(LIB_OBJS) $(ASM_OBJS)
agsprite$(EXE_EXT): agsprite$(OBJ_EXT) $(LIB_OBJS) $(SPRITE_OBJS)
%$(EXE_EXT): %$(OBJ_EXT) $(LIB_OBJS)

minishilka$(EXE_EXT): minishilka.c
	$(HOSTCC) -g3 -O0 $< -o $@

%$(OBJ_EXT): %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_WARN) -o $@ -c $<

%$(EXE_EXT): %$(OBJ_EXT) $(LIB_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_WARN) -o $@ $^ $(LDFLAGS)

agssemble$(OBJ_EXT): CPPFLAGS += $(ASM_CPPFLAGS)

kw_search.h: scmd_tok.h scmd_tok.c regname_tok.h regname_tok.c

Assembler$(OBJ_EXT): kw_search.h

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

asmparse: y.tab.c lex.yy.c ags_cpu$(OBJ_EXT)
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
	$(RM_F) $(CPROGS) minishilka$(EXE_EXT)
	$(RM_F) $(GEN_FILES)
	$(RM_F) *.out *.o *.obj *.map *.rcb *.exe

install: $(PROGS:%=$(DESTDIR)$(bindir)/%)

$(DESTDIR)$(bindir)/%: %
	install -D -m 755 $< $@

.PHONY: all clean
