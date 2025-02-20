#define _GNU_SOURCE
#include "Clib32.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "version.h"
#ifdef _WIN32
#include <direct.h>
#define MKDIR(D) mkdir(D)
#define PSEP '\\'
#else
#define MKDIR(D) mkdir(D, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#define PSEP '/'
#endif

#define ADS ":::AGStract " VERSION " by rofl0r:::"

static void usage(char *argv0) {
	fprintf(stderr, ADS "\nusage:\n%s agsgame.exe targetdir\n\n", argv0);
	exit(1);
}

static void dump_exe(struct AgsFile *ags, const char *dir) {
	if(ags->pack_off) {
		char fnbuf[512];
		snprintf(fnbuf, sizeof(fnbuf), "%s%cagspack.exestub", dir, PSEP);
		AgsFile_extract(ags, 0, 0, ags->pack_off, fnbuf);
	}
}

static FILE* open_packfile(const char* fn) {
	return fopen(fn, "w");
}

#define EFPRINTF(F, FMT, ...) \
	do{if(fprintf(F, FMT, __VA_ARGS__) < 0) {perror("fprintf"); return 1;}}while(0)
int main(int argc, char** argv) {
	if(argc < 3) usage(argv[0]);
	fprintf(stdout, ADS "\n");
	struct AgsFile *ags = calloc(1, sizeof(*ags));
	char *fn = argv[1];
	char *dir = argv[2];
	char fnbuf[512];
	char db[512];

	AgsFile_init(ags, fn);
	if(!AgsFile_open(ags)) {
		fprintf(stderr, "error opening %s\n", fn);
		return 1;
	}

	snprintf(fnbuf, sizeof(fnbuf), "%s%cagspack.info", dir, PSEP);
	FILE *outf = open_packfile(fnbuf);
	if(outf == 0 && errno == ENOENT) {
		MKDIR(dir);
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

	if(strchr(fn, PSEP)) {
		strcpy(db, fn);
		*strrchr(db, PSEP) = 0;
		AgsFile_setSourceDir(ags, db);
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
		snprintf(fnbuf, sizeof(fnbuf), "%s%c%s", dir, PSEP, currfn);
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
