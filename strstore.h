#include <stdlib.h>
#ifndef STRSTORE_H
#define STRSTORE_H

/*
	strstore.

	an append-only store for strings, with a list-like interface
	and fast indexing.
	by default using 32 bit uint for field indices and counters to
	keep memory usage low and caches lines well used.
	if that's not a concern, you can define STRSTORE_INDEX_TYPE to
	e.g. size_t.
	you may even define it to uint16_t if you know you'll be handling
	less than 64K worth of combined string lengths.
	all strings are owned and managed by the strstore and copied
	upon strstore_append(). memory is allocated in chunks and
	strings are kept in its own region and indices into that region
	in another.
	this is useful if you want to avoid calling strdup() over and
	over using a regular list, and don't need to delete or reorder
	items.
	strings in the store may be modified, but only if the
	modification doesn't increase the length.

	for an even more compact representation, it is possible to
	define STRSTORE_LINEAR 1, in which case the
	STRSTORE_INDEX_TYPE is irrelevant, as the store doesn't even
	allocate the index array.
	this is only useful if the string items are only ever read
	in a linear fashion.
	the caller needs to use a running counter which adds up
	the length of the currently processed entry after its use.
	in this configuration, a modification is still possible,
	but it shall not modify the length of the strings at all.

	both the index array and the string region are allocated in
	steps of at least STRSTORE_MIN_ALLOC bytes, defaulting to
	4096 (a page of memory on most systems). this also helps the
	system allocator to keep overhead low.
	this value needs to be a power of 2 and preferably exceed
	the size of the longest string you expect to add.
	the default should be good for most purposes.
	if on append the boundary of the index array needs to grow,
	the current size is doubled.
	if the string region needs to grow, the current average string
	length is used to approximately double the capacity.

	if you need to embed a strstore into a struct in a public header,
	don't include this header but simply forward-declare a pointer
	to struct strstore.

*/

#ifndef STRSTORE_INDEX_TYPE
#define STRSTORE_INDEX_TYPE unsigned int
#endif

#ifndef STRSTORE_LINEAR
#define STRSTORE_LINEAR 0
#endif

typedef struct strstore {
	size_t lcount; /* number of stored strings. */
	size_t lcapa;  /* currently allocated space for list entries, in slots */
	size_t ssize;  /* total size of all strings, including NULs */
	size_t scapa;  /* currently allocated space for strings, in bytes */
	STRSTORE_INDEX_TYPE *ldata; /* contiguous list indices */
	char *sdata;          /* contiguous string data */
} strstore;

#ifndef STRSTORE_MIN_ALLOC
#define STRSTORE_MIN_ALLOC 4096
#endif

#define strstore_new() (strstore*) calloc(1, sizeof(strstore))
#define strstore_free(SS) do { \
	free(SS->ldata); \
	free(SS->sdata); \
	free(SS); \
	SS = (void*)0; \
	} while(0)

#define strstore_count(SS) SS->lcount

#if ! STRSTORE_LINEAR
/* a pointer returned here is only valid until the next call to _add */
#define strstore_get(SS, N) (SS->sdata + SS->ldata[N])
#endif

/* using the linear API, you only fetch the pointer to the first string
   initially, then add strlen(elem)+1 to a running counter to know the
   position of the next string. calling strstore_append after having
   taken this pointer may invalidate it and cause UB. */
#define strstore_get_first(SS) (SS->sdata)


#define ALIGN(X, A) ((X+(A-1)) & -(A))

static int strstore_grow_s(struct strstore* ss, size_t ncap) {
	if(!ncap) {
		size_t alen = ss->lcount ? (ss->ssize / ss->lcount) + 1 : 16;
		ncap = 16+(ss->lcount*2*alen);
		ncap = ALIGN(ncap, STRSTORE_MIN_ALLOC);
	}
	void *d = realloc(ss->sdata, ncap);
	if(!d) return 0;
	ss->scapa = ncap;
	ss->sdata = d;
	return 1;
}

static int strstore_grow_l(struct strstore* ss, size_t ncap) {
	if(STRSTORE_LINEAR) return 1; /* no-op in linear configuration */
	if(!ncap) {
		ncap = ss->lcapa ? ss->lcapa*2*sizeof(*ss->ldata) : sizeof(*ss->ldata);
		ncap = ALIGN(ncap, STRSTORE_MIN_ALLOC);
	}
	void *d = realloc(ss->ldata, ncap);
	if(!d) return 0;
	ss->lcapa = ncap / sizeof(*ss->ldata);
	ss->ldata = d;
	return 1;
}

#undef ALIGN

/* preallocate `ns` entries of `sl` size each.
   use this if you know from the start how many items you'll need.
   set sl to your estimate of the average string length.
   using this circumvents the STRSTORE_MIN_ALLOC alignment. */
static int strstore_prealloc(struct strstore *ss, size_t ns, size_t sl) {
	if(!strstore_grow_l(ss, ns*sizeof(*ss->ldata))) return 0;
	return strstore_grow_s(ss, ns*sl);
}

/* returns length of the appended string + 1,
   0 on memory allocation failure. */
static unsigned strstore_append(struct strstore* ss, const char *s) {
	if(ss->lcount+1 >= ss->lcapa && !strstore_grow_l(ss, 0))
		return 0;
	unsigned ret;
	size_t ssn = ss->ssize;
	while(1) {
		if(ssn+1 >= ss->scapa && !strstore_grow_s(ss, 0))
			return 0;
		ss->sdata[ssn++] = *s;
		if(*s == 0) {
			if(!STRSTORE_LINEAR)
				ss->ldata[ss->lcount++] = ss->ssize;
			else
				++ss->lcount;
			ret = ssn - ss->ssize;
			ss->ssize = ssn;
			return ret;
		}
		++s;
	}
}

#endif
