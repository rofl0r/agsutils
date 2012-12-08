#define _GNU_SOURCE
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

struct variable {
	char* name;
	enum varsize vs;
	unsigned offset;
};

static int add_label(AS *a, char* name, size_t insno) {
	struct label item = { .name = strdup(name), .insno = insno };
	assert(item.name);
	return List_add(a->label_list, &item);
}

static int get_label_offset(AS *a, char* name) {
	struct label item;
	size_t i = 0;
	for(; i < List_size(a->label_list); i++) {
		assert(List_get(a->label_list, i, &item));
		if(!strcmp(item.name, name))
			return item.insno;
	}
	assert(0);
	return 0;
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
	struct function_export item = { .fn = strdup(name), .instr = offset, .type = type};
	assert(item.fn);
	return List_add(a->export_list, &item);
}

static int add_fixup(AS *a, int type, size_t offset) {
	struct fixup item = {.type = type, .offset = offset};
	/* offset equals instruction number for non-DATADATA fixups */
	return List_add(a->fixup_list, &item);
}

static int add_or_get_string(AS* a, char* str) {
	/* return index of string in string table
	 * add to string table if not yet existing */
	struct string item = {.ptr = str, .len = strlen(str) }, iter;
	size_t i = 0;
	for(; i < List_size(a->string_list); i++) {
		assert(List_get(a->string_list, i, &iter));
		if(iter.len == item.len && !strcmp(iter.ptr, str)) {
			return i;
		}
	}
	item.ptr = strdup(str);
	List_add(a->string_list, &item);
	return List_size(a->string_list) -1;
}

static size_t get_string_section_length(AS* a) {
	struct string item;;
	size_t i = 0, l = 0;
	for(; i < List_size(a->string_list); i++) {
		assert(List_get(a->string_list, i, &item));
		l += item.len + 1;
	}
	return l;
}

static int add_variable(AS *a, char* name, enum varsize vs, size_t offset) {
	struct variable item = { .name = name, .vs = vs, .offset = offset };
	return List_add(a->variable_list, &item);
}

static int get_variable_offset(AS* a, char* name) {
	/* return globaldata offset of named variable */
	size_t i = 0;
	struct variable item;
	for(; i < List_size(a->variable_list); i++) {
		assert(List_get(a->variable_list, i, &item));
		if(!strcmp(item.name, name))
			return item.offset;
	}
	assert(0);
	return 0;
}

static ssize_t find_section(FILE* in, char* name) {
	char buf[1024];
	size_t off = 0, l = strlen(name);
	fseek(in, 0, SEEK_SET);
	while(fgets(buf, sizeof buf, in)) {
		off += strlen(buf);
		if(buf[0] == '.' && memcmp(name, buf + 1, l) == 0)
			return off;
	}
	return -1;
}

static int asm_data(AS* a) {
	ssize_t start = find_section(a->in, "data");
	if(start == -1) return 1; // it is valid for .s file to only have .text
	fseek(a->in, start, SEEK_SET);
	char buf[1024];
	size_t data_pos = 0;
	while(fgets(buf, sizeof buf, a->in) && buf[0] != '.' && buf[0] != '\n') {
		char* p = buf, *pend = buf + sizeof buf, *var;
		int exportflag = 0;
		enum varsize vs = vs0;
		if(*p == '#') continue;
		while(isspace(*p) && p < pend) p++;
		if(memcmp(p, "export", 6) == 0) {
			exportflag = 1;
			while(isspace(*p) && p < pend) p++;
		}
		if(memcmp(p, "int", 3) == 0)
			vs = vs4;
		else if(memcmp(p, "short", 5) == 0)
			vs = vs2;
		else if(memcmp(p, "char", 4) == 0)
			vs = vs1;
		else {
			dprintf(2, "error: expected int, short, or char\n");
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
			}
		} else {
			value = atoi(p);
			write_var:
			switch (vs) {
				case vs4:
					ByteArray_writeInt(a->data, value);
					break;
				case vs2:
					ByteArray_writeShort(a->data, value);
					break;
				case vs1:
					ByteArray_writeUnsignedByte(a->data, value);
					break;
				default:
					assert(0);
			}
		}
		if(exportflag) add_export(a, EXPORT_DATA, var, data_pos);
		add_variable(a, var, vs, data_pos);
		data_pos += (const unsigned[vsmax]) {[vs0]=0, [vs1]=1, [vs2]=2, [vs4]=4} [vs];
	}
	return 1;
}

