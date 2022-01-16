#ifndef TARGA_H
#define TARGA_H

/* define generic and easily accessible image type, used
   for targa conversion */

typedef struct ImageData {
	short width;
	short height;
	short bytesperpixel;
	unsigned data_size;
	unsigned char* data;
} ImageData;

#ifndef TARGA_IMPL
#define TARGA_EXPORT extern
TARGA_EXPORT int
Targa_writefile(const char *name, ImageData* d, unsigned char *palette);
TARGA_EXPORT int
Targa_readfile(const char *name, ImageData *idata, int skip_palette);

#else

struct TargaHeader {
	char  idlength;
	char  colourmaptype;
	char  datatypecode;
	short colourmaporigin;
	short colourmaplength;
	char  colourmapdepth;
	short x_origin;
	short y_origin;
	short width;
	short height;
	char  bitsperpixel;
	char  imagedescriptor;
};

#define THO(f) THO_ ## f
enum TargaHeader_offsets {
	THO(idlength) = 0,
	THO(colourmaptype) = 1,
	THO(datatypecode) = 2,
	THO(colourmaporigin) = 3,
	THO(colourmaplength) = 5,
	THO(colourmapdepth) = 7,
	THO(x_origin) = 8,
	THO(y_origin) = 10,
	THO(width) = 12,
	THO(height) = 14,
	THO(bitsperpixel) = 16,
	THO(imagedescriptor) = 17,
};

static inline uint8_t read_header_field1(unsigned char *hdr, unsigned offset) {
	return hdr[offset];
}

static inline uint16_t read_header_field2(unsigned char *hdr, unsigned offset) {
	uint16_t tmp;
	memcpy(&tmp, hdr+offset, 2);
	return end_le16toh(tmp);
}

static void TargaHeader_from_buf(struct TargaHeader *hdr, unsigned char* hdr_buf) {
	hdr->idlength = read_header_field1(hdr_buf, THO(idlength));
	hdr->colourmaptype = read_header_field1(hdr_buf, THO(colourmaptype));
	hdr->datatypecode = read_header_field1(hdr_buf, THO(datatypecode));
	hdr->colourmaporigin = read_header_field2(hdr_buf, THO(colourmaporigin));
	hdr->colourmaplength = read_header_field2(hdr_buf, THO(colourmaplength));
	hdr->colourmapdepth = read_header_field1(hdr_buf, THO(colourmapdepth));
	hdr->x_origin = read_header_field2(hdr_buf, THO(x_origin));
	hdr->y_origin = read_header_field2(hdr_buf, THO(y_origin));
	hdr->width = read_header_field2(hdr_buf, THO(width));
	hdr->height = read_header_field2(hdr_buf, THO(height));
	hdr->bitsperpixel = read_header_field1(hdr_buf, THO(bitsperpixel));
	hdr->imagedescriptor = read_header_field1(hdr_buf, THO(imagedescriptor));
}

static inline void write_header_field1(unsigned char* buf, unsigned off, uint8_t v) {
	buf[off] = v;
}
static inline void write_header_field2(unsigned char* buf, unsigned off, uint16_t v) {
	uint16_t tmp = end_htole16(v);
	memcpy(buf+off, &tmp, 2);
}

static void TargaHeader_to_buf(struct TargaHeader *hdr, unsigned char* hdr_buf) {
	write_header_field1(hdr_buf, THO(idlength), hdr->idlength);
	write_header_field1(hdr_buf, THO(colourmaptype), hdr->colourmaptype);
	write_header_field1(hdr_buf, THO(datatypecode), hdr->datatypecode);
	write_header_field2(hdr_buf, THO(colourmaporigin), hdr->colourmaporigin);
	write_header_field2(hdr_buf, THO(colourmaplength), hdr->colourmaplength);
	write_header_field1(hdr_buf, THO(colourmapdepth), hdr->colourmapdepth);
	write_header_field2(hdr_buf, THO(x_origin), hdr->x_origin);
	write_header_field2(hdr_buf, THO(y_origin), hdr->y_origin);
	write_header_field2(hdr_buf, THO(width), hdr->width);
	write_header_field2(hdr_buf, THO(height), hdr->height);
	write_header_field1(hdr_buf, THO(bitsperpixel), hdr->bitsperpixel);
	write_header_field1(hdr_buf, THO(imagedescriptor), hdr->imagedescriptor);
}

enum TargaImageType {
	TIT_COLOR_MAPPED = 1,
	TIT_TRUE_COLOR = 2,
	TIT_BLACK_WHITE = 3,
	TIT_RLE_COLOR_MAPPED = 9,
	TIT_RLE_TRUE_COLOR = 10,
	TIT_RLE_BLACK_WHITE = 11,
};

struct TargaFooter {
   unsigned extensionareaoffset;
   unsigned developerdirectoryoffset;
   char signature[16];
   char dot;
   char null;
};


#define STATIC_ASSERT(COND) static char static_assert_ ## __LINE__ [COND ? 1 : -1]
//STATIC_ASSERT(sizeof(struct TargaHeader) == 18);

