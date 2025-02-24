#ifndef SCRIPT_H
#define SCRIPT_H

#include <stddef.h>
#include "File.h"

typedef struct AgsScriptInfo {
	size_t start;
	size_t len;

	size_t globaldatasize;
	size_t codesize;
	size_t stringssize;

	size_t globaldatastart;
	size_t codestart;
	size_t stringsstart;

	size_t fixupcount;
	size_t fixupstart;

	size_t importcount;
	size_t importstart;

	size_t exportcount;
	size_t exportstart;

	size_t sectioncount;
	size_t sectionstart;

	int version;
} ASI;

enum DisasmFlags {
	DISAS_DEBUG_BYTECODE = 1 << 0,
	DISAS_DEBUG_OFFSETS = 1 << 1,
	DISAS_SKIP_LINENO = 1 << 2,
	DISAS_DEBUG_FIXUPS = 1 << 3,
	DISAS_VERBOSE = 1 << 4,
};

int ASI_read_script(AF *a, ASI* s);
int ASI_disassemble(AF* a, ASI* s, char *fn, int flags);

#pragma RcB2 DEP "Script.c"

#endif
