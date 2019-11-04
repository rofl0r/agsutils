#ifndef SPRITEFILE_H
#define SPRITEFILE_H
#include "File.h"
#include "ImageData.h"

typedef struct SpriteFile {
	short version;
	unsigned short num_sprites;
	int compressed;
	int id;
#if 0
	short *widths;
	short *heights;
#endif
	unsigned *offsets;
	unsigned char *palette;
} SpriteFile;

int SpriteFile_read(AF* f, SpriteFile *sf);

/* returns uncompressed sprite in data */
int SpriteFile_extract(AF* f, SpriteFile *sf, int spriteno, ImageData *data);

#pragma RcB2 DEP "SpriteFile.c"

#endif

