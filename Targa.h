#ifndef TARGA_H
#define TARGA_H

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
} __attribute__((packed, aligned (1)));

enum TargaImageType {
	TIT_COLOR_MAPPED = 1,
	TIT_TRUE_COLOR = 2,
	TIT_BLACK_WHITE = 3,
	TIT_RLE_COLOR_MAPPED = 9,
	TIT_RLE_TRUE_COLOR = 10,
	TIT_RLE_BLACK_WHITE = 11,
};

#endif
