PROGS_SRCS = \
	agstract.c \
	agspack.c \
	agscriptxtract.c \
	agssemble.c \
	agsdisas.c \
	agssim.c \
	agsinject.c

PROGS_OBJS =  $(PROGS_SRCS:.c=.o)
PROGS = $(PROGS_SRCS:.c=)

LIB_SRCS = \
	Assembler.c \
	ByteArray.c \
	Clib32.c \
	DataFile.c \
	File.c \
	List.c \
	MemGrow.c \
	RoomFile.c \
	Script.c \
	StringEscape.c

LIB_OBJS =  $(LIB_SRCS:.c=.o)

CFLAGS_WARN = -Wall -Wextra -Wno-unknown-pragmas

-include config.mak

all: $(PROGS)

$(PROGS): $(LIB_OBJS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_WARN) -o $@ -c $<

%: %.o $(LIB_OBJS)
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
	rm -f $(PROGS) $(LIB_OBJS) $(PROGS_OBJS)
	rm -f *.out
	rm -f *.o
	rm -f *.rcb

.PHONY: all clean
