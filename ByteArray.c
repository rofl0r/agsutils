/* ByteArray (C) 2012-2025 rofl0r
 *
 * licensed under the LGPL 2.1+ */

#include "ByteArray.h"
#include "debug.h"
#include "endianness.h"

#include <errno.h>
#include <stdlib.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef _WIN32
#define WINFILE_EXPORT static
#include "winfile.h"
#define OPEN(FN, FL) win_open(FN, FL)
#define READ(FD, BUF, CNT) win_read(FD, BUF, CNT)
#define LSEEK(FD, POS, WHENCE) win_lseek(FD, POS, WHENCE)
#define MMAP(ADR, LEN, PRT, FLG, FD, OFF) win_mmap(ADR, LEN, PRT, FLG, FD, OFF)
#define MUNMAP(ADR, LEN) win_munmap(ADR, LEN)
#define CLOSE(FD) win_close(FD)
#define FD_INVALID INVALID_HANDLE_VALUE
#else
#include <sys/mman.h>
#define OPEN(FN, FL) open(FN, FL)
#define READ(FD, BUF, CNT) read(FD, BUF, CNT)
#define LSEEK(FD, POS, WHENCE) lseek(FD, POS, WHENCE)
#define MMAP(ADR, LEN, PRT, FLG, FD, OFF) mmap(ADR, LEN, PRT, FLG, FD, OFF)
#define MUNMAP(ADR, LEN) munmap(ADR, LEN)
#define CLOSE(FD) close(FD)
#define FD_INVALID -1
#endif

void ByteArray_defaults(struct ByteArray* self) {
	memset(self, 0, sizeof(*self));
}

void ByteArray_ctor(struct ByteArray* self) {
	ByteArray_defaults(self);
	// original constructor code goes here
	self->readMultiByte = ByteArray_readMultiByte;
	self->readByte = ByteArray_readByte;
	self->readUnsignedByte = ByteArray_readUnsignedByte;
	self->readShort = ByteArray_readShort;
	self->readUnsignedShort = ByteArray_readUnsignedShort;
	self->readInt = ByteArray_readInt;
	self->readUnsignedInt = ByteArray_readUnsignedInt;
	self->readUnsignedLongLong = ByteArray_readUnsignedLongLong;
	self->readBytes = ByteArray_readBytes;

	self->writeInt = ByteArray_writeInt;
	self->writeUnsignedInt = ByteArray_writeUnsignedInt;
	self->writeShort = ByteArray_writeShort;
	self->writeUnsignedShort = ByteArray_writeUnsignedShort;
	self->writeByte = ByteArray_writeByte;
	self->writeUnsignedByte = ByteArray_writeUnsignedByte;;
	self->writeMem = ByteArray_writeMem;
	self->writeUTFBytes = ByteArray_writeUTFBytes;
	self->writeBytes = ByteArray_writeBytes;
	self->writeFloat = ByteArray_writeFloat;

	self->set_position = ByteArray_set_position;
	self->set_position_rel = ByteArray_set_position_rel;
	self->get_position = ByteArray_get_position;

	self->bytesAvailable = ByteArray_bytesAvailable;

	self->sys_endian = ENDIANNESS_BE ? BAE_BIG : BAE_LITTLE;
}

struct ByteArray* ByteArray_new(void) {
	struct ByteArray* self = (struct ByteArray*) malloc(sizeof(*self));
	if(self) ByteArray_ctor(self);
	return self;
}

void ByteArray_set_endian(struct ByteArray* self, enum ByteArray_Endianess endian) {
	self->endian = endian;
}

void ByteArray_set_flags(struct ByteArray *self, int flags) {
	self->flags = flags;
}

enum ByteArray_Endianess ByteArray_get_endian(struct ByteArray* self) {
	return self->endian;
}

// a real byte array clears the mem and resets
// "len" and pos to 0
// where len is equivalent to the bytes written into it
//void* mem_getptr(MG* mem, size_t offset, size_t byteswanted);
void ByteArray_clear(struct ByteArray* self) {
	fprintf(stderr, "clear called\n");
	assert_op(self->type, ==, BAT_MEMSTREAM);
	void *p = mem_getptr(&self->source_mem, 0, self->size);
	if(p) memset(p, 0, self->size);
}

void ByteArray_close(struct ByteArray* self) {
	assert_op(self->type, ==, BAT_MEMSTREAM);
	mem_free(&self->source_mem);
}

ba_off_t ByteArray_get_position(struct ByteArray* self) {
	return self->pos;
}

