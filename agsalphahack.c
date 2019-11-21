#define _GNU_SOURCE
#include "DataFile.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "version.h"
#define ADS ":::AGSalphahack " VERSION " by rofl0r:::"

static int usage(char *argv0) {
	dprintf(2, ADS "\nusage:\n%s [-s spriteno] DIR\n"
		   "removes alphachannel flag from specified sprite in game file.\n"
		   "if spriteno is -1 (default), all flags are removed.\n"
		   "gamefile is typically ac2game.dta or game28.dta inside DIR.\n"
		   "this is useful when sprites have been hacked/converted to 16bit.\n"
		   "you should make a backup of the file before doing this.\n"
		   , argv0);
	return 1;
}

#define SPF_ALPHACHANNEL 0x10

int main(int argc, char**argv) {
	int c, flags = 0, spriteno = -1;
	while ((c = getopt(argc, argv, "s:")) != EOF) switch(c) {
		case 's': spriteno = atoi(optarg); break;
		default: return usage(argv[0]);
	}
	if(!argv[optind]) return usage(argv[0]);
	char *dir = argv[optind];

	ADF a_b, *a = &a_b;
	char fnbuf[512];
	if(!ADF_find_datafile(dir, fnbuf, sizeof(fnbuf)))
		return 1;
	if(!ADF_open(a, fnbuf)) return 1;

	off_t off = ADF_get_spriteflagsstart(a);
	unsigned nsprites = ADF_get_spritecount(a);
	ADF_close(a);

	printf("removing alpha of %u out of %u spriteflags.\n", spriteno==-1?nsprites:1, nsprites);

	FILE *f = fopen(fnbuf, "r+b");
	if(!f) return 1;
	fseeko(f, off, SEEK_SET);
	unsigned char *buf = malloc(nsprites);
	fread(buf, 1, nsprites, f);

	size_t i;
	for(i=0;i<nsprites;++i) {
		if(spriteno == -1 || spriteno == i)
			buf[i] = buf[i] & (~SPF_ALPHACHANNEL);
	}
	fseeko(f, off, SEEK_SET);
	fwrite(buf, 1, nsprites, f);
	fclose(f);
	free(buf);
	return 0;
}
