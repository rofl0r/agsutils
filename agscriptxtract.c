#define _GNU_SOURCE
#include "DataFile.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "version.h"

#ifdef _WIN32
#include <direct.h>
#define MKDIR(D) mkdir(D)
#else
#define MKDIR(D) mkdir(D, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#endif

#define ADS ":::AGStract " VERSION " by rofl0r:::"

static int usage(char *argv0) {
	fprintf(stderr, ADS "\nusage:\n%s [-oblf] dir [outdir]\n"
		   "extract all scripts from game files in dir\n"
		   "pass a directory with extracted game files.\n"
		   "options:\n"
		   "-o : dump offset comments in disassembly\n"
		   "-b : dump hexadecimal bytecode comments in disassembly\n"
		   "-f : dump informative original fixups section\n"
		   "-l : remove linenumber debug assembly directives [produces smaller files]\n"
		   "-v : enable verbose warning output\n"
		   , argv0);
	return 1;
}

static float gamefileversion2engine(int gamever) {
	static const float oldversions[] = {
	[5] = 2.00, [6] = 2.01, [7] = 2.03, [9] = 2.07,
	[11] = 2.20, [12] = 2.30, [18] = 2.50, [19] = 2.51,
	[20] = 2.53, [21] = 2.54, [22] = 2.55, [24] = 2.56,
	[25] = 2.60, [26] = 2.61, [27] = 2.62, [31] = 2.70,
	[32] = 2.72, [35] = 3.00, [36] = 3.01, [37] = 3.10,
	[39] = 3.11, [40] = 3.12, [41] = 3.20, [42] = 3.21,
	[43] = 3.30, [44] = 3.31, [45] = 3.40, [46] = 3.401,
	[47] = 3.402, [48] = 3.41, [49] = 3.411, [50] = 3.50,
	};
	if(gamever <= 50) return oldversions[gamever];
	return gamever / 1000000 + ((double)gamever / (double)100000.f) - gamever / 100000;
}

static void disas(const char*inp, char *o, int flags) {
	//ARF_find_code_start
	AF f_b, *f = &f_b;
	ASI sc;
	if(AF_open(f, o)) {
		char s[256];
		size_t l = strlen(o);
		memcpy(s, o, l + 1);
		s[l-1] = 's';
		ASI *i = ASI_read_script(f, &sc) ? &sc : 0;
		fprintf(stdout, "disassembling [%s] %s -> %s", inp, o, s);
		if(!i || !ASI_disassemble(f, i, s, flags)) fprintf(stdout, " FAIL");
		fprintf(stdout, "\n");
		AF_close(f);
	}
}

static char *filename(const char *dir, const char *fn, char *buf, size_t bsize) {
	snprintf(buf, bsize, "%s/%s", dir, fn);
	return buf;
}

#include "RoomFile.h"
#include <dirent.h>

static int is_roomfile(const char *fn) {
	const char *p = strrchr(fn, '.');
	if(!p) return 0;
	++p;
#define LOWER(X) tolower((unsigned) (X))
	return LOWER(p[0]) == 'c' && LOWER(p[1]) == 'r' && LOWER(p[2]) == 'm' && p[3] == 0;
#undef LOWER
}

