#ifndef SPRITEFILE_H
#define SPRITEFILE_H
#include "File.h"
#include "ImageData.h"

typedef struct SpriteFile {
	short version;
	unsigned short num_sprites;
	int compressed;
	int id;

	unsigned *offsets;
	unsigned char *palette;

	/* private stuff */
	unsigned sc_off;

} SpriteFile;

/* read interface */

/* read TOC */
int SpriteFile_read(AF* f, SpriteFile *sf);
/* returns uncompressed sprite in data */
int SpriteFile_extract(AF* f, SpriteFile *sf, int spriteno, ImageData *data);


/* write interface */
int SpriteFile_write_header(FILE *f, SpriteFile *sf);
int SpriteFile_add(FILE *f, SpriteFile *sf, ImageData *data);
int SpriteFile_finalize(FILE* f, SpriteFile *sf);

#pragma RcB2 DEP "SpriteFile.c"

#endif

