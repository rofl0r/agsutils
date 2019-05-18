#define _GNU_SOURCE
#include "Clib32.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "version.h"
#define ADS ":::AGSpack " VERSION "by rofl0r:::"

__attribute__((noreturn))
void usage(char *argv0) {
	dprintf(2, ADS "\nusage:\n%s directory target-pack\n\n", argv0);
	exit(1);
}

int main(int argc, char** argv) {
	if(argc < 3) usage(argv[0]);
	char *dir = argv[1];
	char *pack = argv[2];
	char fnbuf[512];
	char line[1024];
	FILE* fp;
	snprintf(fnbuf, sizeof(fnbuf), "%s/%s", dir, "agspack.info");
	if(!(fp = fopen(fnbuf, "r"))) {
		dprintf(2, "couldnt open %s\n", fnbuf);
		return 1;
	}
	size_t index = 0;
	struct AgsFile ags_b, *ags = &ags_b;
	AgsFile_init(ags, pack);
	AgsFile_setSourceDir(ags, dir);
	AgsFile_setDataFileCount(ags, 1); //TODO
	if(!AgsFile_setDataFile(ags, 0, "AGSPACKv0.0.1")) {
		dprintf(2, "error: packname exceeds 20 chars");
		return 1;
	}
	while(fgets(line, sizeof(line), fp)) {
		size_t l = strlen(line);
		if(l) line[l - 1] = 0;
		char *p = strchr(line, '=');
		if(!p) return 1;
		*p = 0; p++;
		if(0) ;
		else if(strcmp(line, "agsversion") == 0)
			AgsFile_setVersion(ags, atoi(p));
		else if(strcmp(line, "filecount") == 0)
			AgsFile_setFileCount(ags, atoi(p));
		else if(isdigit(*line))
			if(!AgsFile_setFile(ags, index++, p)) {
				perror(p);
				return 1;
			}
	}
	fclose(fp);
	size_t l = AgsFile_getFileCount(ags);
	for(index = 0; index < l; index++) {
		// TODO read from input file, but it seems to be all 0 for some games.
		AgsFile_setFileNumber(ags, index, 0);
	}
	int ret = !AgsFile_write(ags);
	AgsFile_close(ags);
	return ret;
}

