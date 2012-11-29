#define _GNU_SOURCE
#include "Script.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

static void dump_byte(int fd, unsigned char byte) {
	dprintf(fd, "\\x%.2x", (int) byte);
}

static int dump_section(AF* a, int fd, char *section, size_t start, size_t len, size_t align) {
	size_t i;
	if(len) {
		AF_set_pos(a, start);
		dprintf(fd, ".%s", section);
		for(i = 0; i < len; i++) {
			if(i % align == 0) dprintf(fd, "\n");
			dump_byte(fd, ByteArray_readByte(a->b));
		}
		dprintf(fd, "\n");
	}
	return 1;
}

static int dump_strings(AF* a, int fd, size_t start, size_t len) {
	if(!len) return 0;
	AF_set_pos(a, start);
	dprintf(fd, ".%s\n\"", "strings");
	char buf[4096];
	while(len) {
		size_t togo = len > sizeof(buf) ? sizeof(buf) : len;
		if(togo != (size_t) AF_read(a, buf, togo))
			return 0;
		size_t i;
		for(i = 0; i < togo; i++) {
			len --;
			if(!buf[i]) {
				dprintf(fd, "\"\n");
				if(len) dprintf(fd, "\"");
			} else 
				dprintf(fd, "%c", buf[i]);
		}
	}
	return 1;
}

struct fixup_data {
	char *types;
	unsigned *codeindex;
};

static struct fixup_data get_fixups(AF* a, size_t start, size_t count) {
	struct fixup_data ret = {0};
	size_t i;
	if(!(ret.types = malloc(count))) goto out;
	if(!(ret.codeindex = malloc(count * sizeof(unsigned)))) goto err1;
	
	AF_set_pos(a, start);

	if(count != (size_t) AF_read(a, ret.types, count)) goto err2;
	for(i = 0; i < count; i++)
		ret.codeindex[i] = AF_read_uint(a);

	out:
	return ret;
	err2:
	free(ret.codeindex);
	ret.codeindex = 0;
	err1:
	free(ret.types);
	ret.types = 0;
	goto out;
}

#define FIXUP_GLOBALDATA  1     // code[fixup] += &globaldata[0]
#define FIXUP_FUNCTION    2     // code[fixup] += &code[0]
#define FIXUP_STRING      3     // code[fixup] += &strings[0]
#define FIXUP_IMPORT      4     // code[fixup] = &imported_thing[code[fixup]]
#define FIXUP_DATADATA    5     // globaldata[fixup] += &globaldata[0]
#define FIXUP_STACK       6     // code[fixup] += &stack[0]
static int dump_fixups(AF* a, int fd, size_t start, size_t count) {
	static const char* typenames[] = {
		[FIXUP_GLOBALDATA] = "FIXUP_GLOBALDATA",
		[FIXUP_FUNCTION] = "FIXUP_FUNCTION",
		[FIXUP_STRING] = "FIXUP_STRING",
		[FIXUP_IMPORT] = "FIXUP_IMPORT",
		[FIXUP_DATADATA] = "FIXUP_DATADATA",
		[FIXUP_STACK] = "FIXUP_STACK",
	};
	struct fixup_data fxd = get_fixups(a, start, count);
	if(!fxd.types) return 0;
	
	dprintf(fd, ".%ss\n", "fixup");
	size_t i;
	for(i = 0; i < count; i++) {
		dprintf(fd, "%s: %.12u\n", typenames[fxd.types[i]], fxd.codeindex[i]);
	}
	return 1;
}


static int dump_import_export(AF* a, int fd, size_t start, size_t count, int import) {
	static const char* secnames[2] = { "export", "import" };
	const char* secname = secnames[import];
	size_t i;
	char buf[256]; /* arbitrarily chosen */
	if(!count) return 1;
	AF_set_pos(a, start);
	dprintf(fd, ".%ss\n", secname);
	for(i = 0; i < count; i++) {
		if(!AF_read_string(a, buf, sizeof(buf))) return 0;
		dprintf(fd, "%.12zu\"%s\"\n", i, buf);
		if(!import) {
			unsigned int addr = AF_read_uint(a);
			dprintf(fd, "%d:%.12u\n", addr >> 24, addr & 0x00FFFFFF);
		}
	}
	return 1;
}

