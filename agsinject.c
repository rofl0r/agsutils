#define _GNU_SOURCE
#include "DataFile.h"
#include "RoomFile.h"
#include "ByteArray.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "version.h"
#define ADS ":::AGSinject " VERSION " by rofl0r:::"

__attribute__((noreturn))
void usage(char *argv0) {
	dprintf(2,
	ADS "\nusage:\n%s index input.o inject_to.crm\n"
	"index is the number of script to replace, i.e. 0 for first script\n"
	"only relevant if the output file is a gamefile which contains multiple scripts\n"
	"for example gamescript is 0, dialogscript is 1 (if existing), etc\n"
	"a room file (.crm) only has one script so you must pass 0.\n", argv0);
	exit(1);
}

/* inj = filename of file to inject in */
static int inject(char *o, char *inj, unsigned which) {
	//ARF_find_code_start
	AF f_b, *f = &f_b;
	size_t index, found;
	int isroom = !strcmp(".crm", inj + strlen(inj) - 4);
	if(isroom && which != 0) return -1;
	if(!AF_open(f, inj)) return 0;
	ssize_t start;
	for(index = found = 0; 1 ; found++, index = start + 4) {
		if(!isroom && (start = ARF_find_code_start(f, index)) == -1) {
			dprintf(2, "error, only %zu scripts found\n", found);
			return 0;
		} else if(isroom) {
			/* use roomfile specific script lookup, as it's faster */
			struct RoomFile rinfo = {0};
			if(!RoomFile_read(f, &rinfo)) return 0;
			start = rinfo.blockpos[BLOCKTYPE_COMPSCRIPT3];
		}
		if(found != which) continue;
		char *tmp = tempnam(".", "agsinject.tmp");
		FILE *out = fopen(tmp, "w");
		if(!out) return 0;

		/* 1) dump header */
		AF_dump_chunk_stream(f, 0, isroom ? start -4 : start, out);
		AF_set_pos(f, start);

		/* open replacement object file */
		struct ByteArray b;
		ByteArray_ctor(&b);
		ByteArray_open_file(&b, o);

		if(isroom) {
			/* 2a) if room, write length */
			/* room files, unlike game files, have a length field of size 4 before
			 * the compiled script starts. */
			unsigned l = ByteArray_get_length(&b);
			struct ByteArray c;
			ByteArray_ctor(&c);
			ByteArray_open_mem(&c, 0, 0);
			ByteArray_set_flags(&c, BAF_CANGROW);
			ByteArray_set_endian(&c, BAE_LITTLE);
			ByteArray_writeInt(&c, l);
			ByteArray_dump_to_stream(&c, out);
			ByteArray_close(&c);
		}
		/* 2b) dump object file */
		ByteArray_dump_to_stream(&b, out);
		ByteArray_close_file(&b);

		ASI s;
		if(!ASI_read_script(f, &s)) {
			dprintf(2, "trouble finding script in %s\n", inj);
			return 0;
		}
		/* 3) dump rest of file */
		AF_dump_chunk_stream(f, start + s.len, ByteArray_get_length(f->b) - (start + s.len), out);
		AF_close(f);
		fclose(out);
		return !rename(tmp, inj);
	}
	return 0;
}

int main(int argc, char**argv) {
	if(argc != 4) usage(argv[0]);
	char *o = argv[2], *inj = argv[3], *p;
	if(!(p = strrchr(o, '.')) || strcmp(p, ".o")) {
		dprintf(2, "error: object file has no .o extension\n");
		return 1;
	}
	int which = atoi(argv[1]);
	dprintf(1, "injecting %s into %s as %d'th script ...", o, inj, which);
	int ret = inject(o, inj, which);
	if(ret >= 0) dprintf(1, "OK\n");
	else {
		dprintf(1, "FAIL\n");
		if(ret == -1) {
			dprintf(2, "invalid index %d for roomfile, only 0 possible\n", which);
			ret = 0;
		} else perror("error:");
	}
	return !ret;
}
