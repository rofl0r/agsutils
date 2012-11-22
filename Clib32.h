#ifndef CLIB32_H
#define CLIB32_H

#include <stdio.h>
#include "./ByteArray.h"

#define MAX_FILES 10000
#define MAXMULTIFILES 25

struct MultiFileLib {
	char data_filenames[MAXMULTIFILES][20];
	size_t num_data_files;
	char filenames[MAX_FILES][25];
	unsigned offset[MAX_FILES];
	unsigned length[MAX_FILES];
	char file_datafile[MAX_FILES];        // number of datafile
	size_t num_files;
};

struct MultiFileLibNew {
	char data_filenames[MAXMULTIFILES][50];
	size_t num_data_files;
	char filenames[MAX_FILES][100];
	unsigned offset[MAX_FILES];
	unsigned length[MAX_FILES];
	char file_datafile[MAX_FILES];        // number of datafile
	size_t num_files;
};

struct AgsFile {
	struct ByteArray f;
	struct MultiFileLibNew mflib;
	int libversion;
};

int AgsFile_init(struct AgsFile *buf, char* filename);
void AgsFile_close(struct AgsFile *f);
size_t AgsFile_getCount(struct AgsFile *f);
char *AgsFile_getFileName(struct AgsFile *f, size_t index);
size_t AgsFile_getOffset(struct AgsFile *f, size_t index);
size_t AgsFile_getFileSize(struct AgsFile *f, size_t index);
int AgsFile_dump(struct AgsFile* f, size_t index, char* outfn);
int AgsFile_getVersion(struct AgsFile *f);

/*
int cliboffset(char*);
FILE *clibfopen(char *, char *);
int cliboffset(char *);
int clibfilesize(char *);
char* clibgetdatafile(char*);
int  csetlib(char*,char*);

extern long last_opened_size;
extern char lib_file_name[];
*/

//RcB: DEP "Clib32.c"

#endif