#define EXPORT_FUNCTION 1
#define EXPORT_DATA 2
struct function_export {
	char* fn;
	unsigned instr;
	unsigned type;
};

static struct function_export* get_exports(AF* a, size_t start, size_t count) {
	if(!count) return 0;
	struct function_export* fl = malloc(count * sizeof(struct function_export));
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

struct importlist {
	char** names;
};

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
static struct labels get_labels(AF* a, size_t start, size_t count) {
	struct labels ret = {0, 0};
	if(!count) return ret;
	size_t capa = 0;
	unsigned *p = 0;
	size_t insno = 0;
	unsigned insn;
	AF_set_pos(a, start);
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
		insn = AF_read_uint(a) & 0x00ffffff;
		insno++;
		int isjmp = 0;
		switch(insn) {
			case SCMD_JZ: case SCMD_JMP: case SCMD_JNZ:
				isjmp = 1;
			default:
				break;
		}
		size_t i = 0;
		for(; i < opcodes[insn].argcount; i++) {
			int val = AF_read_int(a);
			insno++;
			if(isjmp) {
				ret.insno[ret.count] = insno + val;
				ret.count++;
			}
		}
	}
	qsort(ret.insno, ret.count, sizeof(unsigned), sort_comp);
	return ret;
}

struct strings {
	size_t count;
	char ** strings;
	char* data;
};

