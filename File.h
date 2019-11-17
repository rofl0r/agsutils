#ifndef FILE_H
#define FILE_H
#include <stddef.h>
#include <stdio.h>
#include "ByteArray.h"

typedef struct AgsFile {
	struct ByteArray b_b;
	struct ByteArray *b;
} AF;

/* 0: error, 1: success */
int AF_open(AF *f, char* fn);
void AF_close(AF* f);

off_t AF_get_pos(AF* f);
int AF_set_pos(AF* f, off_t x);

ssize_t AF_read(AF* f, void* buf, size_t len);
int AF_read_int(AF* f);
unsigned AF_read_uint(AF* f);
short AF_read_short(AF* f);
unsigned short AF_read_ushort(AF* f);
int AF_read_string(AF* a, char* buf, size_t max);

/* dumps file contents between start and start+len into fn */
int AF_dump_chunk(AF* a, size_t start, size_t len, char* fn);
int AF_dump_chunk_stream(AF* a, size_t start, size_t len, FILE* out);
/* "swallows" or skips l bytes, i.e. advances the offset */
int AF_read_junk(AF* a, size_t l);

#pragma RcB2 DEP "File.c"

#endif
