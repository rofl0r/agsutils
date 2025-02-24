#define _GNU_SOURCE
#include "Script.h"
#include "Script_internal.h"
#include "endianness.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define COMMENT(F, FMT, ...) fprintf(F, "; " FMT, __VA_ARGS__)

static int dump_sections(AF* a, FILE *f, size_t start, size_t count) {
	if(count) {
		AF_set_pos(a, start);
		fprintf(f, ".sections\n");
		char buf[300];
		size_t i = 0;
		for(; i < count; i++) {
			if(!AF_read_string(a, buf, sizeof(buf))) return 0;
			int off = AF_read_int(a);
			fprintf(f, "\"%s\" = %d\n", buf, off);
		}
		fprintf(f, "\n");
	}
	return 1;
}

#include "StringEscape.h"
static int dump_strings(AF* a, FILE *f, struct strings *str) {
	if(!str->count) return 1;
	fprintf(f, ".%s\n", "strings");
	char *p, escapebuf[4096];
	size_t i;
	for (i=0; i<str->count; ++i) {
		escape(str->strings[i], escapebuf, sizeof(escapebuf));
		fprintf(f, "\"%s\"\n", escapebuf);
	}
	return 1;
}

static void free_fixup_data(struct fixup_data *ret) {
	size_t i;
	for(i = 0; i <= FIXUP_MAX; i++) {
		free(ret->codeindex_per[i]);
	}
	free(ret->codeindex);
	free(ret->types);
}

/* fixup_data needs to be zeroed */
static int get_fixups(AF* a, size_t start, size_t count, struct fixup_data *ret) {
	size_t i;
	if(!(ret->types = malloc(count))) goto err;
	if(!(ret->codeindex = malloc(count * sizeof(unsigned)))) goto err;

	AF_set_pos(a, start);

	if(count != (size_t) AF_read(a, ret->types, count)) goto err;
	for(i = 0; i < count; i++) {
		if(!(ret->types[i]>=0 && ret->types[i]<=FIXUP_MAX)) {
			fprintf(stderr, "error: get_fixups type assert\n");
			goto err;
		}
		ret->count[ret->types[i]]++;
		ret->codeindex[i] = AF_read_uint(a);
	}
	for(i = 0; i <= FIXUP_MAX; i++) {
		ret->codeindex_per[i] = malloc(ret->count[i] * sizeof(unsigned));
		if(!ret->codeindex_per[i]) {
			fprintf(stderr, "error: get_fixups OOM\n");
			goto err;
		}
		ret->count[i] = 0; /* reset to 0 to use as index i.t. next loop */
	}

	for(i = 0; i < count; i++) {
		ret->codeindex_per[ret->types[i]][ret->count[ret->types[i]]++] = ret->codeindex[i];
	}

	return 1;
err:
	free_fixup_data(ret);
	return 0;
}

static int dump_fixups(FILE *f, size_t count, struct fixup_data *fxd) {
	static const char* typenames[] = {
		[FIXUP_GLOBALDATA] = "FIXUP_GLOBALDATA",
		[FIXUP_FUNCTION] = "FIXUP_FUNCTION",
		[FIXUP_STRING] = "FIXUP_STRING",
		[FIXUP_IMPORT] = "FIXUP_IMPORT",
		[FIXUP_DATADATA] = "FIXUP_DATADATA",
		[FIXUP_STACK] = "FIXUP_STACK",
	};

	fprintf(f, ".%ss\n", "fixup");
	size_t i;
	for(i = 0; i < count; i++) {
		fprintf(f, "%s: %.12u\n", typenames[(int)fxd->types[i]], fxd->codeindex[i]);
	}
	return 1;
}


static int dump_import_export(AF* a, FILE *f, size_t start, size_t count, int import) {
	static const char* secnames[2] = { "export", "import" };
	const char* secname = secnames[import];
	size_t i;
	char buf[256]; /* arbitrarily chosen */
	if(!count) return 1;
	AF_set_pos(a, start);
	fprintf(f, ".%ss\n", secname);
	for(i = 0; i < count; i++) {
		if(!AF_read_string(a, buf, sizeof(buf))) return 0;
		fprintf(f, "%.12zu\"%s\"\n", i, buf);
		if(!import) {
			unsigned int addr = AF_read_uint(a);
			fprintf(f, "%d:%.12u\n", addr >> 24, addr & 0x00FFFFFF);
		}
	}
	return 1;
}

