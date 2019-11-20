#ifndef RLE_H
#define RLE_H

unsigned rle_encode(
        unsigned char *data, unsigned data_size,
        unsigned bpp, unsigned char** result);

/* caller needs to provide result buffer of w*h*bpp size. */
void rle_decode(
        unsigned char *data, unsigned data_size,
        unsigned bpp, unsigned char* result, unsigned result_size);

#pragma RcB2 DEP "rle.c"

#endif
