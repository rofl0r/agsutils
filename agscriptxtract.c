#define _GNU_SOURCE
#include "DataFile.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "version.h"
#define ADS ":::AGStract " VERSION " by rofl0r:::"

static int usage(char *argv0) {
	dprintf(2, ADS "\nusage:\n%s [-obl] dir\n"
		   "pass a directory with extracted game files.\n"
		   "options:\n"
		   "-o : dump offset comments in disassembly\n"
		   "-b : dump hexadecimal bytecode comments in disassembly\n"
		   "-l : remove linenumber debug assembly directives [produces smaller files]\n"
		   , argv0);
	return 1;
}

static void disas(const char*inp, char *o, int flags) {
	//ARF_find_code_start
	AF f_b, *f = &f_b;
	ASI sc;
	if(AF_open(f, o)) {
		char s[32];
		size_t l = strlen(o);
		memcpy(s, o, l + 1);
		s[l-1] = 's';
		ASI *i = ASI_read_script(f, &sc) ? &sc : 0;
		dprintf(1, "disassembling [%s] %s -> %s", inp, o, s);
		if(!i || !ASI_disassemble(f, i, s, flags)) dprintf(1, " FAIL");
		dprintf(1, "\n");
		AF_close(f);
	}
}

#include "RoomFile.h"
#include <dirent.h>
static void dumprooms(char* dir, int flags) {
	DIR* d = opendir(dir);
	if(!d) return;
	struct dirent* di = 0;

	while((di = readdir(d))) {
		size_t l = strlen(di->d_name);
		if(l > 4 + 4 && !memcmp(di->d_name, "room", 4) && !memcmp(di->d_name + l - 4, ".crm", 4)) {
			char fnbuf[512];
			snprintf(fnbuf, sizeof(fnbuf), "%s/%s", dir, di->d_name);
			AF f; ssize_t off; ASI s;
			if(!AF_open(&f, fnbuf)) continue;
			if((off = ARF_find_code_start(&f, 0)) == -1) continue;
			AF_set_pos(&f, off);
			if(!ASI_read_script(&f, &s)) {
				dprintf(2, "trouble finding script in %s\n", di->d_name);
				continue;
			}
			char buf[512];
			assert(l < sizeof(buf));
			memcpy(buf, di->d_name, l - 4);
			buf[l-4] = '.';
			buf[l-3] = 'o';
			buf[l-2] = 0;
			AF_dump_chunk(&f, s.start, s.len, buf);
			disas(di->d_name, buf, flags);
		}
	}
	closedir(d);
}

void dump_script(AF* f, ASI* s, char* fn, int flags) {
	if(!s->len) return;
	AF_dump_chunk(f, s->start, s->len, fn);
	disas("game28.dta", fn, flags);
}

int main(int argc, char**argv) {
	int flags = 0, c;
	while ((c = getopt(argc, argv, "obl")) != EOF) switch(c) {
		case 'o': flags |= DISAS_DEBUG_OFFSETS; break;
		case 'b': flags |= DISAS_DEBUG_BYTECODE; break;
		case 'l': flags |= DISAS_SKIP_LINENO; break;
		default: return usage(argv[0]);
	}
	if(!argv[optind]) return usage(argv[0]);
	char *dir = argv[optind];
	ADF a_b, *a = &a_b;
	ADF_init(a, dir);
	if(!ADF_open(a)) return 1;
	ASI* s;
	s = ADF_get_global_script(a);
	dump_script(a->f, s, "globalscript.o", flags);
	s = ADF_get_dialog_script(a);
	dump_script(a->f, s, "dialogscript.o", flags);
	size_t i, l = ADF_get_scriptcount(a);
	for(i = 0; i < l; i++) {
		char fnbuf[32];
		s = ADF_get_script(a, i);
		snprintf(fnbuf, sizeof(fnbuf), "gamescript%zu.o", i);
		dump_script(a->f, s, fnbuf, flags);
	}
	ADF_close(a);
	dumprooms(dir, flags);

	return 0;
}
