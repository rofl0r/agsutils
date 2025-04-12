#define _GNU_SOURCE
#include "DataFile.h"
#include "RoomFile.h"
#include "ByteArray.h"
#ifdef __POCC__
#define WINFILE_EXPORT static
#include "winfile.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include "version.h"
#define ADS ":::AGSinject " VERSION " by rofl0r:::"

#ifdef __POCC__
#include <time.h>
static char *tempnam(const char *dir, const char *pfx) {
	(void) dir;
	char buf[L_tmpnam + 64];
	buf[0] = 0;
	tmpnam(buf);
	int i; char *p = buf + strlen(buf);
	for(i = 0; pfx[i] && i < 8; ++i, ++p)
		*p = pfx[i];
	*p = 0;
	sprintf(p, "%016llx", (unsigned long long) _rdtsc());
	return strdup(buf);
}
#define RENAME(OLD, NEW) win_rename(OLD, NEW)
#else
#define RENAME(OLD, NEW) rename(OLD, NEW)
#endif

int usage(char *argv0) {
	fprintf(stderr,
	ADS "\n"
	"usage (simple):\n"
	"---------------\n"
	"%s index input.o inject_to.crm\n"
	"index is the number of script to replace, i.e. 0 for first script\n"
	"only relevant if the output file is a gamefile which contains multiple scripts\n"
	"for example gamescript is 0, dialogscript is 1 (if existing), etc\n"
	"a room file (.crm) only has one script so you must pass 0.\n\n"

	"usage (extended):\n"
	"-----------------\n"
	"%s -e [OPTIONS] target index1:input1.o [index2:input2.o...indexN:inputN.o]\n"
	"in extended mode, indicated by -e switch, target denotes destination file\n"
	"(e.g. game28.dta, *.crm...), and file(s) to inject are passed as\n"
	"index:filename tuples.\n"
	"this allows to inject several compiled scripts at once.\n"
	"OPTIONS:\n"
	"-t : only inject obj files whose timestamps are newer than the one of target.\n"
	"example: %s -e game28.dta 0:globalscript.o 1:dialogscript.o\n"
	, argv0, argv0, argv0);
	return 1;
}

/* inj = filename of file to inject in */
static int inject(const char *o, const char *inj, unsigned which) {
	//ARF_find_code_start
	AF f_b, *f = &f_b;
	unsigned long long index, found;
	int isroom = !strcmp(".crm", inj + strlen(inj) - 4);
	if(isroom && which != 0) return -2;
	if(!AF_open(f, inj)) return -1;
	long long start;
	for(index = found = 0; 1 ; found++, index = start + 4) {
		int room_length_bytes = 4;
		if(!isroom && (start = ARF_find_code_start(f, index)) == -1LL) {
			fprintf(stderr, "error, only %llu scripts found\n", (long long)found);
			return -3;
		} else if(isroom) {
			/* use roomfile specific script lookup, as it's faster */
			struct RoomFile rinfo = {0};
			if(!RoomFile_read(f, &rinfo)) return -3;
			start = rinfo.blockpos[BLOCKTYPE_COMPSCRIPT3];
			if(rinfo.version >= 32) room_length_bytes = 8;
		}
		if(found != which) continue;
		char *tmp = tempnam(".", "agsinject.tmp");
		FILE *out = fopen(tmp, "wb");
		if(!out) return -1;

		/* 1) dump header */
		AF_dump_chunk_stream(f, 0, isroom ? start -room_length_bytes : start, out);
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
			if(room_length_bytes == 8)
				/* we should actually write one long long
				   instead of 2 ints, but we assume that no
				   room script will be bigger than 2 GB. */
				ByteArray_writeInt(&c, 0);
			ByteArray_dump_to_stream(&c, out);
			ByteArray_close(&c);
		}
		/* 2b) dump object file */
		ByteArray_dump_to_stream(&b, out);
		ByteArray_close_file(&b);

		ASI s;
		if(!ASI_read_script(f, &s)) {
			fprintf(stderr, "trouble finding script in %s\n", inj);
			return -3;
		}
		/* 3) dump rest of file */
		AF_dump_chunk_stream(f, start + s.len, ByteArray_get_length(f->b) - (start + s.len), out);
		AF_close(f);
		fclose(out);

		int rnret = RENAME(tmp, inj);
		if(rnret == -1 && errno == EEXIST) {
			/* windows is special, as usual */
			fprintf(stderr, "rename failed from %s to %s\n", tmp, inj);
		}
		return rnret;
	}
	return -5;
}

static int check_objname(const char* o) {
	const char* p;
	if(!(p = strrchr(o, '.')) || strcmp(p, ".o")) {
		fprintf(stderr, "error: object file has no .o extension\n");
		return 0;
	}
	return 1;
}

static int injectpr(const char *obj, const char *out, unsigned which) {
	printf("injecting %s into %s as %d'th script ...", obj, out, which);
	int ret = inject(obj, out, which);
	if(ret == 0) printf("OK\n");
	else {
		printf("FAIL\n");
		if(ret == -2) {
			fprintf(stderr, "invalid index %d for roomfile, only 0 possible\n", which);
			ret = 0;
		} else if (ret == -1) perror("error");
		return 0;
	}
	return 1;
}

static int getstamp(const char* fn, time_t *stamp) {
	struct stat st;
	if(stat(fn, &st) == -1) {
		perror("stat");
		return 0;
	}
	*stamp = st.st_mtime;
	return 1;
}

static int ts_is_newer(const time_t *t1, const time_t *t2)
{
	return *t2 > *t1;
}

int main(int argc, char**argv) {
	char *out, *obj;
	int which;
	if(argc == 4 && isdigit(*argv[1])) {
		obj = argv[2];
		out = argv[3];
		which = atoi(argv[1]);
		if(!check_objname(obj)) return 1;
		if(!injectpr(obj, out, which)) return 1;
		return 0;
	}
	int c, extended = 0, usestamps = 0;
	while ((c = getopt(argc, argv, "et")) != EOF) switch(c) {
		case 'e': extended = 1; break;
		case 't': usestamps = 1; break;
		default: return usage(argv[0]);
	}
	if(!extended || !argv[optind] || !argv[optind+1])
		return usage(argv[0]);

	out = argv[optind];
	time_t stamp = {0};

	if(usestamps && !getstamp(out, &stamp)) return 1;

	while(argv[++optind]) {
		obj = argv[optind];
		char *p = strchr(obj, ':');
		if(!isdigit(*obj) || !p) return usage(argv[0]);
		*p = 0;
		which = atoi(obj);
		obj = ++p;
		if(!check_objname(obj)) return 1;
		if(usestamps) {
			time_t ostamp;
			if(!getstamp(obj, &ostamp)) return 1;
			if(!ts_is_newer(&stamp, &ostamp)) continue;
		}
		if(!injectpr(obj, out, which)) return 1;
	}
	return 0;
}