static struct export* get_exports(AF* a, size_t start, size_t count) {
	if(!count) return 0;
	struct export* fl = malloc(count * sizeof(struct export));
	AF_set_pos(a, start);
	char buf[4096]; size_t i;
	for(i = 0; i < count; i++) {
		if(!AF_read_string(a, buf, sizeof(buf))) return 0;
		fl[i].fn = strdup(buf);
		unsigned int addr = AF_read_uint(a);
		fl[i].type = addr >> 24;
		fl[i].instr = (addr & 0x00FFFFFF);
	}
	return fl;
}

static struct importlist get_imports(AF* a, size_t start, size_t count) {
	struct importlist ret = {0};
	if(!count) return ret;
	ret.names = malloc(count * sizeof(char*));
	if(!ret.names) return ret;
	AF_set_pos(a, start);
	char buf[4096]; size_t i;
	for(i = 0; i < count; i++) {
		if(!AF_read_string(a, buf, sizeof(buf))) {
			free(ret.names);
			return (struct importlist) {0};
		}
		ret.names[i] = strdup(buf);
	}
	return ret;
}

struct labels {
	unsigned count;
	unsigned *insno;
};

static int sort_comp(const void* xp, const void *yp) {
	const unsigned *x = xp, *y = yp;
	if(*x == *y) return 0;
	else if(*x > *y) return 1;
	else return -1;
}

#include "ags_cpu.h"
static struct labels get_labels(unsigned *code, size_t count) {
	struct labels ret = {0, 0};
	if(!count) return ret;
	size_t capa = 0;
	unsigned *p = 0;
	size_t insno = 0;
	unsigned insn;
	while(insno < count) {
		if(ret.count + 1 > capa) {
			capa = capa ? capa * 2 : 16;
			if(!(p = realloc(ret.insno, capa * sizeof(unsigned*)))) {
				if(ret.insno) free(ret.insno);
				ret.count = 0;
				ret.insno = 0;
				return ret;
			} else ret.insno = p;
		}
		insn = code[insno] & 0x00ffffff;
		insno++;
		int isjmp = 0;
		if(insn >= SCMD_MAX) {
			fprintf(stderr, "error: instruction number unknown\n");
			abort();
		}
		switch(insn) {
			case SCMD_JZ: case SCMD_JMP: case SCMD_JNZ:
				isjmp = 1;
			default:
				break;
		}
		size_t i = 0;
		for(; i < opcodes[insn].argcount; i++) {
			int val = code[insno];
			insno++;
			if(isjmp) {
				if((int) insno + val < 0 || insno + val >= count || code[insno + val] > SCMD_MAX) {
					fprintf(stderr, "error: label referenced from jump at %zu is out of bounds\n"
					"or points to non-instruction start code.\n", insno);
					abort();
				}
				ret.insno[ret.count] = insno + val;
				ret.count++;
			}
		}
	}
	qsort(ret.insno, ret.count, sizeof(unsigned), sort_comp);
	return ret;
}

static struct strings get_strings(AF* a, int flags, size_t start, size_t size) {
	struct strings ret = {0,0,0};
	int corrupt = 0;
	if(!size) return ret;
	if(!(ret.data = malloc(size))) return ret;
	AF_set_pos(a, start);
	if(size != (size_t) AF_read(a, ret.data, size)) {
		free1:
		free(ret.data);
		retrn:
		return ret;
	}
	size_t i, strcnt = 0;
	for(i = 0; i < size; i++) {
		if(!ret.data[i]) strcnt++;
		else if(ret.data[i] < 9 || (unsigned char) ret.data[i] > 127
		|| (ret.data[i] > 13 && ret.data[i] < 32))
			corrupt++;
	}
	if(corrupt && (flags & DISAS_VERBOSE)) {
		fprintf(stderr, "warning: %d unusual bytes in string data section, file may be corrupted\n", corrupt);
	}
	if(ret.data[size-1]) {
		fprintf(stderr, "%s", "warning: string data section doesn't end with 0, data probably corrupted\n");
		strcnt++;
		ret.data[size-1] = 0;
	}
	if(!(ret.strings = malloc(strcnt * sizeof(char*)))) goto free1;
	char* p = ret.data;
	size_t l = 0;
	strcnt = 0;

