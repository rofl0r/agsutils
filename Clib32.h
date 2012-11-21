#ifndef CLIB32_H
#define CLIB32_H

#include <stdio.h>

int cliboffset(char*);
FILE *clibfopen(char *, char *);
int cliboffset(char *);
int clibfilesize(char *);
char* clibgetdatafile(char*);
int  csetlib(char*,char*);

extern long last_opened_size;
extern char lib_file_name[];

#endif