ssize_t get_import_index(AS* a, char* name, size_t len) {
	size_t i;
	struct string item;
	for(i = 0; i < List_size(a->import_list); i++) {
		assert(List_get(a->import_list, i, &item));
		if(len == item.len && !strcmp(name, item.ptr)) return i;
	}
	return -1;
}

void add_import(AS *a, char* name) {
	size_t l = strlen(name);
	if(get_import_index(a, name, l) != -1) return;
	struct string item;
	item.ptr = strdup(name);
	item.len = l;
	List_add(a->import_list, &item);
}

static int find_export(AS *a, int type, char* name, unsigned *offset) {
	struct function_export item;
	size_t i;
	for(i = 0; i < List_size(a->export_list); i++) {
		assert(List_get(a->export_list, i, &item));
		if(item.type == type && !strcmp(name, item.fn)) {
			*offset = item.instr;
			return 1;
		}
	}
	return 0;
}

void generate_import_table(AS *a) {
	size_t i;
	struct label item;
	unsigned off;
	for(i = 0; i < List_size(a->function_ref_list); i++) {
		assert(List_get(a->function_ref_list, i, &item));
		if(!find_export(a, EXPORT_FUNCTION, item.name, &off))
			add_import(a, item.name);
	}
}

#include "ags_cpu.h"

int get_reg(char* regname) {
	int i = AR_NULL + 1;
	for(; i < AR_MAX; i++)
		if(strcmp(regnames[i], regname) == 0)
			return i;
	return AR_NULL;
}

static size_t mnemolen[SCMD_MAX];
static int mnemolen_initdone = 0;

void init_mnemolen(void) {
	size_t i = 0;
	for(; i< SCMD_MAX; i++)
		mnemolen[i] = strlen(opcodes[i].mnemonic);
	mnemolen_initdone = 1;
}

static unsigned find_insn(char* sym) {
	if(!mnemolen_initdone) init_mnemolen();
	size_t i = 0, l = strlen(sym);
	for(; i< SCMD_MAX; i++)
		if(l == mnemolen[i] && memcmp(sym, opcodes[i].mnemonic, l) == 0)
			return i;
	return 0;
}