	for(i = 0; i < size; i++) {
		if(!ret.data[i]) {
			ret.strings[strcnt] = p;
			p += l + 1;
			l = 0;
			strcnt++;
		} else
			l++;
	}
	ret.count = strcnt;
	goto retrn;
}

static char* get_varname(struct export* exp, size_t expcount, unsigned globaloffset) {
	size_t i = 0;
	for(; i < expcount; i++)
		if(exp[i].type == EXPORT_DATA && exp[i].instr == globaloffset)
			return exp[i].fn;
	return 0;
}

unsigned *get_code(AF *a, size_t start, size_t count) {
	unsigned *ret = malloc(count * sizeof(unsigned));
	if(!ret) return 0;
	AF_set_pos(a, start);
	size_t i;
	for(i=0; i<count; i++)
		ret[i] = AF_read_uint(a);
	return ret;
}

static unsigned get_varsize_from_instr(unsigned* code, size_t codecount, size_t index) {
	assert(index < codecount);
	switch(code[index]) {
		case SCMD_MEMREADB: case SCMD_MEMWRITEB:
			return 1;
		case SCMD_MEMREADW: case SCMD_MEMWRITEW:
			return 2;
		case SCMD_MEMWRITEPTR:
		case SCMD_MEMREADPTR:
		case SCMD_MEMZEROPTR:
		case SCMD_MEMINITPTR:
		case SCMD_MEMREAD: case SCMD_MEMWRITE:
			return 4;
		default:
			return 0;
	}
}

struct fixup_resolved {
	unsigned code;
	unsigned offset;
};

static int fixup_cmp(const void *a, const void *b) {
	const struct fixup_resolved *u1 = a;
	const struct fixup_resolved *u2 = b;
	return (u1->code - u2->code);
}

/* assumes list members are sorted. set iter to -1 for first call */
int find_next_match(struct fixup_resolved *list, size_t nel, unsigned value, size_t *iter) {
	struct fixup_resolved comparer = {
		.code = value
	};
	if(*iter == (size_t)-1) {
		struct fixup_resolved* ret = bsearch(&comparer, list, nel, sizeof(list[0]), fixup_cmp);
		if(!ret) return 0;
		*iter = ret - list;
		while(*iter>=1 && list[*iter-1].code == value)
			--(*iter);
		return 1;
	} else {
		++(*iter);
		if(*iter < nel && list[*iter].code == value)
			return 1;
		return 0;
	}
}

static int has_datadata_fixup(unsigned gdoffset, struct fixup_data *fxd) {
	size_t i;
	for(i = 0; i < fxd->count[FIXUP_DATADATA]; i++)
		if(fxd->codeindex_per[FIXUP_DATADATA][i] == gdoffset)
			return 1;
	return 0;
}