static struct strings get_strings(AF* a, size_t start, size_t size) {
	struct strings ret = {0,0,0};
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

static char* get_varname(struct function_export* exp, size_t expcount, unsigned globaloffset) {
	size_t i = 0;
	for(; i < expcount; i++)
		if(exp[i].type == EXPORT_DATA && exp[i].instr == globaloffset)
			return exp[i].fn;
	return 0;
}

static int dump_globaldata(AF *a, int fd, size_t start, size_t size, struct function_export* exp, size_t expcount) {
	if(!size) return 1;
	assert(size % 4 == 0);
	dprintf(fd, ".%s\n", "data");
	AF_set_pos(a, start);
	size_t i = 0, v = 0;
	for(; i < size; v++, i+=sizeof(int)) {
		int x = AF_read_int(a);
		char* vn = get_varname(exp, expcount, v * sizeof(int));
		if(vn) dprintf(fd, "export %s = %d\n", vn, x);
		else dprintf(fd, "var%.6zu = %d\n", v, x);
	}
	return 1;
}

static int disassemble_code_and_data(AF* a, ASI* s, int fd) {
	size_t start = s->codestart;
	size_t len = s->codesize * sizeof(unsigned);
	if(!len) return 0;
	
	struct function_export* fl = get_exports(a, s->exportstart, s->exportcount);
	if(!fl) return 0;
	if(!dump_globaldata(a, fd, s->globaldatastart, s->globaldatasize, fl, s->exportcount)) return 0;
	
	struct fixup_data fxd = get_fixups(a, s->fixupstart, s->fixupcount);
	if(!fxd.types) return 0; //FIXME free fl and members.
	
	struct importlist il = get_imports(a, s->importstart, s->importcount);
	
	struct labels lbl = get_labels(a, s->codestart, s->codesize);
	
	struct strings str = get_strings(a, s->stringsstart, s->stringssize);
	
	AF_set_pos(a, start);
	dprintf(fd, ".%s\n", "text");
	
	size_t currInstr = 0, currExp = 0, currFixup = 0, currLbl = 0;
	/* the data_data fixups appear to be glued separately onto the fixup logic,
	 * they are the only entries not sorted by instrucion number */
	while(currFixup < s->fixupcount && fxd.types[currFixup] == FIXUP_DATADATA) currFixup++;
	while(currInstr < s->codesize) {
		unsigned regs, args, insn = AF_read_uint(a), op = insn & 0x00ffffff;
		while(currExp < s->exportcount && fl[currExp].type != EXPORT_FUNCTION)
			currExp++;
		if(currExp < s->exportcount && fl[currExp].instr == currInstr) {
			/* new function starts here */
			dprintf(fd, "\n%s:\n", fl[currExp].fn);
			currExp++;
		}
		if(currLbl < lbl.count) {
			if(lbl.insno[currLbl] == currInstr) {
				size_t numrefs = 0;
				while(currLbl < lbl.count && lbl.insno[currLbl] == currInstr) {
					currLbl++; numrefs++;
				}
				dprintf(fd, "label%.12zu: #referenced by %zu spots\n", currInstr, numrefs);
			}
		}
		
		currInstr++;
		
		if(insn == SCMD_LINENUM) {
			insn = AF_read_uint(a);
			dprintf(fd, "# line %u\n", insn);
			currInstr++;
			continue;
		}
		regs = opcodes[op].regcount;
		args = opcodes[op].argcount;
		dprintf(fd, "%.12zu\t%s ", currInstr - 1, opcodes[op].mnemonic);
		for (size_t l = 0; l < args; l++) {
			if(l) dprintf(fd, ", ");
			insn = AF_read_uint(a);
			currInstr++;
			if((!l && regs) || (l == 1 && regs == 2))
				dprintf(fd, "%s", regnames[insn]);
			else {
				while(currFixup < s->fixupcount && fxd.types[currFixup] == FIXUP_DATADATA)
					currFixup++; /* DATADATA fixups are unrelated to the code */
				if(currFixup < s->fixupcount && fxd.codeindex[currFixup] == currInstr - 1) {
					switch(fxd.types[currFixup]) {
						case FIXUP_IMPORT:
							dprintf(fd, "%s", il.names[insn]);
							break;
						case FIXUP_FUNCTION: {
							size_t x = 0;
							for(; x < s->exportcount; x++) {
								if(fl[x].type == EXPORT_FUNCTION && fl[x].instr == insn) {
									dprintf(fd, "%s", fl[x].fn);
									break;
								}
							}
							break;
						}
						case FIXUP_GLOBALDATA:
							assert(insn % 4 == 0);
							char *vn = get_varname(fl, s->exportcount, insn);
							if(vn) dprintf(fd, "@%s", vn);
							else dprintf(fd, "@var%.6u", insn / 4);
							break;
						case FIXUP_STRING:
							dprintf(fd, "\"%s\"", str.data + insn);
						default:
							break;
					}
					currFixup++;
				} else {
					switch(op) {
						case SCMD_JMP: case SCMD_JZ: case SCMD_JNZ:
							dprintf(fd, "label%.12zu", currInstr + (int) insn);
							break;
						default:
							dprintf(fd, "%d", insn);
					}
				}
			}
		}
		dprintf(fd, "\n");
	}
	free (fl);
	return 1;
}

int ASI_disassemble(AF* a, ASI* s, char *fn) {
	int fd, ret = 1;
	if((fd = open(fn, O_CREAT | O_TRUNC | O_WRONLY, 0660)) == -1)
		return 0;
	AF_set_pos(a, s->start);
	//if(!dump_globaldata(a, fd, s->globaldatastart, s->globaldatasize)) goto err_close;
	if(!disassemble_code_and_data(a, s, fd)) goto err_close;
	if(!dump_strings(a, fd, s->stringsstart, s->stringssize)) goto err_close;
	if(!dump_fixups(a, fd, s->fixupstart, s->fixupcount)) goto err_close;
	if(!dump_import_export(a, fd, s->importstart, s->importcount, 1)) goto err_close;
	if(!dump_import_export(a, fd, s->exportstart, s->exportcount, 0)) goto err_close;
	if(!dump_section(a, fd, "sections", s->sectionstart, s->sectioncount, 32)) goto err_close;
	ret:
	close(fd);
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
	assert(memcmp("SCOM", sig, 4) == 0);
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
