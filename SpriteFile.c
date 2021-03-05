#include "SpriteFile.h"
#include <stdlib.h>
#include <assert.h>

#define MAX_OLD_SPRITES 0xfffe

static int alloc_sprite_index(SpriteFile *si, int nsprites) {
	si->offsets = calloc(4, nsprites);
	return 1;
}

static void *readfunc_n(unsigned char *in, unsigned *out, int bpp)
{
	*out = 0;
	switch(bpp) {
	default:
		*out |= (*(in++) << 24); /*fall-through*/
	case 3:
		*out |= (*(in++) << 16); /*fall-through*/
	case 2:
		*out |= (*(in++) << 8); /*fall-through*/
	case 1:
		*out |= (*(in++) << 0); /*fall-through*/
	}
	return in;
}

static void writefunc_n(unsigned char *out, int n, unsigned value, int bpp)
{
	out+=n*bpp;
	unsigned i = 0;
	switch(bpp) {
	default:
		out[i++] = (value & 0xff000000) >> 24; /*fall-through*/
	case 3:
		out[i++] = (value & 0xff0000) >> 16; /*fall-through*/
	case 2:
		out[i++] = (value & 0xff00) >> 8; /*fall-through*/
	case 1:
		out[i++] = (value & 0xff) >> 0; /*fall-through*/
	}
}

static char* unpackl(signed char *out, signed char *in, int size, int bpp)
{
	int n = 0;
	while (n < size) {
		signed char c = *(in++);
		unsigned val;
		if(c == -128) c = 0;
		if(c < 0) {
			int i = 1 - c;
			in = readfunc_n(in, &val, bpp);
			while(i--) {
				if (n >= size) return 0;
				writefunc_n(out, n++, val, bpp);
			}
		} else {
			int i = c + 1;
			while (i--) {
				if (n >= size) return 0;
				in = readfunc_n(in, &val, bpp);
				writefunc_n(out, n++, val, bpp);
			}
		}
	}
	return in;
}

static int ags_unpack(ImageData *d) {
	unsigned y;
	unsigned outsize = d->width*d->height*d->bytesperpixel;
	unsigned char *out = malloc(outsize), *p = d->data, *q = out;
	if(!out) return 0;
	for(y = 0; y < d->height; ++y, q+=d->width*d->bytesperpixel) {
		p = unpackl(q, p, d->width, d->bytesperpixel);
	}
	free(d->data);
	d->data = out;
	d->data_size = outsize;
	return 1;
}

static unsigned readfunc_p(unsigned char *in, int n, int bpp)
{
	unsigned out = 0;
	in += n*bpp;
	switch(bpp) {
	default:
		out |= (*(in++) << 24); /*fall-through*/
	case 3:
		out |= (*(in++) << 16); /*fall-through*/
	case 2:
		out |= (*(in++) << 8); /*fall-through*/
	case 1:
		out |= (*(in++) << 0); /*fall-through*/
	}
	return out;
}

static void* writefunc_p(unsigned char *out, unsigned value, int bpp)
{
	switch(bpp) {
	default:
		*(out++) = (value & 0xff000000) >> 24; /*fall-through*/
	case 3:
		*(out++) = (value & 0xff0000) >> 16; /*fall-through*/
	case 2:
		*(out++) = (value & 0xff00) >> 8; /*fall-through*/
	case 1:
		*(out++) = (value & 0xff) >> 0; /*fall-through*/
	}
	return out;
}


static char* packl(unsigned char *out, unsigned char *in, int size, int bpp)
{
	int n = 0;
	unsigned col;
	while (n < size) {
		int i = n, j = n + 1, jmax = j + 126;
		if (jmax >= size) jmax = size - 1;
		if (i == size - 1) {
			col = readfunc_p(in, n++, bpp);
			out = writefunc_p(out, 0, 1);
			out = writefunc_p(out, col, bpp);
		} else if(readfunc_p(in, i, bpp) == readfunc_p(in, j, bpp)) {
			while((j < jmax) && (readfunc_p(in, j, bpp) == readfunc_p(in, j + 1, bpp))) ++j;
			col = readfunc_p(in, i, bpp);
			out = writefunc_p(out, i-j, 1);
			out = writefunc_p(out, col, bpp);
			n += j - i + 1;
		} else {
			while ((j < jmax) && (readfunc_p(in, j, bpp) != readfunc_p(in, j + 1, bpp))) ++j;
			int c = j - i;
			out = writefunc_p(out, c++, 1);
			memcpy(out, in+i*bpp, c*bpp);
			out += c*bpp;
			n += c;
		}
	}
	return out;
}