struct varinfo find_fixup_for_globaldata(FILE *f, int flags, size_t offset,
		struct fixup_resolved *fxlist_resolved, size_t fxlist_cnt,
		unsigned* code, size_t codecount)
{

	size_t iter = (size_t)-1, x;
	struct varinfo ret = {0,0};
	while(find_next_match(fxlist_resolved, fxlist_cnt, offset, &iter)) {
		x = fxlist_resolved[iter].offset;
		if(1) {
			assert(x + 1 < codecount);
			if(1) {
				ret.numrefs++;
				unsigned oldvarsize = ret.varsize;
				ret.varsize = get_varsize_from_instr(code, codecount, x+1);
				if(!ret.varsize) switch(code[x+1]) {
				case SCMD_REGTOREG:
					// li mar, @Obj; mr ax, mar; callobj ax
					if(x+5 < codecount &&
						code[x+2] == AR_MAR &&
						code[x+4] == SCMD_CALLOBJ &&
						code[x+3] == code[x+5]) {
						ret.varsize = 4;
					}
					break;
				case SCMD_MUL:
					// li mar, @var; muli dx, 4; ptrget mar; ptrassert; dynamicbounds dx
					if(x+5 < codecount &&
						code[x+2] != AR_MAR &&
						code[x+4] == SCMD_MEMREADPTR &&
						code[x+5] == AR_MAR)
						ret.varsize = 4;
					// muli is used as an array index like:
					// muli REG, 4; add MAR, REG; PUSH/POP MAR; ...
					else if(x+11 < codecount &&
						code[x+4] == SCMD_ADDREG &&
						code[x+5] == AR_MAR &&
						code[x+6] == code[x+2] &&
						code[x+7] == SCMD_PUSHREG &&
						code[x+8] == AR_MAR &&
						code[x+9] == SCMD_POPREG &&
						code[x+10] == AR_MAR) {
							ret.varsize = get_varsize_from_instr(code, codecount, x+11);
							if(!ret.varsize &&
							    x+14 < codecount &&
							    code[x+11] == SCMD_ADDREG &&
							    code[x+12] == AR_MAR &&
							    code[x+13] != code[x+2]
							)
							/* this variation adds another reg to mar before calling ptrget */
								ret.varsize = get_varsize_from_instr(code, codecount, x+14);
							else if(!ret.varsize &&
							        x+13 < codecount &&
							        code[x+11] == SCMD_PUSHREG &&
							        code[x+12] == AR_AX)
							/* this variation pushes ax on the stack before doing a ptrget ax, which overwrites ax */
								ret.varsize = get_varsize_from_instr(code, codecount, x+13);
						}
					/* muli dx, 4; add mar, dx; add mar, cx; ptr... */
					else if(x+10 < codecount &&
						code[x+4] == SCMD_ADDREG &&
						code[x+5] == AR_MAR &&
						(code[x+6] == AR_CX || code[x+6] == AR_DX) &&
						code[x+7] == SCMD_ADDREG &&
						code[x+8] == AR_MAR &&
						(code[x+9] == AR_CX || code[x+9] == AR_DX))
							ret.varsize = get_varsize_from_instr(code, codecount, x+10);
					else if(x+7 < codecount &&
						code[x+4] == SCMD_ADDREG &&
						code[x+5] == AR_MAR &&
						code[x+6] == code[x+2]) {
						ret.varsize = get_varsize_from_instr(code, codecount, x+7);
						if(!ret.varsize &&
						   x + 9 < codecount &&
						   code[x+7] == SCMD_PUSHREG &&
						   code[x+8] == AR_AX)
							ret.varsize = get_varsize_from_instr(code, codecount, x+9);
					}

					break;
				case SCMD_ADDREG:
					// addreg is typically used on an index register into an array
					// followed by a byteread/store of the desired size
					// the index register needs to be added to mar reg
					assert(x+2 < codecount && code[x+2] == AR_MAR);
					ret.varsize = get_varsize_from_instr(code, codecount, x+4);
					// a typical method call
					if(!ret.varsize && x+8 < codecount &&
					   code[x+4] == SCMD_REGTOREG &&
					   code[x+5] == AR_MAR &&
					   code[x+7] == SCMD_CALLOBJ &&
					   code[x+6] == code[x+8])
						ret.varsize = 4;
					break;
				case SCMD_PUSHREG:
					// ptrget and similar ops are typically preceded by push mar, pop mar
					if(x+4 < codecount &&
					   code[x+2] == AR_MAR &&
					   code[x+3] == SCMD_POPREG &&
					   code[x+4] == AR_MAR) {
						ret.varsize = get_varsize_from_instr(code, codecount, x+5);
						if(!ret.varsize && x+7 < codecount &&
						   code[x+5] == SCMD_ADDREG &&
					           code[x+6] == AR_MAR)
							ret.varsize = get_varsize_from_instr(code, codecount, x+8);
					} else if(x+2 < codecount &&
						  code[x+2] == AR_AX)
						/* ptrget is sometimes preceded by push ax */
						ret.varsize = get_varsize_from_instr(code, codecount, x+3);
					break;
				}
				if(!ret.varsize) {
					if(oldvarsize) {
						/* don't bother guessing the varsize if we already determined it */
						ret.varsize = oldvarsize;
					}
					if(flags & DISAS_VERBOSE)
						fprintf(stderr, "warning: '%s' globaldata fixup on insno %zu offset %zu\n",
						opcodes[code[x+1]].mnemonic, x+1, offset);
					COMMENT(f, "warning: '%s' globaldata fixup on insno %zu offset %zu\n",
						opcodes[code[x+1]].mnemonic, x+1, offset);
				}
				if(oldvarsize != 0 && oldvarsize != ret.varsize)
					assert(0);
			}
		}
	}
	return ret;
}

