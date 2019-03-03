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
	char* fn;
	char *dir;
	off_t pack_off;
};

/* generic interface */
void AgsFile_init(struct AgsFile *buf, char* filename);
void AgsFile_close(struct AgsFile *f);

/* reader interface */
int AgsFile_open(struct AgsFile *buf);

int AgsFile_getVersion(struct AgsFile *f);
size_t AgsFile_getFileCount(struct AgsFile *f);
char *AgsFile_getFileName(struct AgsFile *f, size_t index);
size_t AgsFile_getOffset(struct AgsFile *f, size_t index);
size_t AgsFile_getFileSize(struct AgsFile *f, size_t index);
int AgsFile_getFileNumber(struct AgsFile *f, size_t index);
size_t AgsFile_getDataFileCount(struct AgsFile *f);
char *AgsFile_getDataFileName(struct AgsFile *f, size_t index);
int AgsFile_dump(struct AgsFile* f, size_t index, const char* outfn);
int AgsFile_extract(struct AgsFile* f, off_t start, size_t len, const char* outfn);

/* writer interface */
// the directory containing the files passed via setFile
void AgsFile_setSourceDir(struct AgsFile *f, char* sourcedir);
void AgsFile_setVersion(struct AgsFile *f, int version);
void AgsFile_setFileCount(struct AgsFile *f, size_t count);
int AgsFile_setFile(struct AgsFile *f, size_t index, char* fn);
void AgsFile_setDataFileCount(struct AgsFile *f, size_t count);
void AgsFile_setFileNumber(struct AgsFile *f, size_t index, int number);
int AgsFile_setDataFile(struct AgsFile *f, size_t index, char* fn);
int AgsFile_write(struct AgsFile *f);

//RcB: DEP "Clib32.c"

#endif
