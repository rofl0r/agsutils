#include <string.h>

#define N 4096

#define OUT(X) if(out < outend) *(out++) = X; else return 0;

int lzwdecomp(unsigned char* in, unsigned long insz,
	      unsigned char* out, unsigned long outsz)
{
	unsigned char buf[N];
	unsigned char *inend = in+insz, *outend = out+outsz;
	int bits, len, mask, i = N - 16;
	while (1) {
		if(in >= inend) return 0;
		bits = *(in++);
		for (mask = 1; mask & 0xFF; mask <<= 1) {
			if (bits & mask) {
				if(in+2>inend) return 0;
				short tmp;
				memcpy(&tmp, in, 2);
				in += 2;
				int j = tmp;

				len = ((j >> 12) & 15) + 3;
				j = (i - j - 1) & (N - 1);

				while (len--) {
					buf[i] = buf[j];
					OUT(buf[i]);
					j = (j + 1) & (N - 1);
					i = (i + 1) & (N - 1);
				}
			} else {
				if(in >= inend) return 0;
				buf[i] = *(in++);
				OUT(buf[i]);
				i = (i + 1) & (N - 1);
			}
			if(out == outend) break;
		}
		if(out == outend) break;
	}
	return 1;
}