static int is_all_zeroes(const char* buf, int len) {
	while(len--) {
		if(*buf != 0) return 0;
		buf++;
	}
	return 1;
}

static const char* get_varsize_typename(unsigned varsize) {
	static const char typenames[][6] = {[0]="ERR", [1]="char", [2]="short", [4]="int"};
	switch(varsize) {
		case 0: case 1: case 2: case 4:
			return typenames[varsize];
		case 200:
			return "string";
	}
	return 0;
}

static struct varinfo get_varinfo_from_code(
	int flags,
	unsigned *code, size_t codesize,
	size_t offset,
	struct fixup_data *fxd,
	struct fixup_resolved *gd_fixups_resolved,
	FILE *f)
{
		struct varinfo vi;
		if(has_datadata_fixup(offset, fxd))
			vi = (struct varinfo){0,4};
		else {
			vi = find_fixup_for_globaldata(f, flags, offset, gd_fixups_resolved, fxd->count[FIXUP_GLOBALDATA], code, codesize);
			if(vi.varsize == 0 && has_datadata_fixup(offset+200, fxd))
				vi.varsize = 200;
		}
		return vi;
}

int get_varinfo_from_exports(size_t offs, struct export *exp, size_t expcount, struct varinfo *vi)
{
	struct export *end = exp + expcount;
	for(; exp < end; ++exp)
		if(exp->instr == offs && exp->type == EXPORT_DATA) {
			vi->varsize = 1; /* unfortunately no size info is available, so we need to default to char for safety */
			return 1;
		}
	return 0;
}

static int dump_globaldata(AF *a, FILE *f, int flags,
			   size_t start, size_t size,
			   struct export* exp, size_t expcount,
			   struct fixup_data *fxd,
			   unsigned *code, size_t codesize) {
	if(!size) return 1;

	size_t fxcount = fxd->count[FIXUP_GLOBALDATA];
	struct fixup_resolved *gd_fixups_resolved = malloc(sizeof(struct fixup_resolved) * fxcount);
	if(!gd_fixups_resolved) return 0;
	size_t i;
	for(i=0; i < fxcount; i++) {
		unsigned x = fxd->codeindex_per[FIXUP_GLOBALDATA][i];
		assert(x < codesize);
		gd_fixups_resolved[i].code = code[x];
		gd_fixups_resolved[i].offset = x;
	}
	qsort(gd_fixups_resolved, fxcount, sizeof(gd_fixups_resolved[0]), fixup_cmp);

	fprintf(f, ".%s\n", "data");
	AF_set_pos(a, start);

	for(i = 0; i < size; ) {
		struct varinfo vi;
		vi = get_varinfo_from_code(flags, code, codesize, i, fxd, gd_fixups_resolved, f);
		if(vi.varsize == 0) get_varinfo_from_exports(i, exp, expcount, &vi);
		int x;
		char *comment = "";
		int is_str = 0;
		sw:
		switch(vi.varsize) {
			case 200:
				{
					off_t savepos = AF_get_pos(a);
					if(i + 204 <= size) {
						char buf[200];
						assert(200 == AF_read(a, buf, 200));
						/* read the datadata fixup content*/
						x = AF_read_int(a);
						if(x == i && is_all_zeroes(buf, 200)) {
							x = 0;
							AF_set_pos(a, savepos + 200);
							is_str = 1;
							break;
						}
					}
					AF_set_pos(a, savepos);
					vi.varsize = 0;
					goto sw;
				}
			case 4:
				x = AF_read_int(a);
				break;
			case 2:
				x = AF_read_short(a);
				break;
			case 1:
				x = ByteArray_readByte(a->b);
				break;
			case 0:
				vi.varsize = 1;
				x = ByteArray_readByte(a->b);
				if(vi.numrefs) comment = " ; warning: couldn't determine varsize, default to 1";
				else {
					comment = " ; unreferenced variable, assuming char";
					if(x) break;
					struct varinfo vi2;
					size_t j = i;
					while(++j < size) {
						vi2 = get_varinfo_from_code(flags, code, codesize, j, fxd, gd_fixups_resolved, f);
						if(vi2.varsize || vi.numrefs || get_varinfo_from_exports(j, exp, expcount, &vi2)) break;
						x = ByteArray_readByte(a->b);
						if(x) {
							ByteArray_set_position_rel(a->b, -1);
							x = 0;
							break;
						}
						++vi.varsize;
					}
				}
				break;
		}
		char* vn = get_varname(exp, expcount, i), buf[32];
		const char *tn = get_varsize_typename(vi.varsize);
		if(!tn || (vi.varsize == 200 && !is_str)) {
			snprintf(buf, sizeof buf, "char[%u]", vi.varsize);
			tn = buf;
		}
		if(has_datadata_fixup(i, fxd)) {
			if(vn) fprintf(f, "export %s %s = .data + %d%s\n", tn, vn, x, comment);
			else fprintf(f, "%s var%.6zu = .data + %d%s\n", tn, i, x, comment);
		} else {
			if(vn) fprintf(f, "export %s %s = %d%s\n", tn, vn, x, comment);
			else fprintf(f, "%s var%.6zu = %d%s\n", tn, i, x, comment);
		}
		i += vi.varsize;
	}
	free(gd_fixups_resolved);
	return 1;
}

