#include "SpriteFile.h"
#include <stdlib.h>
#include <assert.h>

static int alloc_sprite_index(SpriteFile *si, int nsprites) {
	si->offsets = malloc(nsprites*4);
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
		assert(p);
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
	if(sf->version == 5 || sf->compressed) data->data_size = AF_read_uint(f);
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

int SpriteFile_read(AF* f, SpriteFile *sf) {
	AF_set_pos(f, 0);
	sf->version = AF_read_short(f);
	char buf[16];
	ssize_t n = AF_read(f, buf, 13);
	if(n != 13) return 0;
	if(memcmp(buf, " Sprite File ", 13)) return 0;
	sf->id = 0;
	if(sf->version < 4 || sf->version > 6) return 0;
	if(sf->version == 4) sf->compressed = 0;
	else if(sf->version == 5) sf->compressed = 1;
	else if(sf->version >= 6) {
		AF_read(f, buf, 1);
		sf->compressed = (buf[0] == 1);
		sf->id = AF_read_int(f);
	}

	if(sf->version < 5) {
		sf->palette = malloc(256*3);
		AF_read(f, sf->palette, 256*3);
	} else sf->palette = 0;

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
		if(sf->version == 5 || sf->compressed) sprite_data_size = AF_read_uint(f);
		else sprite_data_size = coldep * w * h;
		AF_read_junk(f, sprite_data_size);
	}
	return 1;
}
