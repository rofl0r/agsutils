#define _GNU_SOURCE
#include "Clib32.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "version.h"
#ifdef _WIN32
#define PSEP '\\'
#else
#define PSEP '/'
#endif
#define ADS ":::AGSpack " VERSION " by rofl0r:::"

static int usage(char *argv0) {
	fprintf(stderr, ADS
		"\nusage:\n%s OPTIONS directory target-pack\n\n"
		"OPTIONS:\n"
		"-e: recreate original exe stub\n"
	, argv0);
	return 1;
}

int main(int argc, char** argv) {
	int c, exe_opt = 0;
	while((c = getopt(argc, argv, "e")) != -1) switch(c) {
		default: return usage(argv[0]);
		case 'e': exe_opt = 1; break;
	}
	if (!argv[optind] || !argv[optind+1])
		return usage(argv[0]);

	char *dir = argv[optind];
	char *pack = argv[optind+1];
	char fnbuf[512];
	char line[1024];
	FILE* fp;
	snprintf(fnbuf, sizeof(fnbuf), "%s%c%s", dir, PSEP, "agspack.info");
	if(!(fp = fopen(fnbuf, "r"))) {
		fprintf(stderr, "couldnt open %s\n", fnbuf);
		return 1;
	}
	if(exe_opt) {
		snprintf(fnbuf, sizeof(fnbuf), "%s%c%s", dir, PSEP, "agspack.exestub");
		if(access(fnbuf, R_OK) == -1) {
			fprintf(stderr, "exestub requested, but couldnt read %s\n", fnbuf);
			return 1;
		}
	}
	size_t index = 0;
	struct AgsFile *ags = calloc(1, sizeof(*ags));
	AgsFile_init(ags, pack);
	AgsFile_setSourceDir(ags, dir);

	if(!AgsFile_appendDataFile(ags, "AGSPACKv" VERSION)) {
		fprintf(stderr, "error: packname exceeds 20 chars\n");
		return 1;
	}
	if(exe_opt) AgsFile_setExeStub(ags, "agspack.exestub");
	size_t filecount = 0;
	while(fgets(line, sizeof(line), fp)) {
		size_t l = strlen(line);
		if(l) {
			line[l - 1] = 0;
			if(--l && line[l-1] == '\r') line[l - 1] = 0;
		}
		char *p = strchr(line, '=');
		if(!p) return 1;
		*p = 0; p++;
		if(0) ;
		else if(strcmp(line, "agsversion") == 0 || strcmp(line, "mflversion") == 0)
			AgsFile_setVersion(ags, atoi(p));
		else if(strcmp(line, "filecount") == 0) {
			filecount = atoi(p);
			AgsFile_setNumFiles(ags, filecount);
		}
		else if(isdigit(*line)) {
			// FIXME: we just add the files sequentially now
			// though it's most likely correct...
			//if(!AgsFile_setFile(ags, index++, p))
			if(filecount == 0) {
				fprintf(stderr, "error: filecount required before first file\n");
				return 1;
			}
			++index;
			if(!AgsFile_appendFile(ags, p)) {
				perror(p);
				return 1;
			}
		}
	}
	fclose(fp);
	for(index = 0; index < filecount; index++) {
		// TODO read from input file, but it seems to be all 0 for some games.
		AgsFile_setFileNumber(ags, index, 0);
	}
	int ret = AgsFile_write(ags);
	if(!ret) perror("write");
	AgsFile_close(ags);
	free(ags);
	return !ret;
}