static int disassemble_code_and_data(AF* a, ASI* s, FILE *f, int flags, struct fixup_data *fxd, struct strings *str) {
	int debugmode = getenv("AGSDEBUG") != 0;
	size_t start = s->codestart;
	size_t len = s->codesize * sizeof(unsigned);

	unsigned *code = get_code(a, s->codestart, s->codesize);

	struct export* fl = get_exports(a, s->exportstart, s->exportcount);

	dump_globaldata(a, f, flags, s->globaldatastart, s->globaldatasize, fl, s->exportcount, fxd, code, s->codesize);

	if(!len) return 1; /* its valid for a scriptfile to have no code at all */


	struct importlist il = get_imports(a, s->importstart, s->importcount);

	struct labels lbl = get_labels(code, s->codesize);

	AF_set_pos(a, start);
	fprintf(f, ".%s\n", "text");

	size_t currInstr = 0, currExp = 0, currFixup = 0, currLbl = 0;
	char *curr_func = 0;
	/* the data_data fixups appear to be glued separately onto the fixup logic,
	 * they are the only entries not sorted by instrucion number */
	while(currFixup < s->fixupcount && fxd->types[currFixup] == FIXUP_DATADATA) currFixup++;
	while(currInstr < s->codesize) {
		if(flags & DISAS_DEBUG_OFFSETS) COMMENT(f, "offset: %llu (insno %zu)\n", (long long) AF_get_pos(a), currInstr);
		unsigned regs, args, insn = AF_read_uint(a), op = insn & 0x00ffffff;
		assert(op < SCMD_MAX);
		while(currExp < s->exportcount && fl[currExp].type != EXPORT_FUNCTION)
			currExp++;
		if(currExp < s->exportcount && fl[currExp].instr == currInstr) {
			/* new function starts here */
			curr_func = fl[currExp].fn;
			char comment[64], *p = strrchr(fl[currExp].fn, '$');
			comment[0] = 0;
			if(p) {
				int n;
				if((n = atoi(p+1)) >= 100)
					sprintf(comment, " ; variadic, %d fixed args", n - 100);
				else
					sprintf(comment, " ; %d args", n);
			}
			fprintf(f, "\n%s:%s\n", curr_func, comment);
			currExp++;
		}
		if(currLbl < lbl.count) {
			if(lbl.insno[currLbl] == currInstr) {
				size_t numrefs = 0;
				while(currLbl < lbl.count && lbl.insno[currLbl] == currInstr) {
					currLbl++; numrefs++;
				}
				fprintf(f, "label%.12zu: ", currInstr);
				COMMENT(f, "inside %s, ", curr_func ? curr_func : "???");
				COMMENT(f, "referenced by %zu spots\n", numrefs);
			}
		}

		currInstr++;

		regs = opcodes[op].regcount;
		args = opcodes[op].argcount;

		if(insn == SCMD_LINENUM && (flags & DISAS_SKIP_LINENO)) {
			insn = AF_read_uint(a);
			COMMENT(f, "line %u\n", insn);
			currInstr++;
			continue;
		}

		if(flags & DISAS_DEBUG_BYTECODE) {
			unsigned char insbuf[16];
			unsigned iblen = 0, val;
			val = end_htole32(insn);
			memcpy(insbuf+iblen, &val, 4); iblen += 4;

			off_t currpos = AF_get_pos(a);

			size_t l;
			for(l = 0; l < args; l++) {
				assert(iblen+4 <= sizeof(insbuf));
				val = AF_read_uint(a);
				val = end_htole32(val);
				memcpy(insbuf+iblen, &val, 4); iblen += 4;
			}

			char printbuf[sizeof(insbuf)*2 + 1], *pb = printbuf;
			for(l = 0; l < iblen; l++, pb+=2)
				sprintf(pb, "%02x", (int) insbuf[l]);
			COMMENT(f, "%s\n", printbuf);

			AF_set_pos(a, currpos);
		}

		if(debugmode)
			fprintf(f, "%.12zu""\t%s ", currInstr - 1, opcodes[op].mnemonic);
		else
			fprintf(f, /*"%.12zu"*/"\t%s ", /*currInstr - 1, */opcodes[op].mnemonic);

		if(insn == SCMD_REGTOREG) {
			/* the "mov" instruction differs from all others in that the source comes first
			   we do not want that. */
			unsigned src, dst;
			src = AF_read_uint(a);
			currInstr++;
			dst = AF_read_uint(a);
			currInstr++;
			fprintf(f, "%s, %s\n", regnames[dst], regnames[src]);
			continue;
		}
		size_t l;
		for (l = 0; l < args; l++) {
			char escapebuf[4096];
			if(l) fprintf(f, ", ");
			insn = AF_read_uint(a);
			currInstr++;
			if((!l && regs) || (l == 1 && regs == 2))
				fprintf(f, "%s", regnames[insn]);
			else {
				while(currFixup < s->fixupcount && fxd->types[currFixup] == FIXUP_DATADATA)
					currFixup++; /* DATADATA fixups are unrelated to the code */
				if(currFixup < s->fixupcount && fxd->codeindex[currFixup] == currInstr - 1) {
					switch(fxd->types[currFixup]) {
						case FIXUP_IMPORT:
							if(debugmode)
								fprintf(f, "IMP:%s", il.names[insn]);
							else
								fprintf(f, "%s", il.names[insn]);
							break;
						case FIXUP_FUNCTION: {
							size_t x = 0;
							for(; x < s->exportcount; x++) {
								if(fl[x].type == EXPORT_FUNCTION && fl[x].instr == insn) {
									fprintf(f, "%s", fl[x].fn);
									break;
								}
							}
							break;
						}
						case FIXUP_GLOBALDATA: {
							char *vn = get_varname(fl, s->exportcount, insn);
							if(vn) fprintf(f, "@%s", vn);
							else fprintf(f, "@var%.6u", insn);
							break; }
						case FIXUP_STACK: /* it is unclear if and where those ever get generated */
							fprintf(f, ".stack + %d", insn);
							break;
						case FIXUP_STRING:
							escape(str->data + insn, escapebuf, sizeof(escapebuf));
							fprintf(f, "\"%s\"", escapebuf);
						default:
							break;
					}
					currFixup++;
				} else {
					switch(op) {
						case SCMD_JMP: case SCMD_JZ: case SCMD_JNZ:
							fprintf(f, "label%.12zu", currInstr + (int) insn);
							break;
						default:
							fprintf(f, "%d", insn);
					}
				}
			}
		}
		fprintf(f, "\n");
	}
	free (fl);
	return 1;
}