int ByteArray_is_eof(struct ByteArray* self) {
	return self->pos >= self->size;
}

static void seek_error() {
	perror("seek error!\n");
	assert_dbg(0);
}

static void neg_off() {
	fprintf(stderr, "negative seek attempted\n");
	assert_dbg(0);
}

static void oob(const char *fn, ba_off_t want, ba_off_t have) {
	fprintf(stderr, "%s: oob access attempted (want: %jd, have %jd)\n", fn ? fn : "<unknown>", (intmax_t) want, (intmax_t) have);
	fflush(stderr);
}

void ByteArray_set_length(struct ByteArray* self, ba_off_t len) {
	if(len > self->size) {
		oob(self->filename, len, self->size);
		return;
	}
	self->size = len;
}

ba_off_t ByteArray_get_length(struct ByteArray* self) {
	return self->size;
}

int ByteArray_set_position_rel(struct ByteArray* self, int rel) {
	if((int) self->pos + rel < 0) {
		neg_off();
		rel = -self->pos;
	}
	return ByteArray_set_position(self, self->pos + rel);
}

int ByteArray_set_position(struct ByteArray* self, ba_off_t pos) {
	if(pos == self->pos) return 1;
	if(pos > self->size) {
		oob(self->filename, pos, self->size);
		if(!(self->flags & BAF_NONFATAL_READ_OOB)) assert_dbg(0);
		return 0;
	}

	if(self->type == BAT_FILESTREAM) {
		ba_off_t ret = LSEEK(self->source_fd, pos, SEEK_SET);
		if(ret == (ba_off_t) -1) {
			seek_error();
			return 0;
		}
	}
	self->pos = pos;
	return 1;
}

static void read_error() {
	perror("read error!\n");
	assert_dbg(0);
}

static void read_error_short() {
	perror("read error (short)!\n");
	assert_dbg(0);
}

int ByteArray_open_file(struct ByteArray* self, const char* filename) {
	self->type = BAT_FILESTREAM;
	self->pos = 0;
	self->size = 0;
	self->filename = filename;
	self->source_fd = OPEN(filename, O_RDONLY|O_BINARY);
	if (self->source_fd == FD_INVALID) return 0;
	self->size = LSEEK(self->source_fd, 0, SEEK_END);
	if(self->size == -1LL) return 0;
	LSEEK(self->source_fd, 0, SEEK_SET);

	void *addr = MMAP(NULL, self->size, PROT_READ, MAP_PRIVATE, self->source_fd, 0);
	if(addr == MAP_FAILED) {
		fprintf(stderr, "warning: mmap %s failed (%s) - fd %lld, size %llu\n", filename, strerror(errno), (long long) self->source_fd, (long long) self->size);
		fprintf(stderr, "warning: falling back to slower alternative code\n");
		return 1; // fall back to traditional non-mmaped BAT_FILESTREAM
	}
	return ByteArray_open_mem(self, addr, self->size);
}

void ByteArray_close_file(struct ByteArray *self) {
	if(self->type == BAT_MEMSTREAM) {
		MUNMAP(self->source_mem.mem, self->size);
		self->source_mem.mem = 0;
	}
	CLOSE(self->source_fd);
	self->source_fd = FD_INVALID;
}

int ByteArray_open_mem(struct ByteArray* self, char* data, size_t size) {
	self->size = size;
	self->type = BAT_MEMSTREAM;
	mem_set(&self->source_mem, data, size, size);
	return 1;
}

void* ByteArray_get_mem(struct ByteArray* self, size_t offset, size_t byteswanted) {
	assert_dbg(self->type == BAT_MEMSTREAM);
	return mem_getptr(&self->source_mem, offset, byteswanted);
}

ssize_t ByteArray_readMultiByte(struct ByteArray* self, char* buffer, size_t len) {
	if(self->type == BAT_MEMSTREAM) {
		if((size_t) self->pos + len > (size_t) self->size) {
			oob("memstream" , self->pos + len, self->size);
			if(!(self->flags & BAF_NONFATAL_READ_OOB)) assert_dbg(0);
			ba_off_t slen = self->size - self->pos;
			if(slen > 0) len = slen;
			else return -1;
		}
		void *p = mem_getptr(&self->source_mem, self->pos, len);
		if(p) memcpy(buffer, p, len);
		else return -1;
	} else {
		ssize_t ret = READ(self->source_fd, buffer, len);
		if(ret == -1) {
			read_error();
			return -1;
		} else if((size_t) ret != len) {
			read_error_short();
			self->pos += len;
			return -1;
		}
	}
	self->pos += len;
	return len;
}

