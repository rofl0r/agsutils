#ifndef SCRIPT_INTERNAL_H
#define SCRIPT_INTERNAL_H

#include <stddef.h>

struct strings {
	size_t count;
	char ** strings;
	char* data;
};

struct importlist {
	char** names;
};

#define EXPORT_FUNCTION 1
#define EXPORT_DATA 2
struct function_export {
	char* fn;
	unsigned instr;
	unsigned type;
};

#define FIXUP_GLOBALDATA  1     // code[fixup] += &globaldata[0]
#define FIXUP_FUNCTION    2     // code[fixup] += &code[0]
#define FIXUP_STRING      3     // code[fixup] += &strings[0]
#define FIXUP_IMPORT      4     // code[fixup] = &imported_thing[code[fixup]]
#define FIXUP_DATADATA    5     // globaldata[fixup] += &globaldata[0]
#define FIXUP_STACK       6     // code[fixup] += &stack[0]
struct fixup_data {
	char *types;
	unsigned *codeindex;
};

enum varsize {vs0 = 0, vs1, vs2, vs4, vsmax};
struct varinfo {size_t numrefs; enum varsize varsize;};

#endif
