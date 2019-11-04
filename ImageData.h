#ifndef IMAGEDATA_H
#define IMAGEDATA_H

typedef struct ImageData {
	short width;
	short height;
	short bytesperpixel;
	unsigned data_size;
	unsigned char* data;
} ImageData;

#endif

