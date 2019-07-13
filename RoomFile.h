#ifndef ROOMFILE_H
#define ROOMFILE_H

#include <assert.h>
#include <stddef.h>
#include "File.h"


enum RoomFileBlockType {
	BLOCKTYPE_MAIN = 1,
	BLOCKTYPE_MIN = BLOCKTYPE_MAIN,
	BLOCKTYPE_SCRIPT = 2,
	BLOCKTYPE_COMPSCRIPT = 3,
	BLOCKTYPE_COMPSCRIPT2 = 4,
	BLOCKTYPE_OBJECTNAMES = 5,
	BLOCKTYPE_ANIMBKGRND = 6,
	BLOCKTYPE_COMPSCRIPT3 = 7, /* only bytecode script type supported by released engine code */
	BLOCKTYPE_PROPERTIES = 8,
	BLOCKTYPE_OBJECTSCRIPTNAMES = 9,
	BLOCKTYPE_MAX = BLOCKTYPE_OBJECTSCRIPTNAMES,
	BLOCKTYPE_EOF = 0xFF
};

struct RoomFile {
	short version;
	off_t blockpos[BLOCKTYPE_MAX+1];
	unsigned blocklen[BLOCKTYPE_MAX+1];
};

/* 0: error, 1: succcess. expect pointer to zero-initialized struct */
int RoomFile_read(AF *f, struct RoomFile *r);
char *RoomFile_extract_source(AF *f, struct RoomFile *r, size_t *sizep);

/* this function actually isn't room specific at all, it works with all
   script containers as it looks out for the start signature. */
ssize_t ARF_find_code_start(AF* f, size_t start);


#pragma RcB2 DEP "RoomFile.c"

#endif
