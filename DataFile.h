#ifndef DATAFILE_H
#define DATAFILE_H

#include "File.h"
#include "Script.h"

typedef struct AgsGameData {
	size_t fontcount;
	size_t cursorcount;
	size_t charactercount;
	size_t inventorycount;
	size_t viewcount;
	size_t dialogcount;
	unsigned color_depth;
	int hasdict;
} AGD;

typedef struct AgsDataFile {
	AF f_b;
	AF *f;
	int version;
	unsigned numsprites;
	size_t globalvarcount;
	AGD game;
	size_t scriptcount;
	size_t scriptstart;
	size_t scriptend;
	off_t spriteflagsstart;
	ASI globalscript;
	ASI dialogscript;
	ASI scripts[50];
} ADF;

int ADF_find_datafile(const char *dir, char *fnbuf, size_t flen);
int ADF_open(ADF* a, const char *filename);
void ADF_close(ADF* a);

ASI* ADF_open_objectfile(ADF* a, char* fn);
ASI* ADF_get_script(ADF* a, size_t index);
ASI* ADF_get_global_script(ADF* a);
ASI* ADF_get_dialog_script(ADF* a);
size_t ADF_get_scriptcount(ADF* a);
#define ADF_get_spritecount(A) (A)->numsprites
#define ADF_get_spriteflagsstart(A) (A)->spriteflagsstart

#pragma RcB2 DEP "DataFile.c"

#endif
