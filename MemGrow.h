#ifndef MEMGROW_H
#define MEMGROW_H

#include <stddef.h>

typedef struct MemGrow {
	void* mem;
	size_t used;
	size_t capa;
} MG;

void mem_init(MG* mem);
void mem_free(MG* mem);
int mem_grow_if_needed(MG *mem, size_t newsize);
int mem_write(MG *mem, size_t offset, void* data, size_t size);
void* mem_getptr(MG* mem, size_t offset, size_t byteswanted);
void mem_set(MG* mem, void* data, size_t used, size_t allocated);
int mem_write_file(MG* mem, char* fn);

#pragma RcB2 DEP "MemGrow.c"

#endif