static int ags_pack(ImageData *d) {
	/* ags has no code for 24bit images :( */
	assert(d->bytesperpixel != 3 && d->bytesperpixel <= 4);
	unsigned y;
	/* worst case length: entire file consisting of sequences of 2
	   identical, and one different pixel, resulting in
	   1 byte flag + 1 pixel + 1 byte flag + 1 pixel. in the case of
	   8 bit, that's 1 byte overhead every 3 pixels.
	   since this compression is linebased, additional overhead of
	   1color/line is accounted for.
	*/
	unsigned outsize = d->width*d->height*d->bytesperpixel;
	outsize += 1 + (outsize/d->bytesperpixel/3) + (d->height*d->bytesperpixel);
	unsigned char *out = malloc(outsize), *p = d->data, *q = out;
	outsize = 0;
	if(!out) return 0;
	for(y = 0; y < d->height; ++y, p+=d->width*d->bytesperpixel) {
		unsigned char *next = packl(q, p, d->width, d->bytesperpixel);
		outsize += (next - q);
		q = next;
	}
	free(d->data);
	d->data = out;
	d->data_size = outsize;
	return 1;
}

int SpriteFile_extract(AF* f, SpriteFile *sf, int spriteno, ImageData *data) {
	if(spriteno >= sf->num_sprites+1) return 0;
	if(sf->offsets[spriteno] == 0) return 0;
	AF_set_pos(f, sf->offsets[spriteno]);
	data->bytesperpixel = AF_read_short(f);
	data->width  = AF_read_short(f);
	data->height = AF_read_short(f);
	if(sf->compressed) data->data_size = AF_read_uint(f);
	else data->data_size = data->bytesperpixel * data->width * data->height;
	data->data = malloc(data->data_size);
	if(!data->data) return 0;
	if(AF_read(f, data->data, data->data_size) != data->data_size) {
oops:
		free(data->data);
		data->data = 0;
		return 0;
	}
	if(sf->compressed && !ags_unpack(data)) goto oops;
	return 1;
}

#include "endianness.h"
#define f_set_pos(F, P) fseeko(F, P, SEEK_SET)
#define f_get_pos(F) ftello(F)
#define f_write(F, P, L) fwrite(P, 1, L, F)
#define f_write_int(F, I) do { unsigned _x = end_htole32(I); f_write(F, &_x, 4); } while(0)
#define f_write_uint(F, I) f_write_int(F, I)
#define f_write_short(F, I) do { unsigned short _x = end_htole16(I); f_write(F, &_x, 2); } while(0)
#define f_write_ushort(F, I) f_write_short(F, I)

int SpriteFile_write_header(FILE *f, SpriteFile *sf) {
	f_write_short(f, sf->version);
	if(13 != f_write(f, " Sprite File ", 13)) return 0;
	switch(sf->version) {
		/* override user-set compression setting,
		if required by chosen format */
		case 5: sf->compressed = 1; break;
		case 4: sf->compressed = 0; break;
		/* in case of version >= 6, set by caller*/
	}
	if(sf->version >= 6) {
		if(1 != f_write(f, "\0\1"+(!!sf->compressed), 1)) return 0;
		f_write_int(f, sf->id);
	} else if (sf->version < 5) {
		if(3*256 != f_write(f, sf->palette, 3*256))
			return 0;
	}
	sf->sc_off = f_get_pos(f);
	f_write_ushort(f, 0); /* number of sprites */
	sf->num_sprites = 0;
	return 1;
}

