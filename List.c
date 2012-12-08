#include "List.h"
#include <string.h>
#include <stdlib.h>

void List_init(List *l, size_t itemsize) {
	memset(l, 0, sizeof *l);
	l->mem = &l->mem_b;
	mem_init(l->mem);
	l->itemsize = itemsize;
}

void List_free(List *l) {
	mem_free(l->mem);
	List_init(l, 0);
}

int List_add(List *l, void* item) {
	int ret = mem_write(l->mem, l->count * l->itemsize, item, l->itemsize);
	if(ret) l->count++;
	return ret;
}

int List_get(List *l, size_t index, void* item) {
	if(index >= l->count) return 0;
	void* src = mem_getptr(l->mem, index * l->itemsize, l->itemsize);
	if(!src) return 0;
	memcpy(item, src, l->itemsize);
	return 1;
}