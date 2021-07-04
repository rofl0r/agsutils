#define _GNU_SOURCE
#include "endianness.h"
#include "File.h"
#include "ByteArray.h"
#include "MemGrow.h"
#include "Script_internal.h"
#include "List.h"
#include <ctype.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "Assembler.h"
#include "kw_search.h"

struct fixup {
	int type;
	unsigned offset;
};

struct string {
	size_t len;
	char* ptr;
};

struct label {
	char* name;
	unsigned insno;
};

struct sections_data {
	char *name;
	/* the offset points inside .text section to denote the start
	   of contents originating from a different file; to establish a mapping
	   between 'sourceline' statements and original file for debugging.
	   this happens when the editor converts and merges multiple dialog
	   script into a single script.
	   the number denotes the number of instructions to skip after start of
	   .text section, or using absolute offsets: (absolute_offset - offset_of_text)/4
	   example: when disassembled with -o, a dialogscript i tested looks
	   like this:
	   .text
	   ; offset: 24 (insno 0)
	   ...
	   ; offset: 27856 (insno 6958)
	   _run_dialog1$1: ; 1 args
	          sourceline 1
	   ; offset: 27864 (insno 6960)
	   ...
	   .sections
	   "__DialogScripts.asc" = 0
	   "Dialog 0" = 317
	   "Dialog 1" = 6958
	   ...
	   so _run_dialog1 starts at physical offset 27856, -24 = 27832,
	   27832/4= 6958.
	   in order to keep these offsets accurate after re-assembling (especially
	   after changes have been made, we should actually save the functionname
	   it points to, then after assembling put the so-calculated offset of
	   that same function there.
	   however since these sections are only interesting for debugging,
	   and only used (except those always at offset 0) in dialogscripts,
	   this is currently out of scope.
	*/
	unsigned offset;
};

struct variable {
	char* name;
	unsigned vs;
	unsigned offset;
};

static int add_label(AS *a, char* name, size_t insno) {
	char* tmp = strdup(name);
	return htab_insert(a->label_map, tmp, HTV_N(insno)) != 0;
}

static unsigned get_label_offset(AS *a, char* name) {
	htab_value *ret = htab_find(a->label_map, name);
	if(!ret) {
		dprintf(2, "error: label '%s' not found\n", name);
		if(strncmp(name, "label", 5)) dprintf(2, "hint: label names must start with 'label'\n");
		exit(1);
	}
	return ret->n;
}

static int add_label_ref(AS *a, char * name, size_t insno) {
	/* add reference to named label to a list. after the first pass
	 * over the code these locations have to be fixed with the offset
	 * of the label. */
	struct label item = { .name = strdup(name), .insno = insno };
	assert(item.name);
	return List_add(a->label_ref_list, &item);
}

static int add_function_ref(AS *a, char* name, size_t insno) {
	/* add reference to named function to a list. after the first pass
	 * over the code these locations have to be fixed with the offset
	 * of the label. */
	struct label item = { .name = strdup(name), .insno = insno };
	assert(item.name);
	return List_add(a->function_ref_list, &item);
}

static int add_export(AS *a, int type, char* name, size_t offset) {
	struct export item = { .fn = strdup(name), .instr = offset, .type = type};
	assert(item.fn);
	assert(List_add(a->export_list, &item));
	assert(htab_insert(a->export_map, item.fn, HTV_N(List_size(a->export_list)-1)));
	return 1;
}

static int add_fixup(AS *a, int type, size_t offset) {
	struct fixup item = {.type = type, .offset = offset};
	/* offset equals instruction number for non-DATADATA fixups */
	return List_add(a->fixup_list, &item);
}

static int add_sections_name(AS *a, char* name, int value) {
	struct sections_data item = {.name = strdup(name), .offset = value};
	return List_add(a->sections_list, &item);
}

