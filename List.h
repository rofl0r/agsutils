#ifndef LIST_H
#define LIST_H

#include "MemGrow.h"
#include <stddef.h>

typedef struct List {
	MG* mem, mem_b;
	size_t count;
	size_t itemsize;
} List;

#define List_size(X) ((X)->count)

void List_init(List *l, size_t itemsize);
void List_free(List *l);
int List_add(List *l, void* item);
int List_get(List *l, size_t index, void* item);
void* List_getptr(List *l, size_t index);
void List_sort(List *l, int(*compar)(const void *, const void *));

#pragma RcB2 DEP "List.c"

#endif
