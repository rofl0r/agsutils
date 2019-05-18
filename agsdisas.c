#define _GNU_SOURCE
#include "DataFile.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "version.h"
#define ADS ":::AGSdisas " VERSION " by rofl0r:::"

static int usage(char *argv0) {
	dprintf(2, ADS "\nusage:\n%s [-obl] file.o [file.s]\n"
		   "pass input and optionally output filename.\n"
		   "options:\n"
		   "-o : dump offset comments in disassembly\n"
		   "-b : dump hexadecimal bytecode comments in disassembly\n"
		   "-l : remove linenumber debug assembly directives [produces smaller files]\n"
		   , argv0);
	exit(1);
}

static void disas(char *o, char *s, int flags) {
	//ARF_find_code_start
	AF f_b, *f = &f_b;
	ASI sc;
	if(AF_open(f, o)) {
		ASI *i = ASI_read_script(f, &sc) ? &sc : 0;
		dprintf(1, "disassembling %s -> %s", o, s);
		if(!i || !ASI_disassemble(f, i, s, flags)) dprintf(1, " FAIL");
		dprintf(1, "\n");
		AF_close(f);
	}
}

int main(int argc, char**argv) {
	int flags = 0, c;
	while ((c = getopt(argc, argv, "obl")) != EOF) switch(c) {
		case 'o': flags |= DISAS_DEBUG_OFFSETS; break;
		case 'b': flags |= DISAS_DEBUG_BYTECODE; break;
		case 'l': flags |= DISAS_SKIP_LINENO; break;
		default: return usage(argv[0]);
	}
	if(!argv[optind]) return usage(argv[0]);
	char *o = argv[optind], *s, out[256];
	if(!argv[optind+1]) {
		size_t l = strlen(o);
		snprintf(out, 256, "%s", o);
		out[l-1] = 's'; // overflow me!
		s = out;
	} else s = argv[optind+1];
	disas(o, s, flags);
	return 0;
}
