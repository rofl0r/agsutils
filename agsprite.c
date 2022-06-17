#include "File.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include "endianness.h"
#define TARGA_IMPL
#include "Targa.h"
#include "SpriteFile.h"
#include <assert.h>
#include "version.h"
#include "debug.h"
#ifdef _WIN32
#include <direct.h>
#define MKDIR(D) mkdir(D)
#define PSEP '\\'
#else
#define MKDIR(D) mkdir(D, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#define PSEP '/'
#endif

#define ADS ":::AGSprite " VERSION " by rofl0r:::"

#define FL_EXTRACT 1<<0
#define FL_PACK 1<<1
#define FL_VERBOSE 1<<2
#define FL_UNCOMPRESSED 1<<3
#define FL_HICOLOR 1<<4
#define FL_HICOLOR_SIMPLE (1<<5)
#define FL_SPRINDEX (1<<6)

extern unsigned char defpal[];

/*
sprite file versions:
    kSprfVersion_Uncompressed = 4,
    kSprfVersion_Compressed = 5,
    kSprfVersion_Last32bit = 6,
    kSprfVersion_64bit = 10,
    kSprfVersion_HighSpriteLimit = 11,
    kSprfVersion_StorageFormats = 12,
*/

static int debug_pic = -1, flags, filenr;

static int extract(char* file, char* dir) {
	if(access(dir, R_OK) == -1 && errno == ENOENT) {
		MKDIR(dir);
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
			snprintf(buf, sizeof buf, "%s%cagsprite.pal", dir, PSEP);
			FILE *pal = fopen(buf, "wb");
			if(!pal) goto ferr;
			fwrite(sf.palette, 1, 256*3, pal);
			fclose(pal);
		}
		snprintf(buf, sizeof buf, "%s%cagsprite.info", dir, PSEP);
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
	if(!sf.palette) sf.palette = defpal;
	int i;
	for(i=0; i<sf.num_sprites; i++) {
		if(debug_pic == i) breakpoint();
		ImageData d;
		int ret = SpriteFile_extract(&f, &sf, i, &d);
		if(ret == 1) {
			char namebuf[64];
			snprintf(namebuf, sizeof namebuf, "sprite%06d_%02d_%dx%d.tga", i, d.bytesperpixel*8, d.width, d.height);
			fprintf(info, "%d=%s\n", i, namebuf);
			if(flags & FL_VERBOSE) printf("extracting sprite %d (%s)\n", i, namebuf);
			char filename[1024];
			snprintf(filename, sizeof filename, "%s%c%s", dir, PSEP, namebuf);
			if(!Targa_writefile(filename, &d, sf.palette))
				fprintf(stderr, "error opening %s\n", filename);
			free(d.data);
		} else if (ret == 0) {
			fprintf(stderr, "warning: failed to extract sprite %d\n", i);
		}
	}
	fclose(info);
	return 0;
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
	snprintf(buf, sizeof buf, "%s%cagsprite.info", dir, PSEP);
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
					fprintf(stderr, "warning: converting spritecache version %d to version 6\n", sf.version);
					sf.version = 6;
				}
			} else if(!strcmp("spritecount", buf)) {
				sf.num_sprites = atoi(p);
			} else if(!strcmp("id", buf)) {
				sf.id = atoi(p);
			} else if(!strcmp("palette", buf)) {
				if (*p) {
					char buf2[1024];
					snprintf(buf2, sizeof buf2, "%s%c%s", dir, PSEP, p);
					FILE *pal = fopen(buf2, "rb");
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
				out = fopen(file, "wb");
				if(!out) {
					fprintf(stderr, "error opening %s\n", file);
					return 1;
				}
				/* default to compressed, if possible, and unless overridden */
				sf.compressed = !(flags&FL_UNCOMPRESSED);
				/* SpriteFile_write_header also resets sf.compressed, if needed */
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
			snprintf(fnbuf, sizeof fnbuf, "%s%c%s", dir, PSEP, p);
			ImageData data;
			int skip_palette = org_bpp == 1;
			if(!Targa_readfile(fnbuf, &data, skip_palette)) {
				fprintf(stderr, "error reading tga file %s\n", p);
				return 1;
			}
			tga_to_ags(&data, org_bpp);
			SpriteFile_add(out, &sf, &data);
			free(data.data);
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
	FILE *out = fopen(outfile, "wb");
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
		"assigned during game). in such a case a standard palette\n"
		"is assigned.\n\n"
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