static size_t add_or_get_string__offset(AS* a, char* str) {
	/* return offset of string in string table
	 * add to string table if not yet existing */
	str++; /* leading '"' */
	size_t l = strlen(str), o;
	l--;
	str[l] = 0; /* trailing '"' */

	htab_value *v = htab_find(a->string_offset_map, str);
	if(v) return v->n;

	struct string item = {.ptr = strdup(str), .len = l };
	o = a->string_section_length;
	assert(List_add(a->string_list, &item));
	assert(htab_insert(a->string_offset_map, item.ptr, HTV_N(o)));
	a->string_section_length += l + 1;
	return o;
}

static size_t get_string_section_length(AS* a) {
	return a->string_section_length;
}

static int add_variable(AS *a, char* name, unsigned vs, size_t offset) {
	struct variable item = { .name = strdup(name), .vs = vs, .offset = offset };
	return List_add(a->variable_list, &item);
}

static int get_variable_offset(AS* a, char* name) {
	/* return globaldata offset of named variable */
	size_t i = 0;
	struct variable *item;
	for(; i < List_size(a->variable_list); i++) {
		assert((item = List_getptr(a->variable_list, i)));
		if(!strcmp(item->name, name))
			return item->offset;
	}
	assert(0);
	return 0;
}

static ssize_t find_section(FILE* in, char* name, size_t *lineno) {
	char buf[1024];
	size_t off = 0, l = strlen(name);
	*lineno = 0;
	fseek(in, 0, SEEK_SET);
	while(fgets(buf, sizeof buf, in)) {
		*lineno = *lineno +1;
		off += strlen(buf);
		if(buf[0] == '.' && memcmp(name, buf + 1, l) == 0)
			return off;
	}
	return -1;
}

static int asm_data(AS* a) {
	size_t lineno;
	ssize_t start = find_section(a->in, "data", &lineno);
	if(start == -1) return 1; // it is valid for .s file to only have .text
	fseek(a->in, start, SEEK_SET);
	char buf[1024];
	size_t data_pos = 0;
	while(fgets(buf, sizeof buf, a->in) && buf[0] != '.') {
		if(buf[0] == '\n') continue;
		char* p = buf, *pend = buf + sizeof buf, *var;
		int exportflag = 0;
		unsigned vs = 0;
		if(*p == '#' || *p == ';') continue;
		while(isspace(*p) && p < pend) p++;
		if(!memcmp(p, "export", 6) && isspace(p[6])) {
			p += 7;
			exportflag = 1;
			while(isspace(*p) && p < pend) p++;
		}
		if(memcmp(p, "int", 3) == 0)
			vs = 4;
		else if(memcmp(p, "short", 5) == 0)
			vs = 2;
		else if(memcmp(p, "char", 4) == 0) {
			vs = 1;
			if(p[4] == '[') {
				vs = atoi(p+5);
				char *q = p+5;
				while(isdigit(*q) && q < pend) q++;
				if(vs == 0 || *q != ']') {
					dprintf(2, "error: expected number > 0 and ']' after '['\n");
					return 0;
				}
			}
			else vs = 1;
		} else if(memcmp(p, "string", 6) == 0)
			vs = 200;
		else {
			dprintf(2, "error: expected int, short, char, or string\n");
			return 0;
		}
		while(!isspace(*p) && p < pend) p++;
		while(isspace(*p) && p < pend) p++;
		var = p;
		while(!isspace(*p) && p < pend) p++;
		*p = 0; p++;
		assert(p < pend && *p == '=');
		p++; while(isspace(*p) && p < pend) p++;
		assert(p < pend);
		int value;

		if(*p == '.') {
			p++;
			if(memcmp(p, "data", 4) == 0) {
				p += 4;
				while(isspace(*p) && p < pend) p++;
				assert(p < pend && *p == '+');
				p++;
				while(isspace(*p) && p < pend) p++;
				value = atoi(p);
				add_fixup(a, FIXUP_DATADATA, data_pos);
				goto write_var;
			} else {
				dprintf(2, "error: expected \"data\"\n");
				return 0;
			}
		} else {
			value = atoi(p);
			write_var:
			switch (vs) {
				default:
					for(value = vs; value >= 10; value-=10)
						ByteArray_writeMem(a->data, (void*)"\0\0\0\0\0\0\0\0\0\0", 10);
					while(value--) ByteArray_writeUnsignedByte(a->data, 0);
					break;
				case 4:
					ByteArray_writeInt(a->data, value);
					break;
				case 2:
					ByteArray_writeShort(a->data, value);
					break;
				case 1:
					ByteArray_writeUnsignedByte(a->data, value);
					break;
			}
		}
		if(exportflag) add_export(a, EXPORT_DATA, var, data_pos);
		add_variable(a, var, vs, data_pos);
		data_pos += vs;
	}
	return 1;
}

