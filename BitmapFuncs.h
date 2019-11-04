#ifndef BITMAPFUNCS_H
#define BITMAPFUNCS_H

#include "Bitmap.h"
#include "ImageData.h"
#include "endianness.h"
#include <stdlib.h>
#include <stdio.h>

static int pad_bmp(ImageData *d) {
	unsigned stride = ((((d->width * d->bytesperpixel*8) + 31) & ~31) >> 3);
	if(stride == d->width * d->bytesperpixel) return -1;
	unsigned x,y;
	unsigned outsz = d->height * stride;
	unsigned char *out = malloc(outsz), *p = d->data, *q = out;
	if(!out) return 0;
	for(y = 0; y < d->height; ++y) {
		for(x = 0; x < d->width*d->bytesperpixel; ++x)
			*(q++) = *(p++);
		for(; x < stride; ++x)
			*(q++) = 0;
	}
	free(d->data);
	d->data = out;
	d->data_size = outsz;
	return 1;
}
static void write_bmp(char *name, ImageData *d) {
	FILE *f = fopen(name, "w");
	if(f) {
		struct BITMAPINFOHEADER_X hdr = {
			.bfType = le16(0x4D42),
			.bfSize = le32(sizeof(hdr) + d->data_size),
			.bfOffsetBits = le32(sizeof(hdr)),
			.biSize = le32(sizeof(hdr)-14),
			.biWidth = le32(d->width),
			/* negative height means bmp is stored from top to bottom */
			.biHeight = le32( - d->height),
			.biPlanes = le32(1),
			.biBitCount = le32(d->bytesperpixel * 8),
			.biCompression = 0,
			.biSizeImage = 0,
			.biXPelsPerMeter = le32(0xb11),
			.biYPelsPerMeter = le32(0xb11),
		};
		fwrite(&hdr, 1, sizeof hdr, f);
		pad_bmp(d);
		fwrite(d->data, 1, d->data_size, f);
		fclose(f);
	} else {
		fprintf(stderr, "error opening %s\n", name);
	}
}

#endif

