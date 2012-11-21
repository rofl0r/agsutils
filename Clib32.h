#ifndef CLIB32_H
#define CLIB32_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

int cliboffset(char*);
FILE *clibfopen(char *, char *);
int cliboffset(char *);
int clibfilesize(char *);
char* clibgetdatafile(char*);
int  csetlib(char*,char*);

extern long last_opened_size;
extern char lib_file_name[];

#ifdef __cplusplus
}
#endif

#endif
