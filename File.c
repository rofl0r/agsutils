#include "File.h"

int AF_open(AF *f, const char* fn) {
	f->b = &f->b_b;
	ByteArray_ctor(f->b);
	ByteArray_set_endian(f->b, BAE_LITTLE);
	return ByteArray_open_file(f->b, fn);
}

void AF_close(AF* f) {
	ByteArray_close_file(f->b);
}

ssize_t AF_read(AF* f, void* buf, size_t len) {
	return ByteArray_readMultiByte(f->b, buf, len);
}

long long AF_read_longlong(AF* f) {
	return ByteArray_readUnsignedLongLong(f->b);
}

int AF_read_int(AF* f) {
	return ByteArray_readInt(f->b);
}

unsigned AF_read_uint(AF* f) {
	return ByteArray_readUnsignedInt(f->b);
}

short AF_read_short(AF* f) {
	return ByteArray_readShort(f->b);
}

unsigned short AF_read_ushort(AF* f) {
	return ByteArray_readUnsignedShort(f->b);
}

int AF_read_uchar(AF *f) {
	return ByteArray_readUnsignedByte(f->b);
}

off_t AF_get_pos(AF* f) {
	return ByteArray_get_position(f->b);
}

int AF_set_pos(AF* f, off_t x) {
	return ByteArray_set_position(f->b, x);
}

int AF_dump_chunk_stream(AF* a, size_t start, size_t len, FILE* out) {
	char buf[4096];
	ByteArray_set_position(a->b, start);
	while(len) {
		size_t togo = len > sizeof(buf) ? sizeof(buf) : len;
		if(togo != (size_t) ByteArray_readMultiByte(a->b, buf, togo)) {
			return 0;
		}
		len -= togo;
		char *p = buf;
		while (togo) {
			size_t n = fwrite(p, 1, togo, out);
			if(!n) return 0;
			p += n;
			togo -= n;
		}
	}
	return 1;
}

int AF_dump_chunk(AF* a, size_t start, size_t len, char* fn) {
	FILE *out = fopen(fn, "w");
	if(!out) return 0;
	int ret = AF_dump_chunk_stream(a, start, len, out);
	fclose(out);
	return ret;
}

int AF_read_junk(AF* a, size_t l) {
	/*char buf[512];
	while(l) {
		size_t togo = l > sizeof(buf) ? sizeof(buf) : l;
		if(togo != (size_t) ByteArray_readMultiByte(a->b, buf, togo)) return 0;
		l -= togo;
	}
	return 1;*/
	if(!l) return 1;
	return ByteArray_set_position(a->b, ByteArray_get_position(a->b) + l);
}

int AF_read_string(AF* a, char* buf, size_t max) {
	size_t l = 0;
	while(l < max) {
		if(ByteArray_readMultiByte(a->b, buf + l, 1) != 1)
			return 0;
		if(!buf[l]) return 1;
		l++;
	}
	return 0;
}
