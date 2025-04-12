#define _GNU_SOURCE
#ifdef __POCC__
#define __STDC_WANT_LIB_EXT2__ 1
#endif
#include <stdio.h>
#include "Assembler.h"
#include "DataFile.h"
#include <stdlib.h>
#include <unistd.h>
#include "version.h"
#define ADS ":::AGSsemble " VERSION " by rofl0r:::"

#ifndef DISABLE_CPP
#include "preproc.h"
#endif

static int usage(char *argv0) {
	fprintf(stderr, ADS "\nusage:\n%s [-E] [-i file.i] [-I includedir] [-D preproc define] file.s [file.o]\n"
	"pass an ags assembly filename.\n"
#ifndef DISABLE_CPP
	"-E: invoke built-in C preprocessor 'tinycpp' on the input file before assembling\n"
	"-I includedir - add include dir for CPP\n"
	"-D define - add define for CPP\n"
	"-i file save preprocessor output to file\n"
#else
	"preprocessor disabled in this build, so option -E,-I,-D,-i are ignored\n"
#endif
	"if optional second filename is ommited, will write into file.o\n", argv0);
	return 1;
}

#ifndef DISABLE_CPP
static FILE *freopen_r(FILE *f, char **buf, size_t *size) {
	fflush(f);
	fclose(f);
	return fmemopen(*buf, *size, "r");
}
#endif

int main(int argc, char** argv) {
#ifndef DISABLE_CPP
	struct cpp* cpp = cpp_new();
#endif
	char *tmp, *cppoutfn = 0;
	int flags = 0, c;
		while ((c = getopt(argc, argv, "Ei:I:D:")) != EOF) switch(c) {
#ifndef DISABLE_CPP
		case 'E': flags |= 1; break;
		case 'i': cppoutfn = optarg; break;
		case 'I': cpp_add_includedir(cpp, optarg); break;
		case 'D':
			if((tmp = strchr(optarg, '='))) *tmp = ' ';
			cpp_add_define(cpp, optarg);
			break;
#endif
		default: return usage(argv[0]);
	}
	if(!argv[optind]) return usage(argv[0]);
	char* file = argv[optind];
	char out [256], *outn;
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
	if(!strcmp(outn, file)) {
		fprintf(stderr, "error: input and output file (%s) identical!\n", file);
		return 1;
	}

	FILE *in = fopen(file, "r");
	if(!in) {
		fprintf(stderr, "error opening file %s\n", file);
		return 1;
	}

#ifndef DISABLE_CPP
	if(flags & 1) {
		struct FILE_container {
			FILE *f;
			char *buf;
			size_t len;
		} output = {0};
		if(!cppoutfn) output.f = open_memstream(&output.buf, &output.len);
		else output.f = fopen(cppoutfn, "w");
		fprintf(stdout, "preprocessing %s ...", file);
		int ret = cpp_run(cpp, in, output.f, file);
		if(!ret) {
			fprintf(stdout, "FAIL\n");
			return 1;
		}
		fprintf(stdout, "OK\n");
		fclose(in);
		if(!cppoutfn) in = freopen_r(output.f, &output.buf, &output.len);
		else {
			fclose(output.f);
			in = fopen(cppoutfn, "r");
		}
	}
	// cpp_free(cpp); // FIXME this crashes on windows, find out why
#endif

	AS a_b, *a = &a_b;
	AS_open_stream(a, in);

	fprintf(stdout, "assembling %s -> %s ... ", file, outn);
	int ret = AS_assemble(a, outn);
	AS_close(a);

	if(!ret) fprintf(stdout, "FAIL\n");
	else fprintf(stdout, "OK\n");
	return !ret;
}
