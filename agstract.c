#define _GNU_SOURCE
#include "Clib32.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define VERSION "0.0.1"
#define ADS ":::AGStract " VERSION "by rofl0r:::"

__attribute__((noreturn))
void usage(char *argv0) {
	dprintf(2, ADS "\nusage:\n%s agsgame.exe targetdir\n\n", argv0);
	exit(1);
}

int main(int argc, char** argv) {
	if(argc < 3) usage(argv[0]);
	dprintf(1, ADS "\n");
	struct AgsFile ags_b, *ags = &ags_b;
	char *fn = argv[1];
	char *dir = argv[2];
	char fnbuf[512];
	snprintf(fnbuf, sizeof(fnbuf), "%s/agspack.info", dir);
	int outfd = open(fnbuf, O_WRONLY | O_CREAT | O_TRUNC, 0660);
	if(outfd == -1) {
		perror(fnbuf);
		return 1;
	}
	
	if(!AgsFile_init(ags, fn)) {
		dprintf(2, "error opening %s\n", fn);
		return 1;
	}
	size_t i, l = AgsFile_getCount(ags);
	dprintf(1, "%s: version %d, containing %zu files.\n", fn, AgsFile_getVersion(ags), l);
	dprintf(outfd, "agspackfile=%s\nagsversion=%d\nfilecount=%zu\n", fn, AgsFile_getVersion(ags), l);
	
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
