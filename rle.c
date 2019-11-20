#include <stdlib.h>

#define rle_read_col(OUT, IDX) \
	for(OUT=0, i=0; i<bpp;++i) {OUT |= (q[bpp*(IDX)+i] << (i*8));}
unsigned rle_encode(
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

/* caller needs to provide result buffer of w*h*bpp size. */
void rle_decode(
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
