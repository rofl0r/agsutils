#define _GNU_SOURCE
#include "DataFile.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "version.h"
#define ADS ":::AGSalphainfo " VERSION " by rofl0r:::"

static int usage(char *argv0) {
	fprintf(stderr, ADS "\nusage:\n%s [-s spriteno] DIR\n"
		   "prints alphachannel flags of all sprites, or specified sprite, in game file.\n"
		   "gamefile is typically ac2game.dta or game28.dta inside DIR.\n"
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

	enum ADF_open_error aoe = ADF_open(a, fnbuf);
	if(aoe != AOE_success && aoe <= AOE_gamebase) {
		fprintf(stderr, "failed to open/process data file: %s\n", AOE2str(aoe));
		return 1;
	} else if (aoe != AOE_success) {
		fprintf(stderr, "warning: failed to process some non-essential parts (%s) of gamefile, probably from a newer game format\n", AOE2str(aoe));
	}

	off_t off = ADF_get_spriteflagsstart(a);
	unsigned nsprites = ADF_get_spritecount(a);
	ADF_close(a);

	FILE *f = fopen(fnbuf, "rb");
	if(!f) return 1;
	fseeko(f, off, SEEK_SET);
	unsigned char *buf = malloc(nsprites);
	fread(buf, 1, nsprites, f);
	fclose(f);

	size_t i;
	static const char *offon[] = { "OFF", "ON" };
	for(i=0;i<nsprites;++i) {
		if(spriteno == -1 || spriteno == i)
			printf("sprite %zu: alpha %s\n", i, offon[!!(buf[i] & SPF_ALPHACHANNEL)]);
	}
	free(buf);
	return 0;
}
