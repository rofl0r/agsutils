/* (C) 2011 - 2012 rofl0r.
Permission for use by AGS is hereby granted. */

#ifndef BYTEARRAY_H
#define BYTEARRAY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include "MemGrow.h"

enum ByteArray_Endianess {
	BAE_BIG,
	BAE_LITTLE,
};

enum ByteArray_Type {
	BAT_MEMSTREAM,
	BAT_FILESTREAM,
};

enum ByteArray_Flags {
	BAF_CANGROW = 1,
	BAF_NONFATAL_READ_OOB = 2,
};

struct ByteArray {
	int type;
	int flags;
	enum ByteArray_Endianess endian;
	enum ByteArray_Endianess sys_endian;
	off_t pos;
	off_t size;
	MG source_mem;
	int source_fd;
	const char *filename;
	ssize_t (*readMultiByte)(struct ByteArray*, char*, size_t);
	unsigned long long (*readUnsignedLongLong)(struct ByteArray*);
	unsigned int (*readUnsignedInt)(struct ByteArray*);
	signed int (*readInt)(struct ByteArray*);
	unsigned short (*readUnsignedShort)(struct ByteArray*);
	signed short (*readShort)(struct ByteArray*);
	unsigned char (*readUnsignedByte)(struct ByteArray*);
	signed char (*readByte)(struct ByteArray*);
	off_t (*readBytes) (struct ByteArray* self, struct ByteArray *dest, off_t start, off_t len);
	off_t (*bytesAvailable)(struct ByteArray*);
	off_t (*writeByte) (struct ByteArray* self, signed char what);
	off_t (*writeUnsignedByte) (struct ByteArray* self, unsigned char what);
	off_t (*writeShort) (struct ByteArray* self, signed short what);
	off_t (*writeUnsignedShort) (struct ByteArray* self, unsigned short what);
	off_t (*writeInt) (struct ByteArray* self, signed int what);
	off_t (*writeUnsignedInt) (struct ByteArray* self, unsigned int what);
	off_t (*writeMem) (struct ByteArray* self, unsigned char* what, size_t len);
	off_t (*writeUTFBytes) (struct ByteArray* self, char* what);
	off_t (*writeBytes) (struct ByteArray* self, struct ByteArray* what);
	off_t (*writeFloat) (struct ByteArray* self, float what);
	int (*set_position_rel) (struct ByteArray* self, int rel);
	int (*set_position) (struct ByteArray* self, off_t pos);
	off_t (*get_position) (struct ByteArray* self);
};

void ByteArray_defaults(struct ByteArray* self);
void ByteArray_ctor(struct ByteArray* self);
struct ByteArray* ByteArray_new(void);

void ByteArray_set_endian(struct ByteArray* self, enum ByteArray_Endianess endian);
void ByteArray_set_flags(struct ByteArray *self, int flags);
enum ByteArray_Endianess ByteArray_get_endian(struct ByteArray* self);

int ByteArray_open_file(struct ByteArray* self, const char* filename);
void ByteArray_close_file(struct ByteArray *self);
int ByteArray_open_mem(struct ByteArray* self, char* data, size_t size);
void* ByteArray_get_mem(struct ByteArray* self, size_t offset, size_t byteswanted);
void ByteArray_clear(struct ByteArray* self);
void ByteArray_close(struct ByteArray* self);

void ByteArray_set_length(struct ByteArray* self, off_t len);
off_t ByteArray_get_length(struct ByteArray* self);

off_t ByteArray_get_position(struct ByteArray* self);
int ByteArray_set_position(struct ByteArray* self, off_t pos);
int ByteArray_set_position_rel(struct ByteArray* self, int rel);
off_t ByteArray_bytesAvailable(struct ByteArray* self);
int ByteArray_is_eof(struct ByteArray* self);
ssize_t ByteArray_readMultiByte(struct ByteArray* self, char* buffer, size_t len);
unsigned long long ByteArray_readUnsignedLongLong(struct ByteArray* self);
unsigned int ByteArray_readUnsignedInt(struct ByteArray* self);
int ByteArray_readInt(struct ByteArray* self);
unsigned short ByteArray_readUnsignedShort(struct ByteArray* self);
short ByteArray_readShort(struct ByteArray* self);
unsigned char ByteArray_readUnsignedByte(struct ByteArray* self);
signed char ByteArray_readByte(struct ByteArray* self);
off_t ByteArray_readBytes(struct ByteArray* self, struct ByteArray *dest, off_t start, off_t len);

off_t ByteArray_writeByte(struct ByteArray* self, signed char what);
off_t ByteArray_writeUnsignedByte(struct ByteArray* self, unsigned char what);
off_t ByteArray_writeShort(struct ByteArray* self, signed short what);
off_t ByteArray_writeUnsignedShort(struct ByteArray* self, unsigned short what);
off_t ByteArray_writeInt(struct ByteArray* self, signed int what);
off_t ByteArray_writeUnsignedInt(struct ByteArray* self, unsigned int what);
off_t ByteArray_writeFloat(struct ByteArray* self, float what);
off_t ByteArray_writeMem(struct ByteArray* self, unsigned char* what, size_t len);
off_t ByteArray_writeUTFBytes(struct ByteArray* self, char* what);
off_t ByteArray_writeBytes(struct ByteArray* self, struct ByteArray* what);

unsigned char ByteArray_getUnsignedByte(struct ByteArray* self, off_t index);
void ByteArray_setUnsignedByte(struct ByteArray* self, off_t index, unsigned char what);

int ByteArray_search(struct ByteArray *self, unsigned char* bytes, size_t len);

void ByteArray_dump_to_stream(struct ByteArray* self, FILE *out);
void ByteArray_dump_to_file(struct ByteArray* self, char* filename);

#ifdef __cplusplus
}
#endif

#pragma RcB2 DEP "ByteArray.c"

#endif
