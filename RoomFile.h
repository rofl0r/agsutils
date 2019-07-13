#ifndef ROOMFILE_H
#define ROOMFILE_H

#include <assert.h>
#include <stddef.h>
#include "File.h"


#define RF_FLAGS_EXTRACT_CODE (1 << 0)

struct RoomFile {
	short version;
	off_t scriptpos;
	char *sourcecode;
	unsigned sourcecode_len;
};

/* 0: error, 1: succcess. expect pointer to zero-initialized struct */
int RoomFile_read(AF *f, struct RoomFile *r, int flags);

/* this function actually isn't room specific at all, it works with all
   script containers as it looks out for the start signature. */
ssize_t ARF_find_code_start(AF* f, size_t start);


#pragma RcB2 DEP "RoomFile.c"

#endif
