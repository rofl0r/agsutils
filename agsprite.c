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
#include "rle.h"
#include <assert.h>
#include "version.h"
#define ADS ":::AGSprite " VERSION " by rofl0r:::"

#ifdef __x86_64__
#define breakpoint() __asm__("int3")
#else
#define breakpoint() do{}while(0)
#endif

#define FL_EXTRACT 1<<0
#define FL_PACK 1<<1
#define FL_VERBOSE 1<<2
#define FL_UNCOMPRESSED 1<<3
#define FL_HICOLOR 1<<4
#define FL_HICOLOR_SIMPLE (1<<5)
#define FL_SPRINDEX (1<<6)

static int debug_pic = -1, flags, filenr;

static int lookup_palette(unsigned color, unsigned *palette, int ncols)
{
	int i;
	for(i=0; i<ncols; ++i)
		if(palette[i] == color) return i;
	return -1;
}

static void rgb565_to_888(unsigned lo, unsigned hi, unsigned *r, unsigned *g, unsigned *b)
{
	*r = (hi & ~7) | (hi >> 5);
	*g = ((hi & 7) << 5)  | ((lo & 224) >> 3) | ((hi & 7) >> 1);
	*b = ((lo & 31) << 3) | ((lo & 28) >> 2);
}

static int convert_16_to_24(ImageData *d) {
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

static int create_palette_pic(const ImageData* d, unsigned *palette, unsigned char **data)
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
		default: assert(0);
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
	} else if( /* bpp != 2 && */
		(palcount = create_palette_pic(d, pal, &paldata)) > 0) {
		/* can be saved as 8 bit palettized image */
		bpp = 1;
		data = paldata;
		data_size = d->width*d->height;
	} else if(bpp == 2 && convert_16_to_24(d)) {
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
		.colourmaplength = bpp == 1 ? end_htole16(palcount) : 0,
		.colourmapdepth = bpp == 1 ? 24 : 0,
		.x_origin = 0,
		.y_origin = end_htole16(d->height), /* image starts at the top */
		.width = end_htole16(d->width),
		.height = end_htole16(d->height),
		.bitsperpixel = bpp*8,
		.imagedescriptor = 0x20, /* image starts at the top */
	};
	fwrite(&hdr, 1, sizeof hdr, f);
	unsigned tmp;
	if(bpp == 1) for(i=0; i<palcount; ++i) {
		tmp = end_htole32(pal[i]);
		fwrite(&tmp, 1, 3, f);
	}
	fwrite(data, 1, data_size, f);
	fclose(f);
	free(paldata);
	free(rle_data);
}

