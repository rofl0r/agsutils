#define _GNU_SOURCE
#include "DataFile.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define VERSION "0.0.1"
#define ADS ":::AGStract " VERSION " by rofl0r:::"

__attribute__((noreturn))
void usage(char *argv0) {
	dprintf(2, ADS "\nusage:\n%s dir\npass a directory with extracted game files.\n", argv0);
	exit(1);
}

static void disas(char *o) {
	//ARF_find_code_start
	AF f_b, *f = &f_b;
	ASI sc;
	if(AF_open(f, o)) {
		char s[32];
		size_t l = strlen(o);
		memcpy(s, o, l + 1);
		s[l-1] = 's';
		ASI *i = ASI_read_script(f, &sc) ? &sc : 0;
		dprintf(1, "disassembling %s -> %s\n", o, s);
		if(i) ASI_disassemble(f, i, s);
		AF_close(f);
	}
}

#include "RoomFile.h"
#include <dirent.h>
static void dumprooms(char* dir) {
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
			if((off = ARF_find_code_start(&f)) == -1) continue;
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
			disas(buf);
		}
	}
	closedir(d);
}

int main(int argc, char**argv) {
	char *dir = argv[1];
	if(argc != 2) usage(argv[0]);
	ADF a_b, *a = &a_b;
	ADF_init(a, dir);
	if(!ADF_open(a)) return 1;
	ASI* s;
	s = ADF_get_global_script(a);
	if(s->len) AF_dump_chunk(a->f, s->start, s->len, "globalscript.o");
	s = ADF_get_dialog_script(a);
	if(s->len) AF_dump_chunk(a->f, s->start, s->len, "dialogscript.o");
	size_t i, l = ADF_get_scriptcount(a);
	for(i = 0; i < l; i++) {
		char fnbuf[32];
		s = ADF_get_script(a, i);
		snprintf(fnbuf, sizeof(fnbuf), "gamescript%zu.o", i);
		if(s->len) {
			AF_dump_chunk(a->f, s->start, s->len, fnbuf);
			disas(fnbuf);
		}
	}
	disas("globalscript.o");
	disas("dialogscript.o");
	ADF_close(a);
	dumprooms(dir);

	return 0;
}