int ASI_disassemble(AF* a, ASI* s, char *fn, int flags) {
	FILE *f;
	int ret = 1;
	if((f = fopen(fn, "wb")) == 0)
		return 0;
	AF_set_pos(a, s->start);
	struct fixup_data fxd = {0};
	if(!get_fixups(a, s->fixupstart, s->fixupcount, &fxd)) return 0;
	struct strings str = get_strings(a, flags, s->stringsstart, s->stringssize);

	//if(!dump_globaldata(a, fd, s->globaldatastart, s->globaldatasize)) goto err_close;
	if(!disassemble_code_and_data(a, s, f, flags, &fxd, &str)) goto err_close;
	if(!dump_strings(a, f, &str)) goto err_close;
	if((flags & DISAS_DEBUG_FIXUPS) && !dump_fixups(f, s->fixupcount, &fxd)) goto err_close;
	if(!dump_import_export(a, f, s->importstart, s->importcount, 1)) goto err_close;
	if(!dump_import_export(a, f, s->exportstart, s->exportcount, 0)) goto err_close;
	if(!dump_sections(a, f, s->sectionstart, s->sectioncount)) goto err_close;
	ret:
	free_fixup_data(&fxd);
	fclose(f);
	return ret;
	err_close:
	ret = 0;
	goto ret;
}