ssize_t get_import_index(AS* a, char* name, size_t len) {
	(void) len;
	htab_value *v = htab_find(a->import_map, name);
	if(!v) return -1;
	return v->n;
}

void add_import(AS *a, char* name) {
	size_t l = strlen(name);
	if(get_import_index(a, name, l) != -1) return;
	struct string item;
	item.ptr = strdup(name);
	item.len = l;
	assert(List_add(a->import_list, &item));
	assert(htab_insert(a->import_map, item.ptr, HTV_N(List_size(a->import_list)-1)));
}

static int find_export(AS *a, int type, char* name, unsigned *offset) {
	struct export *item;
	htab_value *v = htab_find(a->export_map, name);
	if(!v) return 0;
	assert((item = List_getptr(a->export_list, v->n)));
	assert(item->type == type && !strcmp(name, item->fn));
	*offset = item->instr;
	return 1;
}

void generate_import_table(AS *a) {
	size_t i;
	struct label *item;
	unsigned off;
	for(i = 0; i < List_size(a->function_ref_list); i++) {
		assert((item = List_getptr(a->function_ref_list, i)));
		if(!find_export(a, EXPORT_FUNCTION, item->name, &off))
			add_import(a, item->name);
	}
}

static int get_reg(char* regname) {
	return kw_find_reg(regname, strlen(regname));
}

#include "StringEscape.h"
/* expects a pointer to the first char after a opening " in a string,
 * converts the string into convbuf, and returns the length of that string */
static size_t get_length_and_convert(char* x, char* end, char* convbuf, size_t convbuflen) {
	size_t result = 0;
	char* e = x + strlen(x);
	assert(e > x && e < end && *e == 0);
	e--;
	while(isspace(*e)) e--;
	if(*e != '"') return (size_t) -1;
	*e = 0;
	result = unescape(x, convbuf, convbuflen);
	return result;
}

/* sets lets char in arg to 0, and advances pointer till the next argstart */
static char* finalize_arg(char **p, char* pend, char* convbuf, size_t convbuflen) {
	if(**p == '"') {
		convbuf[0] = '"';
		size_t l= get_length_and_convert(*p + 1, pend, convbuf+1, convbuflen - 1);
		if(l == (size_t) -1) return 0;
		convbuf[l+1] = '"';
		convbuf[l+2] = 0;
		*p = 0; /* make it crash if its accessed again, since a string should always be the last arg */
		return convbuf;
	} else {
		char* ret = *p;
		while(*p < pend && **p != ',' && !isspace(**p)) (*p)++;
		assert(*p < pend);
		**p = 0; (*p)++;
		while(*p < pend && isspace(**p)) (*p)++;
		assert(*p < pend);
		return ret;
	}
}

static int asm_strings(AS *a) {
	/* add strings in .strings section, even when they're not used from .text */
	size_t lineno;
	ssize_t start = find_section(a->in, "strings", &lineno);
	if(start == -1) return 1;
	fseek(a->in, start, SEEK_SET);
	char buf[1024];
	while(fgets(buf, sizeof buf, a->in) && buf[0] != '.') {
		char* p = buf;
		if(*p == '#' || *p == ';') continue;
		assert(*p == '"');
		size_t l = strlen(p);
		assert(l>1 && p[l-1] == '\n' && p[l-2] == '"');
		p[l-1] = 0;
		add_or_get_string__offset(a, p);
	}
	return 1;
}

