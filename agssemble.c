#define _GNU_SOURCE
#include "Assembler.h"
#include "DataFile.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "version.h"
#define ADS ":::AGSsemble " VERSION " by rofl0r:::"

__attribute__((noreturn))
void usage(char *argv0) {
	dprintf(2, ADS "\nusage:\n%s file.s [file.o]\npass an ags assembly filename.\n"
	"if optional second filename is ommited, will write into file.o\n", argv0);
	exit(1);
}

int main(int argc, char** argv) {
	if(argc < 2 || argc > 3) usage(argv[0]);
	AS a_b, *a = &a_b;
	char* file = argv[1];
	AS_open(a, file);
	char out [256], *outn;
	if(argc == 2) {
		size_t l = strlen(file);
		snprintf(out, 256, "%s", file);
		out[l-1] = 'o'; // overflow me!
		outn = out;
	} else outn = argv[2];
	dprintf(1, "assembling %s -> %s ... ", file, outn);
	int ret = AS_assemble(a, outn);
	AS_close(a);
	
	if(!ret) dprintf(1, "FAIL\n");
	else dprintf(1, "OK\n");
	return !ret;
}
