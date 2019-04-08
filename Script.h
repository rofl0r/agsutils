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

int ASI_read_script(AF *a, ASI* s);
int ASI_disassemble(AF* a, ASI* s, char *fn);

#pragma RcB2 DEP "Script.c"

#endif