static int asm_sections(AS *a) {
	/* add sections in .sections section */
	size_t lineno;
	ssize_t start = find_section(a->in, "sections", &lineno);
	if(start == -1) return 1;
	fseek(a->in, start, SEEK_SET);
	char buf[1024];
	while(fgets(buf, sizeof buf, a->in) && buf[0] != '.') {
		char* p = buf;
		if(strchr("#;\n\r", *p)) continue;
		assert(*p == '"');
		size_t l = strlen(p);
		assert(l>1 && p[l-1] == '\n');
		char *e = strrchr(p, '=');
		assert(e);
		char *f = e;
		while(--f > p && isspace(*f));
		assert(f > p && *f == '"');
		*f = 0;
		while(isspace(*(++e)));
		int val = atoi(e);
		add_sections_name(a, p+1, val);
	}
	return 1;
}

static int asm_text(AS *a) {
	size_t lineno;
	ssize_t start = find_section(a->in, "text", &lineno);
	if(start == -1) return 1;
	fseek(a->in, start, SEEK_SET);
	char buf[1024];
	char convbuf[sizeof(buf)]; /* to convert escaped string into non-escaped version */
	size_t pos = 0;
	while(fgets(buf, sizeof buf, a->in) && buf[0] != '.') {
		lineno++;
		char* p = buf, *pend = buf + sizeof buf;
		if(*p == '#' || *p == ';') continue;
		while(isspace(*p) && p < pend) p++;
		assert(p < pend);
		if(!*p) continue;
		char* sym = p;
		while(!isspace(*p) && p < pend) p++;
		*p = 0; p++;
		size_t l = strlen(sym);
		if(l > 1 && sym[l-1] == ':') {
			// functionstart or label
			sym[l-1] = 0;
			if(memcmp(sym, "label", 5) == 0)
				add_label(a, sym, pos);
			else {
				add_export(a, EXPORT_FUNCTION, sym, pos);
				ByteArray_writeUnsignedInt(a->code, SCMD_THISBASE);
				ByteArray_writeUnsignedInt(a->code, pos);
				pos+=2;
			}
			continue;
		}
		unsigned instr = kw_find_insn(sym, l);
		if(!instr) {
			dprintf(2, "line %zu: error: unknown instruction '%s'\n", lineno, sym);
			return 0;
		}
		if(instr == SCMD_THISBASE) continue; /* we emit this instruction ourselves when a new function starts. */
			
		ByteArray_writeUnsignedInt(a->code, instr);
		pos++;
		size_t arg;
		for(arg = 0; arg < opcodes[instr].argcount; arg++) {
			sym = finalize_arg(&p, pend, convbuf, sizeof(convbuf));
			if(sym == 0) {
				dprintf(2, "line %zu: error: expected \"\n", lineno);
				return 0;
			}
			int value = 0;
			if(arg < opcodes[instr].regcount) {
				value=get_reg(sym);
				if(instr == SCMD_REGTOREG) {
					/* fix reversed order of arguments */
					int dst = value;
					sym = p;
					while(p < pend && *p != ',' && !isspace(*p)) p++;
					assert(p < pend);
					*p = 0;
					value=get_reg(sym);
					ByteArray_writeInt(a->code, value);
					ByteArray_writeInt(a->code, dst);
					pos += 2;
					break;
				}
			} else {
				switch(instr) {
					case SCMD_LITTOREG:
						/* immediate can be function name, string, 
							* variable name, stack fixup, or numeric value */
						if(sym[0] == '"') {
							value = add_or_get_string__offset(a, sym);
							add_fixup(a, FIXUP_STRING, pos);
						} else if(sym[0] == '@') {
							value = get_variable_offset(a, sym+1);
							add_fixup(a, FIXUP_GLOBALDATA, pos);
						} else if(sym[0] == '.') {
							if(memcmp(sym+1, "stack", 5)) {
								dprintf(2, "error: expected stack\n");
								return 0;
							}
							sym += 6;
							while(isspace(*sym) && sym < pend) sym++;
							assert(sym < pend && *sym  == '+');
							sym++;
							while(isspace(*sym) && sym < pend) sym++;
							add_fixup(a, FIXUP_STACK, pos);
							value = atoi(sym);
						} else if(isdigit(sym[0]) || sym[0] == '-') {
							if(sym[0] == '-') assert(isdigit(sym[1]));
							value = atoi(sym);
						} else
							add_function_ref(a, sym, pos);
						break;
					case SCMD_JMP: case SCMD_JZ: case SCMD_JNZ:
						add_label_ref(a, sym, pos);
						break;
					default:
						value = atoi(sym);
				}
			}
			ByteArray_writeInt(a->code, value);
			pos++;
		}

	}
	size_t i;
	struct label *item;
	for(i = 0; i < List_size(a->label_ref_list); i++) {
		assert((item = List_getptr(a->label_ref_list, i)));
		ByteArray_set_position(a->code, item->insno * 4);
		int lbl = get_label_offset(a, item->name);
		assert(lbl >= 0 && lbl < pos);
		int label_insno = lbl - (item->insno+1); /* offset is calculated from next instruction */
		ByteArray_writeInt(a->code, label_insno);
	}
	generate_import_table(a);
	for(i = 0; i < List_size(a->function_ref_list); i++) {
		assert((item = List_getptr(a->function_ref_list, i)));
		ssize_t imp = get_import_index(a, item->name, strlen(item->name));
		if(imp == -1) {
			unsigned off;
			assert(find_export(a, EXPORT_FUNCTION, item->name, &off));
			imp = off;
			add_fixup(a, FIXUP_FUNCTION, item->insno);
		} else {
			add_fixup(a, FIXUP_IMPORT, item->insno);
		}
		assert(imp != -1);
		ByteArray_set_position(a->code, item->insno * 4);
		ByteArray_writeInt(a->code, imp);
	}
	
	return 1;
}

