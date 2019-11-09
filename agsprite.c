#include "File.h"
#include "SpriteFile.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include "endianness.h"
#include "BitmapFuncs.h"
#include "Targa.h"
#include <assert.h>

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

#define rle_read_col(OUT, IDX) \
	for(OUT=0, i=0; i<bpp;++i) {OUT |= (q[bpp*(IDX)+i] << (i*8));}
static unsigned rle_encode(unsigned char *data, unsigned data_size, unsigned bpp, unsigned char** result)
{
	/* worst case length: entire file consisting of sequences of 2
	   identical, and one different pixel, resulting in
	   1 byte flag + 1 pixel + 1 byte flag + 1 pixel. in the case of
	   8 bit, that's 1 byte overhead every 3 pixels. */
	unsigned char *out = malloc(data_size + 1 + (data_size/bpp/3));
	if(!out) return 0;
	unsigned char *p = out, *q = data;
	unsigned i, count = 0, togo = data_size/bpp, repcol;
	unsigned mode = 0; /* 0: stateless, 1: series: 2: repetition */
	unsigned jump_flag;
	while(1) {
		jump_flag = 0;
		unsigned col[2] = {0};
		if(togo) {
			rle_read_col(col[0], count);
			if(togo>1 && mode < 2) rle_read_col(col[1], count+1);
		} else {
			if(count) goto write_series;
			else break;
		}
		switch(mode) {
		case 0:
			if(togo>1) {
				if(col[0] == col[1]) {
		start_rep:
					mode = 2;
					repcol = col[0];
				} else {
		start_series:
					mode = 1;
				}
				count = 1;
			} else {
				goto start_series;
			}
			break;
		case 1:
			if(togo>1) {
				if(col[0] == col[1]) {
					jump_flag = 1;
					goto write_series;
				} else {
		advance:
					if(++count == 128) {
		write_series:
						*(p++) = ((mode - 1) << 7) | (count - 1);
						if(mode == 1) for(i=0;i<count*bpp;++i)
							*(p++) = *(q++);
						else {
							for(i=0;i<bpp*8;i+=8)
								*(p++) = (repcol & (0xff << i)) >> i;
							q += count * bpp;
						}
						if(!togo) goto done;
						if(jump_flag == 1) goto start_rep;
						if(jump_flag == 2) goto start_series;
						mode = 0;
						count = 0;
					}
				}
			} else goto advance;
			break;
		case 2:
			if(col[0] == repcol) goto advance;
			else {
				jump_flag = 2;
				goto write_series;
			}
		}
		togo--;
	}
done:
	*result = out;
	return p-out;
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
	unsigned char *rle_data = 0;
	unsigned rle_data_size = rle_encode(data, data_size, bpp, &rle_data);
	int use_rle = 0;
	if(rle_data && rle_data_size < data_size) {
		data_size = rle_data_size;
		data = rle_data;
		use_rle = 1;
	}
	struct TargaHeader hdr = {
		.idlength = 0,
		.colourmaptype = bpp == 1 ? 1 : 0,
		.datatypecode = bpp == 1 ?
				(use_rle ? TIT_RLE_COLOR_MAPPED : TIT_COLOR_MAPPED) :
				(use_rle ? TIT_RLE_TRUE_COLOR : TIT_TRUE_COLOR),
		.colourmaporigin = 0,
		.colourmaplength = bpp == 1 ? le16(palcount) : 0,
		.colourmapdepth = bpp == 1 ? 24 : 0,
		.x_origin = 0,
		.y_origin = le16(d->height), /* image starts at the top */
		.width = le16(d->width),
		.height = le16(d->height),
		.bitsperpixel = bpp*8,
		.imagedescriptor = 0x20, /* image starts at the top */
	};
	fwrite(&hdr, 1, sizeof hdr, f);
	unsigned tmp;
	if(bpp == 1) for(i=0; i<palcount; ++i) {
		tmp = le32(pal[i]);
		fwrite(&tmp, 1, 3, f);
	}
	fwrite(data, 1, data_size, f);
	fclose(f);
	free(paldata);
	free(rle_data);
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

	if(access(dir, R_OK) == -1 && errno == ENOENT) {
		mkdir(dir,  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	}

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
	for(i=0; i<sf.num_sprites+1; i++) {
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
