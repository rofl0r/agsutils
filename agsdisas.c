#define _GNU_SOURCE
#include "DataFile.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "version.h"
#define ADS ":::AGSdisas " VERSION " by rofl0r:::"

static int usage(char *argv0) {
	fprintf(stderr, ADS "\nusage:\n%s [-oblf] file.o [file.s]\n"
		   "pass input and optionally output filename.\n"
		   "options:\n"
		   "-v : verbose (show filenames)\n"
		   "-o : dump offset comments in disassembly\n"
		   "-b : dump hexadecimal bytecode comments in disassembly\n"
		   "-f : dump informative original fixups section\n"
		   "-l : remove linenumber debug assembly directives [produces smaller files]\n"
		   , argv0);
	exit(1);
}

static int disas(char *o, char *s, int flags, int verbose) {
	//ARF_find_code_start
	AF f_b, *f = &f_b;
	ASI sc;
	int err = 0;
	if(AF_open(f, o)) {
		ASI *i = ASI_read_script(f, &sc) ? &sc : 0;
		if(verbose) printf("disassembling %s -> %s", o, s);
		if(!i || !ASI_disassemble(f, i, s, flags)) err = 1;
		if(verbose) {
			if(err) printf(" FAIL\n");
			else printf(" OK\n");
		}
		AF_close(f);
	}
	return err;
}

int main(int argc, char**argv) {
	int flags = 0, c, verbose = 0;
	while ((c = getopt(argc, argv, "voblf")) != EOF) switch(c) {
		case 'v': verbose = 1; break;
		case 'o': flags |= DISAS_DEBUG_OFFSETS; break;
		case 'b': flags |= DISAS_DEBUG_BYTECODE; break;
		case 'l': flags |= DISAS_SKIP_LINENO; break;
		case 'f': flags |= DISAS_DEBUG_FIXUPS; break;
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
	if(!strcmp(o, s)) {
		fprintf(stderr, "error: input and output file (%s) identical!\n", o);
		return 1;
	}

	return disas(o, s, flags, verbose);
}
