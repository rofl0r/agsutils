#include "MemGrow.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

void mem_init(MG* mem) {
	memset(mem, 0, sizeof *mem);
}

void mem_free(MG* mem) {
	if(mem->mem) free(mem->mem);
	mem_init(mem);
}

/* returns 1 if realloc was necessary, -1 if no realloc was necessary, 0 when realloc failed */
int mem_grow_if_needed(MG *mem, size_t newsize) {
	if(newsize > mem->capa) {
		size_t nucap = mem->capa * 2;
		if(newsize > nucap) {
			nucap = newsize;
			if(nucap & (PAGE_SIZE -1)) {
				nucap += PAGE_SIZE;
				nucap &= ~(PAGE_SIZE -1);
			}
		}
		void *nu = realloc(mem->mem, nucap);
		if(!nu) return 0;
		mem->mem = nu;
		mem->capa = nucap;
		return 1;
	}
	return -1;
}

int mem_write(MG *mem, size_t offset, void* data, size_t size) {
	int ret;
	size_t needed = offset + size;
	if(needed < offset) return 0; /* overflow */
	if((ret = mem_grow_if_needed(mem, needed))) { 
		memcpy((char*) mem->mem + offset, data, size);
		if(needed > mem->used) mem->used = needed;
	}
	return ret;
}

int mem_append(MG *mem, void* data, size_t size) {
	return mem_write(mem, mem->used, data, size);
}

void* mem_getptr(MG* mem, size_t offset, size_t byteswanted) {
	if(!mem->mem || offset + byteswanted > mem->used) return 0;
	return (char*)mem->mem + offset;
}

void mem_set(MG* mem, void* data, size_t used, size_t allocated) {
	mem->mem = data;
	mem->used = used;
	mem->capa = allocated;
}

int mem_write_file(MG* mem, char* fn) {
	if(!mem->mem || !mem->used) return 0;
	int fd = open(fn, O_CREAT | O_TRUNC | O_WRONLY, 0660);
	if(fd == -1) return 0;
	int ret = (write(fd, mem->mem, mem->used) == (ssize_t) mem->used);
	close(fd);
	return ret;
}