/* search for the next occurence of bytes starting from current position.
   return 1 if found before eof, else 0.
   if found, the array position will be set to just after the pattern. */
int ByteArray_search(struct ByteArray *self, unsigned char* bytes, size_t len) {
	size_t match = 0;
	while(!ByteArray_is_eof(self)) {
		unsigned char c = ByteArray_readByte(self);
		if(c == bytes[match]) {
			if(++match == len) return 1;
		} else match = 0;
	}
	return 0;
}

// write contents of self into dest
// if len == 0 all available bytes are used.
// self->pos is considered the start offset to use for self.
// the position in dest will not be advanced.
// the position in source will be advanced len bytes.
// returns the number of bytes written
ba_off_t ByteArray_readBytes(struct ByteArray* self, struct ByteArray *dest, ba_off_t start, ba_off_t len) {
	ba_off_t left = self->size - self->pos;
	if(len == 0) len = left;
	else if(len > left) {
		oob(self->filename, len, left);
		if(!(self->flags & BAF_NONFATAL_READ_OOB)) assert_dbg(0);
		len = left;
	}
	if(len == 0) return 0;
	else if (len > start + dest->size) {
		oob(self->filename, len, start + dest->size);
		if(!(self->flags & BAF_NONFATAL_READ_OOB)) assert_dbg(0);
		len = start + dest->size;
		if(len == 0) return 0;
	}
	if(dest->type != BAT_MEMSTREAM) {
		assert_dbg(0);
	}
	void *p = mem_getptr(&dest->source_mem, start, len);
	if(p) self->readMultiByte(self, p, len);
	else return 0;
	return len;
}

ba_off_t ByteArray_bytesAvailable(struct ByteArray* self) {
	if(self->pos < self->size) return self->size - self->pos;
	return 0;
}

unsigned long long ByteArray_readUnsignedLongLong(struct ByteArray* self) {
	union {
		unsigned long long intval;
		unsigned char charval[sizeof(unsigned long long)];
	} buf;
	self->readMultiByte(self, (char*) buf.charval, 8);
	if(self->endian != self->sys_endian) {
		buf.intval = end_bswap64(buf.intval);
	}
	return buf.intval;
}

unsigned int ByteArray_readUnsignedInt(struct ByteArray* self) {
	union {
		unsigned int intval;
		unsigned char charval[sizeof(unsigned int)];
	} buf;
	self->readMultiByte(self, (char*) buf.charval, 4);
	if(self->endian != self->sys_endian) {
		buf.intval = end_bswap32(buf.intval);
	}
	return buf.intval;
}

int ByteArray_readInt(struct ByteArray* self) {
	union {
		unsigned int intval;
		unsigned char charval[sizeof(unsigned int)];
	} buf;
	self->readMultiByte(self, (char*) buf.charval, 4);
	if(self->endian != self->sys_endian) {
		buf.intval = end_bswap32(buf.intval);
	}
	return buf.intval;
}

unsigned short ByteArray_readUnsignedShort(struct ByteArray* self) {
	union {
		unsigned short intval;
		unsigned char charval[sizeof(unsigned short)];
	} buf;
	self->readMultiByte(self, (char*) buf.charval, 2);
	if(self->endian != self->sys_endian) {
		buf.intval = end_bswap16(buf.intval);
	}
	return buf.intval;
}

short ByteArray_readShort(struct ByteArray* self) {
	union {
		unsigned short intval;
		unsigned char charval[sizeof(unsigned short)];
	} buf;
	self->readMultiByte(self, (char*) buf.charval, 2);
	if(self->endian != self->sys_endian) {
		buf.intval = end_bswap16(buf.intval);
	}
	return buf.intval;
}

unsigned char ByteArray_readUnsignedByte(struct ByteArray* self) {
	union {
		unsigned char intval;
	} buf;
	if(self->readMultiByte(self, (char*) &buf.intval, 1) == 1)
		return buf.intval;
	return -1;
}

signed char ByteArray_readByte(struct ByteArray* self) {
	union {
		signed char intval;
	} buf;
	if(self->readMultiByte(self, (char*) &buf.intval, 1) == 1)
		return buf.intval;
	return -1;
}

/* equivalent to foo = self[x]; (pos stays unchanged) */
unsigned char ByteArray_getUnsignedByte(struct ByteArray* self, ba_off_t index) {
	//assert_op(self->type, ==, BAT_MEMSTREAM);
	assert_op(index, <, self->size);
	ba_off_t save = self->pos;
	unsigned char res;
	ByteArray_set_position(self, index);
	res = ByteArray_readUnsignedByte(self);
	ByteArray_set_position(self, save);
	return res;
}