static int dumprooms(const char* dir, const char* out, int flags) {
	DIR* d = opendir(dir);
	if(!d) return 1;
	int errors = 0;
	struct dirent* di = 0;

	while((di = readdir(d))) {
		size_t l = strlen(di->d_name);
		if(l > 4 && is_roomfile(di->d_name)) {
			char fnbuf[512];
			snprintf(fnbuf, sizeof(fnbuf), "%s/%s", dir, di->d_name);
			AF f; ssize_t off; ASI s;
			if(!AF_open(&f, fnbuf)) goto extract_error;
			struct RoomFile rinfo = {0};
			if(!RoomFile_read(&f, &rinfo)) goto extract_error;
			if((off = ARF_find_code_start(&f, 0)) == -1) goto extract_error;
			assert(off == rinfo.blockpos[BLOCKTYPE_COMPSCRIPT3]);
			AF_set_pos(&f, off);
			if(!ASI_read_script(&f, &s)) {
				fprintf(stderr, "trouble finding script in %s\n", di->d_name);
				continue;
			}
			char buf[256];
			assert(l < sizeof(buf));
			memcpy(buf, di->d_name, l - 4);
			buf[l-4] = '.';
			buf[l-3] = 'o';
			buf[l-2] = 0;
			char outbuf[256];
			AF_dump_chunk(&f, s.start, s.len, filename(out, buf, outbuf, sizeof outbuf));
			disas(di->d_name, outbuf, flags);
			size_t sourcelen;
			char *source = RoomFile_extract_source(&f, &rinfo, &sourcelen);
			if(source) {
				buf[l-3] = 'a';
				buf[l-2] = 's';
				buf[l-1] = 'c';
				buf[l] = 0;
				FILE *f = fopen(filename(out, buf, outbuf, sizeof outbuf), "wb");
				if(f) {
					fprintf(stdout, "extracting room source %s -> %s\n", di->d_name, outbuf);
					fwrite(source, 1, sourcelen, f);
					fclose(f);
				}
				free(source);
			}
			continue;
			extract_error:
			fprintf(stderr, "warning: extraction of file %s failed\n", di->d_name);
			++errors;
		}
	}
	closedir(d);
	return errors;
}

void dump_script(AF* f, ASI* s, char* fn, int flags) {
	if(!s->len) return;
	AF_dump_chunk(f, s->start, s->len, fn);
	disas("game28.dta", fn, flags);
}

static char *pfx_capitalize(char *in, char prefix, char *out) {
	char *p = out, *n = in;
	*(p++) = prefix;
	*(p++) = toupper(*(n++));
	while(*n) *(p++) = tolower(*(n++));
	*p = 0;
	return out;
}

void dump_header(ADF *a, char *fn) {
	unsigned i;
	fprintf(stdout, "regenerating script header %s\n", fn);
	FILE *f = fopen(fn, "w");
	fprintf(f, "#if SCRIPT_API < 300000 && SCRIPT_API > 262000\n");
	if(ADF_get_cursorcount(a)) fprintf(f, "enum CursorMode {\n");
	for(i=0; i<ADF_get_cursorcount(a); ++i) {
		if(i > 0) fprintf(f, ",\n");
		char buf[16] = {0}, *p = ADF_get_cursorname(a, i), *q = buf;
		while(*p) {
			if(!isspace(*p)) *(q++) = *p;
			++p;
		}
		fprintf(f, "  eMode%s = %u", buf, i);
	}
	if(i) fprintf(f, "};\n");
	fprintf(f, "import Character character[%u];\n", ADF_get_charactercount(a));
	for(i=0; i<ADF_get_charactercount(a); ++i) {
		char buf[64], *p = buf, *n = ADF_get_characterscriptname(a, i);
		pfx_capitalize(ADF_get_characterscriptname(a, i), 'c', buf);
		fprintf(f, "import Character %s;\n", buf);
	}
	fprintf(f, "import InventoryItem inventory[%u];\n", ADF_get_inventorycount(a));
	if(a->inventorynames) for(i=1; i<ADF_get_inventorycount(a); ++i) {
		fprintf(f, "import InventoryItem %s;\n", ADF_get_inventoryname(a, i));
	}
	for(i=0; i<ADF_get_guicount(a); ++i) {
		char buf[64];
		pfx_capitalize(ADF_get_guiname(a, i), 'g', buf);
		fprintf(f, "import GUI %s;\n", buf);
	}
	fprintf(f, "#endif\n");
	for(i=0; i<ADF_get_charactercount(a); ++i)
		fprintf(f, "#define %s %zu\n", ADF_get_characterscriptname(a, i), (size_t)i);
	for(i=0; i<ADF_get_guicount(a); ++i) {
		char buf[64], *p = ADF_get_guiname(a, i), *q = buf;
		while(*p) *(q++) = toupper(*(p++));
		*q = 0;
		fprintf(f, "#define %s FindGUIID(\"%s\")\n", buf, ADF_get_guiname(a, i));
	}
	for(i=0; i<ADF_get_viewcount(a); ++i) if(ADF_get_viewname(a, i)[0])
		fprintf(f, "#define %s %d\n", ADF_get_viewname(a, i), (int) i+1);

	fclose(f);
}

