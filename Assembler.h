#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "ByteArray.h"
#include "List.h"
#include <stdio.h>
#include <stddef.h>
#include "hbmap.h"

typedef struct AgsAssembler {
	struct ByteArray obj_b, *obj;
	struct ByteArray data_b, *data;
	struct ByteArray code_b, *code;
	List *export_list, export_list_b;
	List *fixup_list, fixup_list_b;
	List *string_list, string_list_b;
	List *label_ref_list, label_ref_list_b;
	List *function_ref_list, function_ref_list_b;
	List *variable_list, variable_list_b;
	List *import_list, import_list_b;
	hbmap(char*, unsigned, 128) label_map_b;
	hbmap(char*, unsigned, 128) *label_map;

	FILE* in;
} AS;

void AS_open_stream(AS* a, FILE* f);
int AS_open(AS* a, char* fn);
void AS_close(AS* a);
int AS_assemble(AS* a, char* out);

#pragma RcB2 DEP "Assembler.c"

#endif