#ifndef TARGA_EXPORT
#define TARGA_EXPORT static
#endif

#define TARGA_FOOTER_SIGNATURE "TRUEVISION-XFILE"


/* helper funcs */

#include <stdlib.h>
#include <stdio.h>
#include "endianness.h"

#define rle_read_col(OUT, IDX) \
	for(OUT=0, i=0; i<bpp;++i) {OUT |= (q[bpp*(IDX)+i] << (i*8));}
static unsigned rle_encode(
	unsigned char *data, unsigned data_size,
	unsigned bpp, unsigned char** result)
{
	/* worst case length: entire file consisting of sequences of 2
	   identical, and one different pixel, resulting in
	   1 byte flag + 1 pixel + 1 byte flag + 1 pixel. in the case of
	   8 bit, that's 1 byte overhead every 3 pixels. */
	unsigned char *out = malloc(data_size + 1 + (data_size/bpp/3));
	if(!out) return 0;
	unsigned char *p = out, *q = data;
	unsigned i, count = 0, togo = data_size/bpp, repcol = 0;
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
#undef rle_read_col

/* caller needs to provide result buffer of w*h*bpp size. */
static void rle_decode(
	unsigned char *data, unsigned data_size,
        unsigned bpp, unsigned char* result, unsigned result_size) {
	unsigned char
		*p = result, *p_e = p + result_size,
		*q = data, *q_e = q + data_size;
	while(q+1+bpp <= q_e) {
		unsigned count = (*q & 127) + 1;
		unsigned rep  = *q & 128;
		unsigned color = 0, i, j;
		++q;
		if(rep) {
			for(i = 0; i < bpp; ++i)
				color = (color << 8) | *(q++);
			for(i = 0; i < count && p+bpp <= p_e; ++i)
				for(j=0; j<bpp; j++)
					*(p++) = (color >> ((bpp-j-1)*8)) & 0xff;
		} else {
			for(i = 0; i < count && p+bpp <= p_e && q+bpp <= q_e; ++i)
				for(j=0; j<bpp; j++) *(p++) = *(q++);
		}
	}

}

static void rgb565_to_888(unsigned lo, unsigned hi, unsigned *r, unsigned *g, unsigned *b)
{
	*r = (hi & ~7) | (hi >> 5);
	*g = ((hi & 7) << 5)  | ((lo & 224) >> 3) | ((hi & 7) >> 1);
	*b = ((lo & 31) << 3) | ((lo & 28) >> 2);
}

static int ImageData_convert_16_to_24(ImageData *d) {
	size_t outsz = d->width*d->height*3UL;
	unsigned char *out = malloc(outsz),
	*p = out, *pe = out + outsz,
	*q = d->data;
	if(!out) return 0;
	while(p < pe) {
		unsigned r,g,b,lo,hi;
		lo = *(q++);
		hi = *(q++);
		rgb565_to_888(lo, hi, &r, &g, &b);
		*(p++) = b;
		*(p++) = g;
		*(p++) = r;
	}
	free(d->data);
	d->data = out;
	d->data_size = outsz;
	d->bytesperpixel = 3;
	return 1;
}

static int lookup_palette(unsigned color, unsigned *palette, int ncols)
{
	int i;
	for(i=0; i<ncols; ++i)
		if(palette[i] == color) return i;
	return -1;
}

static int ImageData_create_palette_pic(const ImageData* d, unsigned *palette, unsigned char **data)
{
	int ret = 0;
	unsigned char *p = d->data, *q = p + d->data_size;
	*data = malloc(d->width * d->height);
	unsigned char *o = *data, *e = o + (d->width * d->height);
	unsigned i, a = 0;
	while(p < q && o < e) {
		unsigned col, r, g, b;
		switch(d->bytesperpixel) {
		case 2: {
			unsigned lo = *(p++);
			unsigned hi = *(p++);
			rgb565_to_888(lo, hi, &r, &g, &b);
			break; }
		case 3:
		case 4:
			b = *(p++);
			g = *(p++);
			r = *(p++);
			if(d->bytesperpixel == 4) a = *(p++);
			break;
		default: break;
		}
		col = a << 24 | r << 16 | g << 8 | b;
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

static void convert_bottom_left_tga(ImageData *d) {
	size_t y, w = d->width*d->bytesperpixel;
	unsigned char *swp = malloc(w);
	if(!swp) return;
	for(y = 0; y < d->height/2; ++y) {
		size_t to = w*y, bo = (d->height-1-y)*w;
		memcpy(swp, d->data + to, w);
		memcpy(d->data + to, d->data + bo, w);
		memcpy(d->data + bo, swp, w);
	}
	free(swp);
}



/* exports */
TARGA_EXPORT int
Targa_readfile(const char *name, ImageData *idata, int skip_palette) {
	struct TargaHeader hdr; unsigned char hdr_buf[18];
	unsigned char ftr_buf[18+8];
	FILE *f = fopen(name, "rb");
	if(!f) return 0;
	fread(&hdr_buf, 1, sizeof(hdr_buf), f);
	fseek(f, 0, SEEK_END);
	off_t fs = ftello(f);
	if(fs > sizeof ftr_buf) {
		fseek(f, 0-sizeof ftr_buf, SEEK_END);
		fread(ftr_buf, 1, sizeof ftr_buf, f);
		if(!memcmp(ftr_buf+8, TARGA_FOOTER_SIGNATURE, 16))
			fs -= sizeof ftr_buf;
	}
	TargaHeader_from_buf(&hdr, hdr_buf);
	fseek(f, sizeof(hdr_buf) + hdr.idlength, SEEK_SET);
	fs -= ftello(f);
	unsigned char *data = malloc(fs), *palette = 0;
	unsigned palsz = 0;
	fread(data, 1, fs, f);
	if(hdr.colourmaptype) {
		palette = data;
		palsz = hdr.colourmaplength * hdr.colourmapdepth/8;
		if(fs <= palsz) return 0;
		data += palsz;
		fs -= palsz;
	}
	unsigned char *workdata = 0;
	unsigned tmp;
	switch(hdr.datatypecode) {
		case TIT_RLE_COLOR_MAPPED:
		case TIT_RLE_TRUE_COLOR:
			tmp = hdr.width*hdr.height*hdr.bitsperpixel/8;
			workdata = malloc(tmp);
			rle_decode(data, fs, hdr.bitsperpixel/8, workdata, tmp);
			break;
		case TIT_COLOR_MAPPED:
		case TIT_TRUE_COLOR:
			workdata = data;
			break;
		default:
			return 0;
	}
	idata->width = hdr.width;
	idata->height = hdr.height;
	if(skip_palette)
		idata->bytesperpixel = hdr.bitsperpixel/8;
	else
		idata->bytesperpixel = hdr.colourmapdepth? hdr.colourmapdepth/8 : hdr.bitsperpixel/8;

	tmp = idata->width*idata->height*idata->bytesperpixel;
	idata->data_size = tmp;
	idata->data = malloc(tmp);
	if(palette && !skip_palette) {
		unsigned i, j, bpp = hdr.colourmapdepth/8;
		unsigned char *p = idata->data, *q = workdata;
		for(i=0; i < idata->width*idata->height; ++i) {
			unsigned idx = *(q++);
			if(idx >= hdr.colourmaplength) return 0;
			for(j=0; j < bpp; ++j)
				*(p++) = palette[idx*bpp+j];
		}
	} else {
		memcpy(idata->data, workdata, tmp);
	}
	if(workdata != data) free(workdata);
	if(palette) free(palette);
	else free(data);
	if(hdr.y_origin == 0) convert_bottom_left_tga(idata);
	fclose(f);
	return 1;
}

TARGA_EXPORT int
Targa_writefile(const char *name, ImageData* d, unsigned char *palette)
{
	unsigned pal[256];
	unsigned char *paldata = 0, *data = d->data;
	unsigned bpp = d->bytesperpixel;
	unsigned data_size = d->data_size;
	int palcount = 256;
	int i;

	FILE *f = fopen(name, "wb");
	if(!f) {
		fprintf(stderr, "error opening %s\n", name);
		return 0;
	}

	if(bpp == 1) {
		if(!palette) for(i=0; i<256; ++i) pal[i] = rand();
		else for(i=0; i<256; ++i) {
			unsigned r = *(palette++);
			unsigned g = *(palette++);
			unsigned b = *(palette++);
			pal[i] = r << 16 | g << 8 | b ;
		}
	} else if( /* bpp != 2 && */
		(palcount = ImageData_create_palette_pic(d, pal, &paldata)) > 0) {
		/* can be saved as 8 bit palettized image */
		bpp = 1;
		data = paldata;
		data_size = d->width*d->height;
	} else if(bpp == 2 && ImageData_convert_16_to_24(d)) {
		bpp = 3;
		data = d->data;
		data_size = d->data_size;
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
		.colourmaplength = bpp == 1 ? palcount : 0,
		.colourmapdepth = bpp == 1 ? 24 : 0,
		.x_origin = 0,
		.y_origin = d->height, /* image starts at the top */
		.width = d->width,
		.height = d->height,
		.bitsperpixel = bpp*8,
		.imagedescriptor = 0x20, /* image starts at the top */
	};
	unsigned char hdr_buf[18];
	TargaHeader_to_buf(&hdr, hdr_buf);
	fwrite(&hdr_buf, 1, sizeof hdr_buf, f);
	unsigned tmp;
	if(bpp == 1) for(i=0; i<palcount; ++i) {
		tmp = end_htole32(pal[i]);
		fwrite(&tmp, 1, 3, f);
	}
	fwrite(data, 1, data_size, f);
	fclose(f);
	free(paldata);
	free(rle_data);
	return 1;
}

#endif /* TARGA_IMPL */


#endif