static void write_int(FILE* o, int val) {
	val = end_htole32(val);
	fwrite(&val, 4, 1, o);
}

static int fixup_comparefunc(const void *a, const void* b) {
	const struct fixup* fa = a, *fb = b;
	if(fa->type == FIXUP_DATADATA && fb->type != FIXUP_DATADATA)
		return -1;
	if(fb->type == FIXUP_DATADATA && fa->type != FIXUP_DATADATA)
		return 1;
	if(fa->offset < fb->offset) return -1;
	if(fa->offset == fb->offset) return 0;
	return 1;
}

static void sort_fixup_list(AS* a) {
	List_sort(a->fixup_list, fixup_comparefunc);
}

static void write_fixup_list(AS* a, FILE *o) {
	struct fixup *item;
	size_t i;
	for(i = 0; i < List_size(a->fixup_list); i++) {
		assert((item = List_getptr(a->fixup_list, i)));
		char type = item->type;
		fwrite(&type, 1, 1, o);
	}
	for(i = 0; i < List_size(a->fixup_list); i++) {
		assert((item = List_getptr(a->fixup_list, i)));
		write_int(o, item->offset);
	}
}

static void write_string_section(AS* a, FILE* o) {
	struct string item;
	size_t i = 0;
	for(; i < List_size(a->string_list); i++) {
		assert(List_get(a->string_list, i, &item));
		fwrite(item.ptr, item.len + 1, 1, o);
	}
}

static void write_import_section(AS* a, FILE* o) {
	struct string item;
	size_t i = 0;
	for(; i < List_size(a->import_list); i++) {
		assert(List_get(a->import_list, i, &item));
		fwrite(item.ptr, item.len + 1, 1, o);
	}
}

static void write_export_section(AS* a, FILE* o) {
	struct export item;
	size_t i = 0;
	for(; i < List_size(a->export_list); i++) {
		assert(List_get(a->export_list, i, &item));
		fwrite(item.fn, strlen(item.fn) + 1, 1, o);
		unsigned encoded = (item.type << 24) | (item.instr &0x00FFFFFF);
		write_int(o, encoded);
	}
}

static void write_sections_section(AS* a, FILE *o) {
	struct sections_data item;
	size_t i = 0;
	for(; i < List_size(a->sections_list); i++) {
		assert(List_get(a->sections_list, i, &item));
		fwrite(item.name, strlen(item.name) + 1, 1, o);
		write_int(o, item.offset);
		break; // FIXME : currently writing only first item - dialogscripts have more than one
	}
}

