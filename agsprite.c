#include "File.h"
#include "SpriteFile.h"
#include <stdio.h>
#include <stdlib.h>
#include "endianness.h"
#include "BitmapFuncs.h"
#include "Targa.h"
#include <assert.h>

#define CONVERT_32_TO_24

static int lookup_palette(unsigned color, unsigned *palette, int ncols)
{
	int i;
	for(i=0; i<ncols; ++i)
		if(palette[i] == color) return i;
	return -1;
}

static int create_palette_pic(const ImageData* d, unsigned *palette, unsigned char **data)
{
	int ret = 0;
	unsigned char *p = d->data, *q = p + d->data_size;
	*data = malloc(d->width * d->height);
	unsigned char *o = *data, *e = o + (d->width * d->height);
	unsigned i;
	while(p < q && o < e) {
		unsigned col, r, g, b;
		switch(d->bytesperpixel) {
		case 2: {
			unsigned lo = *(p++);
			unsigned hi = *(p++);
			r = (hi & ~7) | (hi >> 5);
			g = ((hi & 7) << 5)  | ((lo & 224) >> 3) | ((hi & 7) >> 1);
			b = ((lo & 31) << 3) | ((lo & 24) >> 3);
			break; }
		case 3:
		case 4:
			b = *(p++);
			g = *(p++);
			r = *(p++);
			if(d->bytesperpixel == 4) ++p;
			break;
		default: assert(0);
		}
		col = r << 16 | g << 8 | b;
		int n = lookup_palette(col, palette, ret);
		if(n < 0) {
			if(ret == 256) {
				free(*data);
				*data = 0;
				return -1;
			}
			n = ret;
			palette[ret++] = col;
		}
		*(o++) = n;
	}
	return ret;
}

static void write_tga(char *name, ImageData* d, unsigned char *palette)
{
	unsigned pal[256];
	unsigned char *paldata = 0, *data = d->data;
	unsigned bpp = d->bytesperpixel;
	unsigned data_size = d->data_size;
	int palcount = 256;
	int i;

	FILE *f = fopen(name, "w");
	if(!f) {
		fprintf(stderr, "error opening %s\n", name);
		return;
	}

	if(bpp == 1) {
		if(!palette) for(i=0; i<256; ++i) pal[i] = rand();
		else for(i=0; i<256; ++i) {
			unsigned r = *(palette++);
			unsigned g = *(palette++);
			unsigned b = *(palette++);
			pal[i] = r << 16 | g << 8 | b ;
		}
	} else if((palcount = create_palette_pic(d, pal, &paldata)) > 0) {
		/* can be saved as 8 bit palettized image */
		bpp = 1;
		data = paldata;
		data_size = d->width*d->height;
	}
	struct TargaHeader hdr = {
		.idlength = 0,
		.colourmaptype = bpp == 1 ? 1 : 0,
		.datatypecode = bpp == 1 ? 1: 2,
		.colourmaporigin = 0,
		.colourmaplength = bpp == 1 ? le16(palcount) : 0,
		.colourmapdepth = bpp == 1 ? 24 : 0,
		.x_origin = 0,
		.y_origin = le16(d->height), /* image starts at the top */
		.width = le16(d->width),
		.height = le16(d->height),
#ifdef CONVERT_32_TO_24
		.bitsperpixel = bpp == 4 ? 24 : (bpp*8),
#else
		.bitsperpixel = bpp*8,
#endif
		.imagedescriptor = 0x20, /* image starts at the top */
	};
	fwrite(&hdr, 1, sizeof hdr, f);
	unsigned tmp;
	if(bpp == 1) for(i=0; i<palcount; ++i) {
		tmp = le32(pal[i]);
		fwrite(&tmp, 1, 3, f);
	}
#ifdef CONVERT_32_TO_24
	if (bpp == 4) for(i=0; i<data_size; i+=4) {
		fwrite(data+i, 1, 3, f);
	} else
#endif
		fwrite(data, 1, data_size, f);
	fclose(f);
	free(paldata);
}

static int usage(char *a) {
	printf(	"%s acsprset.spr OUTDIR\n"
		"extracts all sprites from acsprset.spr to OUTDIR\n"
		"due to the way sprite packs work, for some versions\n"
		"8 bit images are stored without palette (dynamically\n"
		"assigned during game). in such a case a random palette\n"
		"is generated.\n", a);
	return 1;
}

int main(int argc, char **argv) {

	if(argc != 3) return usage(argv[0]);

	char* file = argv[1];
	char *dir = argv[2];

	AF f;
	SpriteFile sf;
	int ret;
	ret = AF_open(&f, file);
	if(!ret) {
		fprintf(stderr, "error opening %s\n", file);
		return 1;
	}
	ret = SpriteFile_read(&f, &sf);
	if(!ret) {
		fprintf(stderr, "error reading spritefile %s\n", file);
		return 1;
	}
	int i;
	for(i=0; i<sf.num_sprites; i++) {
		ImageData d;
		if(SpriteFile_extract(&f, &sf, i, &d)) {
			char namebuf[64];
			switch(d.bytesperpixel) {
			case 1:
			case 2:
			case 3:
			case 4:
				snprintf(namebuf, sizeof namebuf, "%s/sprite%06d_%02d_%dx%d.tga", dir, i, d.bytesperpixel*8, d.width, d.height);
				write_tga(namebuf, &d, sf.palette);
			}
			free(d.data);
		}
	}
	return 0;
}
