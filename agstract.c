#define _GNU_SOURCE
#include "Clib32.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define VERSION "0.0.1"

__attribute__((noreturn))
void usage(char *argv0) {
	dprintf(2, ":::AGStract " VERSION "by rofl0r:::\nusage:\n%s agsgame.exe targetdir\n\n", argv0);
	exit(1);
}

int main(int argc, char** argv) {
	if(argc < 3) usage(argv[0]);
	struct AgsFile ags_b, *ags = &ags_b;
	char *fn = argv[1];
	char *dir = argv[2];
	if(!AgsFile_init(ags, fn)) {
		dprintf(2, "error opening %s\n", fn);
		return 1;
	}
	size_t i, l = AgsFile_getCount(ags);
	char fnbuf[512];
	for(i = 0; i < l; i++) {
		snprintf(fnbuf, sizeof(fnbuf), "%s/%s", dir, AgsFile_getFileName(ags, i));
		dprintf(1, "%s -> %s\n", AgsFile_getFileName(ags, i), fnbuf);
		AgsFile_dump(ags, i, fnbuf);
	}
	
	AgsFile_close(ags);
	
	return 0;
}