static int write_object(AS *a, char *out) {
	FILE *o;
	if(!(o = fopen(out, "w"))) return 0;
	fprintf(o, "SCOM");
	write_int(o, 83); //version
	write_int(o, ByteArray_get_length(a->data)); // globaldatasize
	write_int(o, ByteArray_get_length(a->code) / 4); // codesize
	write_int(o, get_string_section_length(a)); // stringssize
	size_t l = ByteArray_get_length(a->data);
	void *p;
	if(l) {
		p = mem_getptr(&a->data->source.mem, 0, l); // FIXME dont access directly, use some getter method
		assert(p);
		fwrite(p,l,1,o); // globaldata
	}
	l = ByteArray_get_length(a->code);
	if(l) {
		p = mem_getptr(&a->code->source.mem, 0, l);
		assert(p);
		fwrite(p,l,1,o); // code
	}
	write_string_section(a, o);
	write_int(o, List_size(a->fixup_list));
	sort_fixup_list(a);
	write_fixup_list(a, o);
	if(!List_size(a->import_list)) {
		/* AGS declares object files with 0 imports as invalid */
		add_import(a, "");
	}
	write_int(o, List_size(a->import_list));
	write_import_section(a, o);
	write_int(o, List_size(a->export_list));
	write_export_section(a, o);
	write_int(o, List_size(a->sections_list) ? 1 :  0); // FIXME we currently on write first section
	write_sections_section(a, o);
	write_int(o, 0xbeefcafe); // magic end marker.
	fclose(o);
	return 1;
}

int AS_assemble(AS* a, char* out) {
	if(!asm_data(a)) return 0;
	if(!asm_text(a)) return 0;
	// if(!asm_strings(a)) return 0;  // emitting unneeded strings is not necessary
	if(!asm_sections(a)) return 0;
	if(!write_object(a, out)) return 0;
	return 1;
}

void AS_open_stream(AS* a, FILE* f) {
	memset(a, 0, sizeof *a);
	a->obj = &a->obj_b;
	a->data = &a->data_b;
	a->code = &a->code_b;
	ByteArray_ctor(a->obj);
	ByteArray_open_mem(a->obj, 0, 0);
	ByteArray_ctor(a->data);
	ByteArray_set_endian(a->data, BAE_LITTLE);
	ByteArray_set_flags(a->data, BAF_CANGROW);
	ByteArray_open_mem(a->data, 0, 0);
	ByteArray_ctor(a->code);
	ByteArray_set_endian(a->code, BAE_LITTLE);
	ByteArray_set_flags(a->code, BAF_CANGROW);
	ByteArray_open_mem(a->code, 0, 0);

	a->export_list = &a->export_list_b;
	a->fixup_list  = &a->fixup_list_b;
	a->string_list = &a->string_list_b;
	a->label_ref_list = &a->label_ref_list_b;
	a->function_ref_list = &a->function_ref_list_b;
	a->variable_list = &a->variable_list_b;
	a->import_list = &a->import_list_b;
	a->sections_list = &a->sections_list_b;

	a->label_map = htab_create(128);
	a->import_map = htab_create(128);
	a->export_map = htab_create(128);
	a->string_offset_map = htab_create(128);

	List_init(a->export_list, sizeof(struct export));
	List_init(a->fixup_list , sizeof(struct fixup));
	List_init(a->string_list, sizeof(struct string));
	List_init(a->label_ref_list, sizeof(struct label));
	List_init(a->function_ref_list, sizeof(struct label));
	List_init(a->variable_list, sizeof(struct variable));
	List_init(a->import_list, sizeof(struct string));
	List_init(a->sections_list, sizeof(struct sections_data));

	a->in = f;
	a->string_section_length = 0;
	kw_init();
}

int AS_open(AS* a, char* fn) {
	FILE *f = fopen(fn, "r");
	if(!f) return 0;
	AS_open_stream(a, f);
	return 1;
}


void AS_close(AS* a) {
	fclose(a->in);
	kw_finish();
}