static int asm_text(AS *a) {
	ssize_t start = find_section(a->in, "text");
	if(start == -1) return 0;
	fseek(a->in, start, SEEK_SET);
	char buf[1024];
	size_t pos = 0;
	while(fgets(buf, sizeof buf, a->in) && buf[0] != '.') {
		char* p = buf, *pend = buf + sizeof buf;
		if(*p == '#') continue;
		while(isspace(*p) && p < pend) p++;
		assert(p < pend);
		if(!*p) continue;
		char* sym = p;
		while(!isspace(*p) && p < pend) p++;
		*p = 0; p++;
		size_t l = strlen(sym);
		if(l > 1 && sym[l-1] == ':') {
			// functionstart or label
			if(memcmp(sym, "label", 5) == 0)
				add_label(a, p, pos);
			else {
				sym[l-1] = 0;
				add_export(a, EXPORT_FUNCTION, sym, pos);
				ByteArray_writeUnsignedInt(a->code, SCMD_THISBASE);
				ByteArray_writeUnsignedInt(a->code, pos);
				pos+=2;
			}
			continue;
		}
		unsigned instr = find_insn(sym);
		if(!instr) {
			dprintf(2, "error: unknown instruction %s\n", sym);
			return 0;
		}
		if(instr == SCMD_THISBASE) continue; /* we emit this instruction ourselves when a new function starts. */
			
		ByteArray_writeUnsignedInt(a->code, instr);
		pos++;
		size_t arg;
		for(arg = 0; arg < opcodes[instr].argcount; arg++) {
			sym = p;
			while(p < pend && *p != ',' && !isspace(*p)) p++; // FIXME could be a string with embedded whitespace
			assert(p < pend);
			*p = 0; p++;
			while(p < pend && isspace(*p)) p++;
			assert(p < pend);
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
						if(sym[0] == '"')
							value = add_or_get_string(a, sym);
						else if(sym[0] == '@')
							value = get_variable_offset(a, sym+1);
						else if(sym[0] == '.') {
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
						} else if(isdigit(sym[0]))
							value = atoi(sym);
						else
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
	struct label item;
	for(i = 0; i < List_size(a->label_ref_list); i++) {
		assert(List_get(a->label_ref_list, i, &item));
		ByteArray_set_position(a->code, item.insno * 4);
		int label_insno = item.insno - get_label_offset(a, item.name);
		ByteArray_writeInt(a->code, label_insno);
	}
	generate_import_table(a);
	for(i = 0; i < List_size(a->function_ref_list); i++) {
		assert(List_get(a->function_ref_list, i, &item));
		ssize_t imp = get_import_index(a, item.name, strlen(item.name));
		if(imp == -1) {
			unsigned off;
			assert(find_export(a, EXPORT_FUNCTION, item.name, &off));
			imp = off;
			add_fixup(a, FIXUP_FUNCTION, item.insno);
		} else {
			add_fixup(a, FIXUP_IMPORT, item.insno);
		}
		assert(imp != -1);
		ByteArray_set_position(a->code, item.insno * 4);
		ByteArray_writeInt(a->code, imp);
	}
	
	return 1;
}

static void write_int(FILE* o, int val) {
	fwrite(&val, 4, 1, o);
}

static void write_fixup_list(AS* a, FILE *o) {
	struct fixup item;
	size_t i;
	for(i = 0; i < List_size(a->fixup_list); i++) {
		assert(List_get(a->fixup_list, i, &item));
		char type = item.type;
		fwrite(&type, 1, 1, o);
	}
	for(i = 0; i < List_size(a->fixup_list); i++) {
		assert(List_get(a->fixup_list, i, &item));
		write_int(o, item.offset);
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
	struct function_export item;
	size_t i = 0;
	for(; i < List_size(a->export_list); i++) {
		assert(List_get(a->export_list, i, &item));
		fwrite(item.fn, strlen(item.fn) + 1, 1, o);
		unsigned encoded = (item.type << 24) | (item.instr &0x00FFFFFF);
		write_int(o, encoded);
	}
}

static void write_sections_section(AS* a, FILE *o) {
	//FIXME
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
	write_fixup_list(a, o);
	write_int(o, List_size(a->import_list));
	write_import_section(a, o);
	write_int(o, List_size(a->export_list));
	write_export_section(a, o);
	write_int(o, 0); // FIXME sectioncount
	write_sections_section(a, o);
	write_int(o, 0xbeefcafe); // magic end marker.
	fclose(o);
	return 1;
}

int AS_assemble(AS* a, char* out) {
	if(!asm_data(a)) return 0;
	if(!asm_text(a)) return 0;
	if(!write_object(a, out)) return 0;
	return 1;
}

int AS_open(AS* a, char* fn) {
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
	a->label_list = &a->label_list_b;
	a->label_ref_list = &a->label_ref_list_b;
	a->function_ref_list = &a->function_ref_list_b;
	a->variable_list = &a->variable_list_b;
	a->import_list = &a->import_list_b;
	
	List_init(a->export_list, sizeof(struct function_export));
	List_init(a->fixup_list , sizeof(struct fixup));
	List_init(a->string_list, sizeof(struct string));
	List_init(a->label_list, sizeof(struct label));
	List_init(a->label_ref_list, sizeof(struct label));
	List_init(a->function_ref_list, sizeof(struct label));
	List_init(a->variable_list, sizeof(struct variable));
	List_init(a->import_list, sizeof(struct string));

	return (a->in = fopen(fn, "r")) != 0;
}

void AS_close(AS* a) {
	fclose(a->in);
}