int SpriteFile_add(FILE *f, SpriteFile *sf, ImageData *data) {
	f_write_ushort(f, data->bytesperpixel);
	if(data->bytesperpixel) {
		f_write_ushort(f, data->width);
		f_write_ushort(f, data->height);
		if(sf->compressed) {
			ags_pack(data);
			f_write_uint(f, data->data_size);
		}
		f_write(f, data->data, data->data_size);
	}
	++sf->num_sprites;
	return 1;
}

int SpriteFile_finalize(FILE* f, SpriteFile *sf) {
	if(sf->num_sprites >= MAX_OLD_SPRITES) {
		fprintf(stderr, "error: 64bit spritefile support not implemented yet\n");
		return 0;
	}
	f_set_pos(f, sf->sc_off);
	f_write_ushort(f, sf->num_sprites -1);
	return 1;
}

int SpriteFile_read(AF* f, SpriteFile *sf) {
	AF_set_pos(f, 0);
	sf->version = AF_read_short(f);
	char buf[16];
	ssize_t n = AF_read(f, buf, 13);
	if(n != 13) return 0;
	if(memcmp(buf, " Sprite File ", 13)) return 0;
	sf->id = 0;
	switch(sf->version) {
		case 4:
			sf->compressed = 0;
			sf->palette = malloc(256*3);
			AF_read(f, sf->palette, 256*3);
			break;
		case 5:
			sf->compressed = 1;
			break;
		case 6: case 10: case 11:
			AF_read(f, buf, 1);
			sf->compressed = (buf[0] == 1);
			sf->id = AF_read_int(f);
			break;
		default:
			dprintf(2, "unsupported sprite file version %d\n", (int) sf->version);
			return 0;
	}

	if(sf->version >= 5) sf->palette = 0;

	if(sf->version >= 11)
		sf->num_sprites = AF_read_uint(f);
	else
		sf->num_sprites = AF_read_ushort(f);
	if(sf->version < 4) sf->num_sprites = 200;
	sf->num_sprites++;
	alloc_sprite_index(sf, sf->num_sprites);

	int i;
	for(i = 0; i < sf->num_sprites; ++i) {
		sf->offsets[i] = AF_get_pos(f);
		int coldep = AF_read_short(f);
		if(!coldep) {
			sf->offsets[i] = 0;
			continue;
		}
		int w = AF_read_short(f);
		int h = AF_read_short(f);
		unsigned sprite_data_size;
		if(sf->compressed) sprite_data_size = AF_read_uint(f);
		else sprite_data_size = coldep * w * h;
		AF_read_junk(f, sprite_data_size);
	}
	return 1;
}

/* create sprindex.dat, use after SpriteFile_read() */
int SpriteFile_write_sprindex(AF* f, SpriteFile *sf, FILE *outf)
{
	if(sf->num_sprites >= MAX_OLD_SPRITES) {
		fprintf(stderr, "error: support for 64bit sprindex files not supported, too many sprites\n");
		return 0;
	}
	unsigned short *h = calloc(2, sf->num_sprites);
	unsigned short *w = calloc(2, sf->num_sprites);
	f_write(outf, "SPRINDEX", 8);
	int version = 2;
	/* version, figure out when v1 is needed */
	f_write_int(outf, version);
	if(version >= 2) f_write_int(outf, sf->id);
	f_write_uint(outf, sf->num_sprites-1);
	f_write_uint(outf, sf->num_sprites);
	int i;
	for(i=0; i<sf->num_sprites;++i) {
		AF_set_pos(f, sf->offsets[i]);
		int coldep = AF_read_short(f);
		if(coldep == 0) sf->offsets[i] = 0;
		else {
			w[i] = AF_read_short(f);
			h[i] = AF_read_short(f);
		}
	}
	for(i=0; i<sf->num_sprites;++i)
		f_write_short(outf, w[i]);
	for(i=0; i<sf->num_sprites;++i)
		f_write_short(outf, h[i]);
	for(i=0; i<sf->num_sprites;++i)
		f_write_uint(outf, sf->offsets[i]);
	free(h);
	free(w);
	return 1;
}
