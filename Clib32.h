#ifndef CLIB32_H
#define CLIB32_H

#include <stdio.h>
#include "ByteArray.h"

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
	unsigned long long offset[MAX_FILES];
	unsigned long long length[MAX_FILES];
	char file_datafile[MAX_FILES];        // number of datafile
	size_t num_files;
};

struct strstore;
struct MultiFileLibDyn {
	struct strstore *data_filenames;
	struct strstore *filenames;
	unsigned long long *offset;
	unsigned long long *length;
	char *file_datafile;        // number of datafile
};

struct AgsFile {
	struct ByteArray f[MAXMULTIFILES];
	struct MultiFileLibDyn mflib;
	int libversion;
	char* fn;
	char *dir;
	off_t pack_off;
	const char *exestub_fn;
};

/* generic interface */
void AgsFile_init(struct AgsFile *buf, char* filename);
void AgsFile_close(struct AgsFile *f);

/* reader interface */
int AgsFile_open(struct AgsFile *buf);

int AgsFile_getVersion(struct AgsFile *f);
size_t AgsFile_getFileCount(struct AgsFile *f);
size_t AgsFile_getOffset(struct AgsFile *f, size_t index);
size_t AgsFile_getFileSize(struct AgsFile *f, size_t index);
int AgsFile_getFileNumber(struct AgsFile *f, size_t index);
size_t AgsFile_getDataFileCount(struct AgsFile *f);
/* the availability of getFileName* APIs depends upon STRSTORE_LINEAR setting
   in CLib32.c */
char *AgsFile_getFileName(struct AgsFile *f, size_t index);
char *AgsFile_getFileNameLinear(struct AgsFile *f, size_t off);
char *AgsFile_getDataFileName(struct AgsFile *f, size_t index);
char *AgsFile_getDataFileNameLinear(struct AgsFile *f, size_t off);

int AgsFile_dump(struct AgsFile* f, size_t index, const char* outfn);
int AgsFile_extract(struct AgsFile* f, int multifileno, off_t start, size_t len, const char* outfn);

/* writer interface */
// the directory containing the files passed via setFile
void AgsFile_setSourceDir(struct AgsFile *f, char* sourcedir);
void AgsFile_setVersion(struct AgsFile *f, int version);
//int AgsFile_setFile(struct AgsFile *f, size_t index, char* fn);
int AgsFile_setNumFiles(struct AgsFile *f, size_t num_files);
int AgsFile_appendFile(struct AgsFile *f, char* fn);
int AgsFile_appendDataFile(struct AgsFile *f, char* fn);
void AgsFile_setFileNumber(struct AgsFile *f, size_t index, int number);
int AgsFile_setDataFile(struct AgsFile *f, size_t index, char* fn);
void AgsFile_setExeStub(struct AgsFile *f, const char *fn);
int AgsFile_write(struct AgsFile *f);

#pragma RcB2 DEP "Clib32.c"

#endif
