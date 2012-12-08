#define _GNU_SOURCE
#include "Assembler.h"
#include "DataFile.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define VERSION "0.0.1"
#define ADS ":::AGSsemble " VERSION " by rofl0r:::"

__attribute__((noreturn))
void usage(char *argv0) {
	dprintf(2, ADS "\nusage:\n%s file.s\npass an ags assemble filename.\n"
	"will write into file.o\n", argv0);
	exit(1);
}

int main(int argc, char** argv) {
	if(argc != 2) usage(argv[0]);
	AS a_b, *a = &a_b;
	char* file = argv[1];
	AS_open(a, file);
	char out [256];
	size_t l = strlen(file);
	snprintf(out, 256, "%s", file);
	out[l-1] = 'o'; // overflow me!
	int ret = AS_assemble(a, out);
	AS_close(a);
	return !ret;
}