int ASI_read_script(AF *a, ASI* s) {
	s->start = AF_get_pos(a);
	char sig[4];
	size_t l = 4;
	if(l != (size_t) AF_read(a, sig, l)) return 0;
	if(memcmp("SCOM", sig, 4)) {
		fprintf(stderr, "error: SCOM signature expected\n");
		return 0;
	}
	s->version = AF_read_int(a);
	s->globaldatasize = AF_read_int(a);
	s->codesize = AF_read_int(a);
	s->stringssize = AF_read_int(a);
	if(s->globaldatasize) {
		s->globaldatastart = AF_get_pos(a);
		l = s->globaldatasize;
		if(!AF_read_junk(a, l)) return 0;
	} else s->globaldatastart = 0;
	if(s->codesize) {
		s->codestart = AF_get_pos(a);
		l = s->codesize * sizeof(int);
		if(!AF_read_junk(a, l)) return 0;
	} else s->codestart = 0;
	if(s->stringssize) {
		s->stringsstart = AF_get_pos(a);
		l = s->stringssize;
		if(!AF_read_junk(a, l)) return 0;
	} else s->stringsstart = 0;
	s->fixupcount = AF_read_int(a);
	if(s->fixupcount) {
		s->fixupstart = AF_get_pos(a);
		l = s->fixupcount;
		if(!AF_read_junk(a, l)) return 0; /* fixup types */
		l *= sizeof(int);
		if(!AF_read_junk(a, l)) return 0; /* fixups */
	} else s->fixupstart = 0;
	s->importcount = AF_read_int(a);
	if(s->importcount) {
		s->importstart = AF_get_pos(a);
		char buf[300];
		size_t i = 0;
		for(; i < s->importcount; i++)
			if(!AF_read_string(a, buf, sizeof(buf))) return 0;
	} else s->importstart = 0;
	s->exportcount = AF_read_int(a);
	if(s->exportcount) {
		s->exportstart = AF_get_pos(a);
		char buf[300];
		size_t i = 0;
		for(; i < s->exportcount; i++) {
			if(!AF_read_string(a, buf, sizeof(buf))) return 0;
			AF_read_int(a); /* export_addr */
		}
	}  else s->exportstart = 0;
	s->sectionstart = 0;
	s->sectioncount = 0;
	if (s->version >= 83) {
		s->sectioncount = AF_read_int(a);
		if(s->sectioncount) {
			s->sectionstart = AF_get_pos(a);
			char buf[300];
			size_t i = 0;
			for(; i < s->sectioncount; i++) {
				if(!AF_read_string(a, buf, sizeof(buf))) return 0;
				AF_read_int(a); /* section offset */
			}
		}
	}
	if(0xbeefcafe != AF_read_uint(a)) return 0;
	s->len = AF_get_pos(a) - s->start;
	return 1;
}
