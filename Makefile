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
	agsinject.c

PROGS_OBJS =  $(PROGS_SRCS:.c=.o)
CPROGS = $(PROGS_SRCS:.c=$(EXE_EXT))
PROGS = $(CPROGS) agsoptimize agsex

LIB_SRCS = \
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
	SpriteFile.c \
	rle.c \

SPRITE_OBJS =  $(SPRITE_SRCS:.c=.o)

ASM_SRCS = \
	Assembler.c \
	preproc.c \
	tokenizer.c

ASM_OBJS =  $(ASM_SRCS:.c=.o)

LIB_OBJS =  $(LIB_SRCS:.c=.o)

CFLAGS_WARN = -Wall -Wextra -Wno-unknown-pragmas -Wno-sign-compare -Wno-switch -Wno-unused -Wno-pointer-sign

ifeq ($(WINBLOWS),1)
CPPFLAGS=-DNO_MMAN
CC=gcc
EXE_EXT=.exe
RM_F=del
else
RM_F=rm -f
endif

-include config.mak

all: $(PROGS)

$(PROGS_OBJS): $(LIB_OBJS)

agssemble$(EXE_EXT): agssemble.o $(LIB_OBJS) $(ASM_OBJS)
agsprite$(EXE_EXT): agsprite.o $(LIB_OBJS) $(SPRITE_OBJS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_WARN) -o $@ -c $<

%$(EXE_EXT): %.o $(LIB_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_WARN) -o $@ $^ $(LDFLAGS)

rcb:
	make -f Makefile.binary FNAME=agstract
	make -f Makefile.binary FNAME=agspack
	make -f Makefile.binary FNAME=agscriptxtract
	make -f Makefile.binary FNAME=agssemble
	make -f Makefile.binary FNAME=agsdisas
	make -f Makefile.binary FNAME=agsinject
	make -f Makefile.binary FNAME=agssim

clean:
	$(RM_F) $(CPROGS) $(LIB_OBJS) $(PROGS_OBJS) $(ASM_OBJS) $(SPRITE_OBJS)
	$(RM_F) *.out *.o *.rcb *.exe

install: $(PROGS:%=$(DESTDIR)$(bindir)/%)

$(DESTDIR)$(bindir)/%: %
	install -D -m 755 $< $@

.PHONY: all clean
