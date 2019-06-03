#define _GNU_SOURCE
#include "Assembler.h"
#include "DataFile.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "version.h"
#define ADS ":::AGSsemble " VERSION " by rofl0r:::"

static int usage(char *argv0) {
	dprintf(2, ADS "\nusage:\n%s [-E] file.s [file.o]\npass an ags assembly filename.\n"
	"-E: invoke C preprocessor 'cpp' on the input file before assembling\n"
	"if optional second filename is ommited, will write into file.o\n", argv0);
	return 1;
}

int main(int argc, char** argv) {
	int flags = 0, c;
		while ((c = getopt(argc, argv, "E")) != EOF) switch(c) {
		case 'E': flags |= 1; break;
		default: return usage(argv[0]);
	}
	if(!argv[optind]) return usage(argv[0]);
	char* file = argv[optind];
	char out [256], *outn, tmp[256];
	if(!argv[optind+1]) {
		size_t l = strlen(file);
		char *p;
		snprintf(out, 256, "%s", file);
		p = strrchr(out, '.');
		if(!p) p = out + l;
		*(p++) = '.';
		*(p++) = 'o';
		*p = 0;
		outn = out;
	} else outn = argv[optind+1];

	if(flags & 1) {
		snprintf(tmp, sizeof tmp, "%s.i", file);
		dprintf(1, "preprocessing %s -> %s ...", file, tmp);
		char cmd[1024];
		snprintf(cmd, sizeof cmd, "cpp %s > %s", file, tmp);
		int ret = system(cmd);
		if(ret) {
			dprintf(1, "FAIL\n");
			return ret;
		}
		dprintf(1, "OK\n");
		file = tmp;
	}

	AS a_b, *a = &a_b;
	AS_open(a, file);

	dprintf(1, "assembling %s -> %s ... ", file, outn);
	int ret = AS_assemble(a, outn);
	AS_close(a);

	if(!ret) dprintf(1, "FAIL\n");
	else dprintf(1, "OK\n");
	return !ret;
}