/* equivalent to self[x] = what (pos stays unchanged) */
void ByteArray_setUnsignedByte(struct ByteArray* self, ba_off_t index, unsigned char what) {
	ba_off_t save = self->pos;
	if(ByteArray_set_position(self, index)) {
		ByteArray_writeUnsignedByte(self, what);
		self->pos = save;
	}
}

ba_off_t ByteArray_writeByte(struct ByteArray* self, signed char what) {
	return ByteArray_writeMem(self, (unsigned char*) &what, 1);
}

ba_off_t ByteArray_writeUnsignedByte(struct ByteArray* self, unsigned char what) {
	return ByteArray_writeMem(self, (unsigned char*) &what, 1);
}

ba_off_t ByteArray_writeShort(struct ByteArray* self, signed short what) {
	union {
		short intval;
		unsigned char charval[sizeof(what)];
	} u;
	u.intval = what;
	if(self->sys_endian != self->endian) {
		u.intval = end_bswap16(u.intval);
	}
	return ByteArray_writeMem(self, u.charval, sizeof(what));
}

ba_off_t ByteArray_writeUnsignedShort(struct ByteArray* self, unsigned short what) {
	union {
		unsigned short intval;
		unsigned char charval[sizeof(what)];
	} u;
	u.intval = what;
	if(self->sys_endian != self->endian) {
		u.intval = end_bswap16(u.intval);
	}
	return ByteArray_writeMem(self, u.charval, sizeof(what));
}

ba_off_t ByteArray_writeInt(struct ByteArray* self, signed int what) {
	union {
		int intval;
		unsigned char charval[sizeof(what)];
	} u;
	u.intval = what;
	if(self->sys_endian != self->endian) {
		u.intval = end_bswap32(u.intval);
	}
	return ByteArray_writeMem(self, u.charval, sizeof(what));
}

ba_off_t ByteArray_writeUnsignedInt(struct ByteArray* self, unsigned int what) {
	union {
		unsigned int intval;
		unsigned char charval[sizeof(what)];
	} u;
	u.intval = what;
	if(self->sys_endian != self->endian) {
		u.intval = end_bswap32(u.intval);
	}
	return ByteArray_writeMem(self, u.charval, sizeof(what));
}

ba_off_t ByteArray_writeMem(struct ByteArray* self, unsigned char* what, size_t len) {
	if(self->type == BAT_FILESTREAM) {
		fprintf(stderr, "tried to write to file!\n");
		assert_dbg(0);
		return 0;
	}
	if(!(self->flags & BAF_CANGROW) && (size_t) self->pos + len > (size_t) self->size) {
		fprintf(stderr, "oob write attempted");
		assert_dbg(0);
		return 0;
	}

	if(mem_write(&self->source_mem, self->pos, what, len)) {
		self->pos += len;
		if(self->pos > self->size) self->size = self->pos; /* apparently CANGROW was used */
	}
	return len;
}

ba_off_t ByteArray_writeUTFBytes(struct ByteArray* self, char* what) {
	return ByteArray_writeMem(self, (unsigned char*) what, strlen(what));
}

// write contents of what into self
ba_off_t ByteArray_writeBytes(struct ByteArray* self, struct ByteArray* what) {
	if(what->type == BAT_FILESTREAM) {
		fprintf(stderr, "tried to write from non-memory stream\n");
		assert_dbg(0);
		return 0;
	} else {
		unsigned char* p = mem_getptr(&what->source_mem, what->pos, what->size - what->pos);
		if(p)
			return ByteArray_writeMem(self, p, what->size - what->pos);
		return -1;
	}
}

ba_off_t ByteArray_writeFloat(struct ByteArray* self, float what) {
	union {
		float floatval;
		unsigned int intval;
		unsigned char charval[sizeof(what)];
	} u;
	u.floatval = what;
	if(self->sys_endian != self->endian) {
		u.intval = end_bswap32(u.intval);
	}
	return ByteArray_writeMem(self, u.charval, sizeof(what));
}

void ByteArray_dump_to_stream(struct ByteArray* self, FILE *out) {
	assert_op(self->type, ==, BAT_MEMSTREAM);
	mem_write_stream(&self->source_mem, out);
}

void ByteArray_dump_to_file(struct ByteArray* self, char* filename) {
	assert_op(self->type, ==, BAT_MEMSTREAM);
	mem_write_file(&self->source_mem, filename);
}