static void dump_old_dialogscripts(ADF *a, char *dir) {
	if(!a->old_dialogscripts) return;
	size_t i, n =a->game.dialogcount;
	for(i=0; i<n; ++i) {
		char fnbuf[512];
		snprintf(fnbuf, sizeof(fnbuf), "%s/dialogscript%03d.ads", dir, (int)i);
		fprintf(stdout, "extracting dialogscript source %s\n", fnbuf);
		FILE *f = fopen(fnbuf, "w");
		if(!f) {
			perror("fopen");
			continue;
		}
		fprintf(f, "%s", a->old_dialogscripts[i]);
		fclose(f);
	}
}

int main(int argc, char**argv) {
	int flags = 0, c;
	while ((c = getopt(argc, argv, "oblfv")) != EOF) switch(c) {
		case 'o': flags |= DISAS_DEBUG_OFFSETS; break;
		case 'b': flags |= DISAS_DEBUG_BYTECODE; break;
		case 'l': flags |= DISAS_SKIP_LINENO; break;
		case 'f': flags |= DISAS_DEBUG_FIXUPS; break;
		case 'v': flags |= DISAS_VERBOSE; break;
		default: return usage(argv[0]);
	}
	if(!argv[optind]) return usage(argv[0]);
	char *dir = argv[optind];
	char *out = argv[optind+1];
	if(!out) out = ".";
	else MKDIR(out);

	int errors = 0;
	ADF a_b, *a = &a_b;
	char fnbuf[512];
	enum ADF_open_error aoe;
	if(!ADF_find_datafile(dir, fnbuf, sizeof(fnbuf))) {
		fprintf(stderr, "failed to find datafile\n");
		return 1;
	}
	aoe = ADF_open(a, fnbuf);
	if(aoe != AOE_success && aoe <= AOE_script) {
		fprintf(stderr, "failed to open/process data file: %s\n", AOE2str(aoe));
		return 1;
	} else if (aoe != AOE_success) {
		fprintf(stderr, "warning: failed to process some non-essential parts (%s) of gamefile, probably from a newer game format\n", AOE2str(aoe));
	}
	fprintf(stdout, "info: ags engine version code %d (%.3f)\n",
	        a->version, gamefileversion2engine(a->version));
	ASI* s;
	s = ADF_get_global_script(a);
	char buf[256];
	dump_script(a->f, s, filename(out, "globalscript.o", buf, sizeof buf), flags);
	s = ADF_get_dialog_script(a);
	dump_script(a->f, s, filename(out, "dialogscript.o", buf, sizeof buf), flags);
	size_t i, l = ADF_get_scriptcount(a);
	for(i = 0; i < l; i++) {
		char fnbuf[32];
		s = ADF_get_script(a, i);
		snprintf(fnbuf, sizeof(fnbuf), "gamescript%zu.o", i);
		dump_script(a->f, s, filename(out, fnbuf, buf, sizeof buf), flags);
	}

	if(aoe == AOE_success) {
		dump_header(a, filename(out, "builtinscriptheader.ash", buf, sizeof buf));
		dump_old_dialogscripts(a, out);
	} else {
		fprintf(stderr, "skipping scriptheader and dialogscripts due to non-fatal errors\n");
	}
	ADF_close(a);
	errors += dumprooms(dir, out, flags);
	if(errors) fprintf(stderr, "agscriptxtract: got %d errors\n", errors);

	return !!errors;
}
