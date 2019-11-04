#ifndef BITMAP_H
#define BITMAP_H

#define WORD unsigned short
#define DWORD unsigned int
#define LONG DWORD

#define M_BITMAPFILEHEADER						\
	WORD  bfType ;        /* signature word "BM" or 0x4D42 */	\
	DWORD bfSize ;        /* entire size of file */			\
	WORD  bfReserved1 ;   /* must be zero */			\
	WORD  bfReserved2 ;   /* must be zero */			\
	DWORD bfOffsetBits ;  /* offset in file of DIB pixel bits */	\

/* note: original had biWidth/Height as WORD, thus size 12 */
#define M_BITMAPCOREHEADER						\
	DWORD biSize ;      /* size of the structure = 12 */		\
	LONG  biWidth ;     /* width of image in pixels */		\
	LONG  biHeight ;    /* height of image in pixels */		\
	WORD  biPlanes ;    /* = 1 */					\
	WORD  biBitCount ;  /* bits per pixel (1, 4, 8, or 24) */	\

#define M_BITMAPINFOHEADER						\
	M_BITMAPCOREHEADER						\
	DWORD biCompression ;    /* compression code */			\
	DWORD biSizeImage ;      /* number of bytes in image */		\
	LONG  biXPelsPerMeter ;  /* horizontal resolution */		\
	LONG  biYPelsPerMeter ;  /* vertical resolution */		\
	DWORD biClrUsed ;        /* number of colors used */		\
	DWORD biClrImportant ;   /* number of important colors */	\

#define M_BITMAPV2INFOHEADER						\
	M_BITMAPINFOHEADER						\
	DWORD biRedMask ;        /* Red color mask */			\
	DWORD biGreenMask ;      /* Green color mask */			\
	DWORD biBlueMask ;       /* Blue color mask */			\

#define M_BITMAPV3INFOHEADER						\
	M_BITMAPV2INFOHEADER						\
	DWORD biAlphaMask ;      /* Alpha mask */			\

#define M_BITMAPV4HEADER						\
	M_BITMAPV3INFOHEADER						\
	DWORD biCSType ;         /* color space type */			\
	LONG  biRedX;            /* X coordinate of red endpoint */	\
	LONG  biRedY;            /* Y coordinate of red endpoint */	\
	LONG  biRedZ;            /* Z coordinate of red endpoint */	\
	LONG  biGreenX;          /* X coordinate of green endpoint */	\
	LONG  biGreenY;          /* Y coordinate of green endpoint */	\
	LONG  biGreenZ;          /* Z coordinate of green endpoint */	\
	LONG  biBlueX;           /* X coordinate of blue endpoint */	\
	LONG  biBlueY;           /* Y coordinate of blue endpoint */	\
	LONG  biBlueZ;           /* Z coordinate of blue endpoint */	\
	DWORD biGammaRed ;       /* Red gamma value */			\
	DWORD biGammaGreen ;     /* Green gamma value */		\
	DWORD biGammaBlue ;      /* Blue gamma value */			\

#define M_BITMAPV5HEADER						\
	M_BITMAPV4HEADER						\
	DWORD biIntent ;         /* rendering intent */			\
	DWORD biProfileData ;    /* profile data or filename */		\
	DWORD biProfileSize ;    /* size of embedded data or filename */\
	DWORD biReserved ;						\

#define BMP_STRUCT_DECL(X)			\
struct X {					\
	M_ ## X					\
} __attribute__((packed, aligned(2)))

BMP_STRUCT_DECL(BITMAPFILEHEADER);
BMP_STRUCT_DECL(BITMAPCOREHEADER);
BMP_STRUCT_DECL(BITMAPINFOHEADER);
BMP_STRUCT_DECL(BITMAPV2INFOHEADER);
BMP_STRUCT_DECL(BITMAPV3INFOHEADER);
BMP_STRUCT_DECL(BITMAPV4HEADER);
BMP_STRUCT_DECL(BITMAPV5HEADER);

#undef BMP_STRUCT_DECL

#define BMP_STRUCT_DECL_X(X)	\
struct X ## _X {		\
	M_BITMAPFILEHEADER	\
	M_ ## X			\
}  __attribute__((packed, aligned (2)))


BMP_STRUCT_DECL_X(BITMAPCOREHEADER);
BMP_STRUCT_DECL_X(BITMAPINFOHEADER);
BMP_STRUCT_DECL_X(BITMAPV2INFOHEADER);
BMP_STRUCT_DECL_X(BITMAPV3INFOHEADER);
BMP_STRUCT_DECL_X(BITMAPV4HEADER);
BMP_STRUCT_DECL_X(BITMAPV5HEADER);

#undef BMP_STRUCT_DECL_X

#endif
