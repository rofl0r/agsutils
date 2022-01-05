#define _GNU_SOURCE
#include "Clib32.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "version.h"
#define ADS ":::AGStract " VERSION " by rofl0r:::"

__attribute__((noreturn))
void usage(char *argv0) {
	fprintf(stderr, ADS "\nusage:\n%s agsgame.exe targetdir\n\n", argv0);
	exit(1);
}

static void dump_exe(struct AgsFile *ags, const char *dir) {
	if(ags->pack_off) {
		char fnbuf[512];
		snprintf(fnbuf, sizeof(fnbuf), "%s/agspack.exestub", dir);
		AgsFile_extract(ags, 0, 0, ags->pack_off, fnbuf);
	}
}

static FILE* open_packfile(const char* fn) {
	return fopen(fn, "w");
}

#define EFPRINTF(F, FMT, ARGS...) \
	do{if(fprintf(F, FMT, ## ARGS) < 0) {perror("fprintf"); return 1;}}while(0)
int main(int argc, char** argv) {
	if(argc < 3) usage(argv[0]);
	fprintf(stdout, ADS "\n");
	struct AgsFile *ags = calloc(1, sizeof(*ags));
	char *fn = argv[1];
	char *dir = argv[2];
	char fnbuf[512];
	snprintf(fnbuf, sizeof(fnbuf), "%s/agspack.info", dir);
	FILE *outf = open_packfile(fnbuf);
	if(outf == 0 && errno == ENOENT) {
		mkdir(dir,  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		outf = open_packfile(fnbuf);
	}
	if(outf == 0) {
		perror("fopen");
		fprintf(stderr, "did you forget to create %s?\n", dir);
		perror(fnbuf);
		return 1;
	}
	EFPRINTF(outf, "info=infofile created by %s\n"
	        "info=this file is needed to reconstruct the packfile with AGSpack\n", ADS);
	AgsFile_init(ags, fn);
	if(!AgsFile_open(ags)) {
		fprintf(stderr, "error opening %s\n", fn);
		return 1;
	}
	dump_exe(ags, dir);
	int ec = 0;
	size_t i, l = AgsFile_getFileCount(ags), ld =AgsFile_getDataFileCount(ags);
	fprintf(stdout, "%s: mfl version %d, containing %zu files.\n", fn, AgsFile_getVersion(ags), l);
	EFPRINTF(outf, "agspackfile=%s\nmflversion=%d\nfilecount=%zu\n", fn, AgsFile_getVersion(ags), l);
	EFPRINTF(outf, "datafilecount=%zu\n", ld);
	for(i = 0; i < ld; i++) {
		EFPRINTF(outf, "df%zu=%s\n", i, AgsFile_getDataFileName(ags, i));
	}
	for(i = 0; i < l; i++) {
		char buf[16];
		snprintf(buf, sizeof(buf), "%d", AgsFile_getFileNumber(ags, i));
		EFPRINTF(outf, "fileno%zu=%s\n", i, buf);
	}
	for(i = 0; i < l; i++) {
		char *currfn = AgsFile_getFileName(ags, i);
		snprintf(fnbuf, sizeof(fnbuf), "%s/%s", dir, currfn);
		fprintf(stdout, "%s -> %s\n", currfn, fnbuf);
		EFPRINTF(outf, "%zu=%s\n", i, currfn);
		if(!AgsFile_dump(ags, i, fnbuf)) ec++;
	}

	AgsFile_close(ags);
	free(ags);
	fclose(outf);
	ec = !!ec;
	return ec;
}