static int extract(char* file, char* dir) {
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
	if(flags & FL_VERBOSE) printf("processing spritefile TOC...\n");
	ret = SpriteFile_read(&f, &sf);
	if(!ret) {
		fprintf(stderr, "error reading spritefile %s\n", file);
		return 1;
	}
	FILE *info = 0;
	{
		char buf[1024];
		if(sf.palette) {
			snprintf(buf, sizeof buf, "%s/agsprite.pal", dir);
			FILE *pal = fopen(buf, "w");
			if(!pal) goto ferr;
			fwrite(sf.palette, 1, 256*3, pal);
			fclose(pal);
		}
		snprintf(buf, sizeof buf, "%s/agsprite.info", dir);
		info = fopen(buf, "w");
		if(!info) {
		ferr:
			fprintf(stderr, "error opening %s\n", buf);
			return 1;
		}
		fprintf(info,
		"info=infofile created by " ADS "\n"
		"info=this file is needed to reconstruct acsprset.spr\n"
		"spritecacheversion=%d\n"
		"spritecount=%d\n"
		"id=%d\n"
		"palette=%s\n"
		, sf.version, sf.num_sprites, sf.id, sf.palette ? "agsprite.pal" : ""
		);
	}
	int i;
	for(i=0; i<sf.num_sprites; i++) {
		if(debug_pic == i) breakpoint();
		ImageData d;
		if(SpriteFile_extract(&f, &sf, i, &d)) {
			char namebuf[64];
			snprintf(namebuf, sizeof namebuf, "sprite%06d_%02d_%dx%d.tga", i, d.bytesperpixel*8, d.width, d.height);
			fprintf(info, "%d=%s\n", i, namebuf);
			if(flags & FL_VERBOSE) printf("extracting sprite %d (%s)\n", i, namebuf);
			char filename[1024];
			snprintf(filename, sizeof filename, "%s/%s", dir, namebuf);
			write_tga(filename, &d, sf.palette);
			free(d.data);
		}
	}
	fclose(info);
	return 0;
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

static int read_tga(FILE *f, ImageData *idata, int skip_palette) {
	struct TargaHeader hdr;
	struct TargaFooter ftr;
	fread(&hdr, 1, sizeof hdr, f);
	fseek(f, 0, SEEK_END);
	off_t fs = ftello(f);
	if(fs > sizeof ftr) {
		fseek(f, 0-sizeof ftr, SEEK_END);
		fread(&ftr, 1, sizeof ftr, f);
		if(!memcmp(ftr.signature, TARGA_FOOTER_SIGNATURE, sizeof ftr.signature))
			fs -= sizeof ftr;
	}
	hdr.colourmaplength = end_htole16(hdr.colourmaplength);
	hdr.colourmapdepth = end_htole16(hdr.colourmapdepth);
	hdr.x_origin = end_htole16(hdr.x_origin);
	hdr.y_origin = end_htole16(hdr.y_origin);
	hdr.width = end_htole16(hdr.width);
	hdr.height = end_htole16(hdr.height);

	fseek(f, sizeof hdr + hdr.idlength, SEEK_SET);
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
	return 1;
}

static int is_upscaled_16bit(ImageData *d) {
	int i;
	for(i=0; i<d->data_size; i++) {
		if(i%3 != 1) { /* topmost 3 bits appended */
			if((d->data[i] & 7) != (d->data[i] >> 5))
				return 0;
		} else { /* topmost 2 bits appended */
			if((d->data[i] & 3) != (d->data[i] >> 6))
				return 0;
		}
	}
	return 1;
}

static int rawx_to_ags16(ImageData *d, int bpp) {
	int i, imax = d->data_size/bpp;
	unsigned char *data = malloc(d->width*d->height*2UL), *p = data;
	if(!data) return 0;
	for(i=0; i<imax; i++) {
		unsigned b = d->data[i*bpp+0];
		unsigned g = d->data[i*bpp+1];
		unsigned r = d->data[i*bpp+2];
		unsigned hi = (r & ~7) | (g >> 5);
		unsigned lo = ((g & 28) << 3) | (b >> 3);
		*(p++) = lo;
		*(p++) = hi;
	}
	free(d->data);
	d->data = data;
	d->bytesperpixel = 2;
	d->data_size = d->width*d->height*2;
	return 1;
}

static int raw24_to_ags16(ImageData *d) {
	return rawx_to_ags16(d, 3);
}
static int raw32_to_ags16(ImageData *d) {
	return rawx_to_ags16(d, 4);
}

static int raw24_to_32(ImageData *d) {
	unsigned char* data = malloc(d->width*d->height*4UL), *p = data, *q = d->data;
	if(!data) return 0;
	int i;
	for(i=0;i<d->width*d->height;++i) {
		unsigned b = *(q++);
		unsigned g = *(q++);
		unsigned r = *(q++);
		*(p++) = b;
		*(p++) = g;
		*(p++) = r;
		/* restore transparency for "magic magenta" pixels */
		*(p++) = "\xff\0"[!!(r == 0xff && g == 0 && b == 0xff)];
	}
	free(d->data);
	d->data = data;
	d->bytesperpixel = 4;
	d->data_size = d->width*d->height*4;
	return 1;
}
static int raw32_swap_alpha(ImageData *d) {
#if 0
	unsigned char *p = d->data, *pe = d->data+d->data_size;
	while(p < pe) {
		unsigned char *q = p;
		unsigned r = *(q++);
		unsigned g = *(q++);
		unsigned b = *(q++);
		unsigned a = *(q++);
		*(p++) = a;
		*(p++) = r;
		*(p++) = g;
		*(p++) = b;
	}
#endif
	return 1;
}
/* return true if alpha uses only values 0 (fully transparent)
   or 0xff (not transparent), and transparency is only used
   for "magic magenta". */
static int is_hicolor_candidate(ImageData *d) {
	unsigned char *p = d->data, *pe = d->data+d->data_size;
	while(p < pe) {
		unsigned b = *(p++);
		unsigned g = *(p++);
		unsigned r = *(p++);
		unsigned a = *(p++);
		switch(a) {
			case 0xff: break;
			case 0:
			if(!(r == 0xff && g == 0 && b == 0xff))
				return 0;
			break;
			default: return 0;
		}
	}
	return 1;
}
static int tga_to_ags(ImageData *d, int org_bpp) {
	/* convert raw image data to something acceptable for ags */
	switch(d->bytesperpixel) {
	case 4:
		if(flags & FL_HICOLOR) return raw32_to_ags16(d);
		else if(flags & FL_HICOLOR_SIMPLE && is_hicolor_candidate(d)) {
			if(flags & FL_VERBOSE) printf("converting %d to 16bpp\n", filenr);
			return raw32_to_ags16(d);
		} else return raw32_swap_alpha(d);
	case 3:
		if(flags & FL_HICOLOR) return raw24_to_ags16(d);
		if(org_bpp == 2 && is_upscaled_16bit(d)) return raw24_to_ags16(d);
		else return raw24_to_32(d);
	}
	return 1;
}

static int pack(char* file, char* dir) {
	if(access(dir, R_OK) == -1 && errno == ENOENT) {
		fprintf(stderr, "error opening dir %s\n", dir);
		return 1;
	}
	FILE *info = 0, *out = 0;
	char buf[1024];
	snprintf(buf, sizeof buf, "%s/agsprite.info", dir);
	info = fopen(buf, "r");
	if(!info) {
		fprintf(stderr, "error opening %s\n", buf);
		return 1;
	}
	SpriteFile sf = {0};
	int line = 0;
	while(fgets(buf, sizeof buf, info)) {
		++line;
		if(buf[0] == '#') continue; /* comment */
		char *p;
		p = strrchr(buf, '\n');
		if(p) {
			p[0] = 0;
			if(p > buf && p[-1] == '\r') p[-1] = 0;
		}
		p = strchr(buf, '=');
		if(!p) {
			fprintf(stderr, "syntax error on line %d of agsprite.info\n", line);
			return 1;
		}
		*(p++) = 0;
		if(!out) {
			if(0) {
			} else if(!strcmp("info", buf)) {
			} else if(!strcmp("spritecacheversion", buf)) {
				sf.version = atoi(p);
				if(sf.version > 6) {
					fprintf(stderr, "warning: converting to spritecache version 6\n");
					sf.version = 6;
				}
			} else if(!strcmp("spritecount", buf)) {
				sf.num_sprites = atoi(p);
			} else if(!strcmp("id", buf)) {
				sf.id = atoi(p);
			} else if(!strcmp("palette", buf)) {
				if (*p) {
					char buf2[1024];
					snprintf(buf2, sizeof buf2, "%s/%s", dir, p);
					FILE *pal = fopen(buf2, "r");
					if(!pal) {
						fprintf(stderr, "error opening %s\n", buf2);
						return 1;
					}
					sf.palette = malloc(256*3);
					fread(sf.palette, 1, 256*3, pal);
					fclose(pal);
				}
			} else {
				if(strcmp(buf, "0")) {
					fprintf(stderr, "unexpected keyword %s\n", buf);
					return 1;
				}
				out = fopen(file, "w");
				if(!out) {
					fprintf(stderr, "error opening %s\n", file);
					return 1;
				}
				/* default to compressed, if possible, and unless overridden */
				sf.compressed = !(flags&FL_UNCOMPRESSED);
				SpriteFile_write_header(out, &sf);
			}
		}
		if(out) {
			int n = filenr = atoi(buf);
			int org_bpp = 4;
			/* FIXME: use sscanf */
			if(strstr(p, "_08_")) org_bpp = 1;
			else if(strstr(p, "_16_")) org_bpp = 2;
			if(flags & FL_VERBOSE) printf("adding %d (%s)\n", n, p);
			if(debug_pic == n) breakpoint();

			while(sf.num_sprites < n) SpriteFile_add(out, &sf, &(ImageData){0});
			char fnbuf[1024];
			snprintf(fnbuf, sizeof fnbuf, "%s/%s", dir, p);
			FILE *spr = fopen(fnbuf, "r");
			if(!spr) {
				fprintf(stderr, "error opening %s\n", fnbuf);
				return 1;
			}
			ImageData data;
			int skip_palette =
				(sf.version == 4 && org_bpp != 1) ||
				(sf.version >= 4 && org_bpp == 1);
			if(!read_tga(spr, &data, skip_palette)) {
				fprintf(stderr, "error reading tga file %s\n", p);
				return 1;
			}
			tga_to_ags(&data, org_bpp);
			SpriteFile_add(out, &sf, &data);
			free(data.data);
			fclose(spr);
		}
	}
	SpriteFile_finalize(out, &sf);
	fclose(out);
	fclose(info);
	return 0;
}

static int sprindex(char* infile, char* outfile) {
	AF f;
	SpriteFile sf;
	int ret;
	ret = AF_open(&f, infile);
	if(!ret) {
		fprintf(stderr, "error opening %s\n", infile);
		return 1;
	}
	if(flags & FL_VERBOSE) printf("processing spritefile TOC...\n");
	ret = SpriteFile_read(&f, &sf);
	if(!ret) {
		fprintf(stderr, "error reading spritefile %s\n", infile);
		return 1;
	}
	FILE *out = fopen(outfile, "w");
	if(!out) {
		fprintf(stderr, "error opening outfile %s\n", outfile);
		return 1;
	}
	ret = SpriteFile_write_sprindex(&f, &sf, out);
	AF_close(&f);
	fclose(out);
	return !ret;
}

static int parse_argstr(char *arg)
{
	const struct flagmap {
		char chr;
		int flag;
	} map[] = {
		{ 'x', FL_EXTRACT},
		{ 'c', FL_PACK},
		{ 'i', FL_SPRINDEX},
		{ 'v', FL_VERBOSE},
		{ 'u', FL_UNCOMPRESSED},
		{ 'h', FL_HICOLOR},
		{ 'H', FL_HICOLOR_SIMPLE},
		{0, 0},
	};
	int ret = 0, i;
	while(*arg) {
		int found = 0;
		for(i=0;map[i].chr;++i)
			if(map[i].chr == *arg) {
				ret |= map[i].flag;
				found = 1;
				break;
		}
		if(!found) return 0;
		++arg;
	}
	return ret;
}

static int usage(char *a) {
	printf(	"%s ACTIONSTR acsprset.spr DIR\n"
		"ACTIONSTR can be:\n"
		"x - extract\n"
		"c - pack\n"
		"i - create sprindex.dat from .spr\n"
		"optionally followed by option characters.\n\n"
		"option characters:\n"
		"v - be verbose (both)\n"
		"u - don't use RLE compression if v >= 6 (pack)\n"
		"h - store all 32bit sprites as 16bit (pack)\n"
		"H - same, but only when alpha unused (pack)\n"
		"\n"
		"extract mode:\n"
		"extracts all sprites from acsprset.spr to DIR\n"
		"due to the way sprite packs work, for some versions\n"
		"8 bit images are stored without palette (dynamically\n"
		"assigned during game). in such a case a random palette\n"
		"is generated.\n\n"
		"pack mode:\n"
		"packs files in DIR to acsprset.spr\n"
		"image files need to be in tga format\n\n"
		"sprite index mode:\n"
		"here DIR parameter is repurposed to actually mean output file.\n"
		"a sprindex.dat file corresponding to acsprset.spr param is created.\n\n"
		"examples:\n"
		"%s xv acsprset.spr IMAGES/\n"
		"%s cu test.spr IMAGES/\n"
		"%s i repack.spr FILES/sprindex.dat\n"
		, a, a, a, a);
	return 1;
}

int main(int argc, char **argv) {

	if(argc != 4 || !(flags = parse_argstr(argv[1]))
	|| !((flags & FL_EXTRACT) || (flags & FL_PACK) || (flags & FL_SPRINDEX))
	|| ((flags&FL_EXTRACT)&&(flags&FL_PACK)) )
		return usage(argv[0]);

	char* file = argv[2];
	char *dir = argv[3];
	if(getenv("DEBUG")) debug_pic = atoi(getenv("DEBUG"));

	if(flags & FL_EXTRACT) return extract(file, dir);
	else if(flags & FL_SPRINDEX) return sprindex(file, dir);
	else return pack(file, dir);
}
