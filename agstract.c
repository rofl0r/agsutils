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
	dprintf(2, ADS "\nusage:\n%s agsgame.exe targetdir\n\n", argv0);
	exit(1);
}

static void dump_exe(struct AgsFile *ags, const char *dir) {
	if(ags->pack_off) {
		char fnbuf[512];
		snprintf(fnbuf, sizeof(fnbuf), "%s/agspack.exestub", dir);
		AgsFile_extract(ags, 0, 0, ags->pack_off, fnbuf);
	}
}

static int open_packfile(const char* fn) {
	return open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0660);
}

int main(int argc, char** argv) {
	if(argc < 3) usage(argv[0]);
	dprintf(1, ADS "\n");
	struct AgsFile ags_b, *ags = &ags_b;
	char *fn = argv[1];
	char *dir = argv[2];
	char fnbuf[512];
	snprintf(fnbuf, sizeof(fnbuf), "%s/agspack.info", dir);
	int outfd = open_packfile(fnbuf);
	if(outfd == -1 && errno == ENOENT) {
		mkdir(dir,  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		outfd = open_packfile(fnbuf);
	}
	if(outfd == -1) {
		perror("open");
		dprintf(2, "did you forget to create %s?\n", dir);
		perror(fnbuf);
		return 1;
	}
	dprintf(outfd, "info=infofile created by %s\n"
	        "info=this file is needed to reconstruct the packfile with AGSpack\n", ADS);
	AgsFile_init(ags, fn);
	if(!AgsFile_open(ags)) {
		dprintf(2, "error opening %s\n", fn);
		return 1;
	}
	dump_exe(ags, dir);
	size_t i, l = AgsFile_getFileCount(ags), ld =AgsFile_getDataFileCount(ags);
	dprintf(1, "%s: version %d, containing %zu files.\n", fn, AgsFile_getVersion(ags), l);
	dprintf(outfd, "agspackfile=%s\nagsversion=%d\nfilecount=%zu\n", fn, AgsFile_getVersion(ags), l);
	dprintf(outfd, "datafilecount=%zu\n", ld);
	for(i = 0; i < ld; i++) {
		dprintf(outfd, "df%zu=%s\n", i, AgsFile_getDataFileName(ags, i));
	}
	for(i = 0; i < l; i++) {
		char buf[16];
		snprintf(buf, sizeof(buf), "%d", AgsFile_getFileNumber(ags, i));
		dprintf(outfd, "fileno%zu=%s\n", i, buf);
	}
	for(i = 0; i < l; i++) {
		char *currfn = AgsFile_getFileName(ags, i);
		snprintf(fnbuf, sizeof(fnbuf), "%s/%s", dir, currfn);
		dprintf(1, "%s -> %s\n", currfn, fnbuf);
		dprintf(outfd, "%zu=%s\n", i, currfn);
		AgsFile_dump(ags, i, fnbuf);
	}
	
	AgsFile_close(ags);
	close(outfd);
	
	return 0;
}
