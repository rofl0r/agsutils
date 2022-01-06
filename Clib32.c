/* CLIB32 - DJGPP implemention of the CLIB reader.
  (c) 1998-99 Chris Jones, (c) 2012 rofl0r
  
  22/12/02 - Shawn's Linux changes approved and integrated - CJ

  v1.2 (Apr'01)  added support for new multi-file CLIB version 10 files
  v1.1 (Jul'99)  added support for appended-to-exe data files

  Adventure Game Studio source code Copyright 1999-2011 Chris Jones.
  All rights reserved.

  The AGS Editor Source Code is provided under the Artistic License 2.0
  http://www.opensource.org/licenses/artistic-license-2.0.php

  You MAY NOT compile your own builds of the engine without making it EXPLICITLY
  CLEAR that the code has been altered from the Standard Version.
  
  v1.3 rofl0r: code cleaned up for usage in agsutils.
  v1.4 rofl0r: added writer and reader interfaces, as well as the entire writer code.
  
*/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "ByteArray.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include "Clib32.h"
#include "endianness.h"

#ifndef _WIN32
#define O_BINARY 0
#define PSEP '/'
#else
#define PSEP '\\'
#endif

#define RAND_SEED_SALT 9338638

static char *clibendfilesig = "CLIB\x1\x2\x3\x4SIGE";
static char *clibpasswencstring = "My\x1\xde\x4Jibzle";
static int _last_rand;

static void init_pseudo_rand_gen(int seed) {
	_last_rand = seed;
}

static int get_pseudo_rand() {
	return( ((_last_rand = _last_rand * 214013L
	+ 2531011L) >> 16) & 0x7fff );
}

static void clib_encrypt_text(unsigned char *clear, unsigned char *encbuf) {
	unsigned adx = 0;
	while(1) {
		*encbuf = *clear + clibpasswencstring[adx];
		if(!*clear) break;
		adx++; encbuf++; clear++;
		if (adx > 10) adx = 0;
	}
}

static void clib_decrypt_text(char *toenc) {
	unsigned adx = 0;
	while (1) {
		*toenc -= clibpasswencstring[adx];
		if (!*toenc) break;
		adx++; toenc++;
		if (adx > 10) adx = 0;
	}
}

static void fgetnulltermstring(char *sss, struct ByteArray *ddd, int bufsize) {
	int b = -1;
	off_t l = ByteArray_get_length(ddd);
	do {
		if (b < bufsize - 1) b++;
		if(ByteArray_get_position(ddd) >= l)
			return;
		sss[b] = ByteArray_readByte(ddd);
	} while (sss[b] != 0);
}

long last_opened_size;

static int fread_data_enc_byte(struct ByteArray *ba) {
	return ByteArray_readUnsignedByte(ba) - get_pseudo_rand();
}

static uint32_t fread_data_enc_int(struct ByteArray *ba) {
	union {
		uint32_t i;
		unsigned char c[4];
	} res;
	res.i = ByteArray_readUnsignedInt(ba);
	size_t i = 0;
	for(; i < 4; i++)
		res.c[i] -= get_pseudo_rand();
	return res.i;
}

static void fread_data_intarray_enc(struct ByteArray *ba, unsigned* dest, size_t count) {
	size_t i = 0;
	for(; i < count; i++)
		dest[i] = fread_data_enc_int(ba);
}

static void fread_data_intarray(struct ByteArray *ba, unsigned* dest, size_t count) {
	size_t i = 0;
	for(; i < count; i++)
		dest[i] = ByteArray_readInt(ba);
}

#if 0
static void fread_data_enc(void *data, size_t dataSize, size_t dataCount, struct ByteArray *ooo) {
	ByteArray_readMultiByte(ooo, (char*)data, dataSize * dataCount);
	unsigned char *dataChar = (unsigned char*)data;
	size_t i = 0;
	for (; i < dataSize * dataCount; i++)
		dataChar[i] -= get_pseudo_rand();
}
#endif

static void fgetstring_enc(char *sss, struct ByteArray *ooo, int maxLength) {
	int i = 0;
	while ((i == 0) || (sss[i - 1] != 0)) {
		sss[i] = ByteArray_readByte(ooo) - get_pseudo_rand();
		if (i < maxLength - 1) i++;
	}
}

static int read_v30_clib(struct MultiFileLibNew * mfl, struct ByteArray * wout) {
	size_t aa;
	/* int reservedFlags = */ ByteArray_readInt(wout);

	mfl->num_data_files = ByteArray_readInt(wout);
	assert(mfl->num_data_files <= MAXMULTIFILES);
	for (aa = 0; aa < mfl->num_data_files; aa++)
		fgetnulltermstring(mfl->data_filenames[aa], wout, 50);
	mfl->num_files = ByteArray_readInt(wout);

	if (mfl->num_files > MAX_FILES)
		return -1;

	for (aa = 0; aa < mfl->num_files; aa++) {
		fgetnulltermstring(mfl->filenames[aa], wout, 100);
		mfl->file_datafile[aa] = ByteArray_readUnsignedByte(wout); /* "LibUid" */
		unsigned long long tmp;
		tmp = ByteArray_readUnsignedLongLong(wout);
		mfl->offset[aa] = tmp;
		tmp = ByteArray_readUnsignedLongLong(wout);
		mfl->length[aa] = tmp;
	}

//	for(aa = 0; aa < mfl->num_files; aa++)
//		mfl->file_datafile[aa] = fread_data_enc_byte(wout);
	return 0;
}

static int getw_enc(struct ByteArray *ooo) {
	return fread_data_enc_int(ooo);
}

static int read_new_new_enc_format_clib(struct MultiFileLibNew * mfl, struct ByteArray * wout) {
	size_t aa;
	int randSeed = ByteArray_readInt(wout);
	init_pseudo_rand_gen(randSeed + RAND_SEED_SALT);
	mfl->num_data_files = getw_enc(wout);
	for (aa = 0; aa < mfl->num_data_files; aa++)
		fgetstring_enc(mfl->data_filenames[aa], wout, 50);
	mfl->num_files = getw_enc(wout);

	if (mfl->num_files > MAX_FILES)
		return -1;

	for (aa = 0; aa < mfl->num_files; aa++)
		fgetstring_enc(mfl->filenames[aa], wout, 100);
	for (aa = 0; aa < mfl->num_files; aa++)
		mfl->offset[aa] = fread_data_enc_int(wout);
	for (aa = 0; aa < mfl->num_files; aa++)
		mfl->length[aa] = fread_data_enc_int(wout);
	for (aa = 0; aa < mfl->num_files; aa++)
		mfl->file_datafile[aa] = fread_data_enc_byte(wout);
	return 0;
}

static int read_new_new_format_clib(struct MultiFileLibNew* mfl, struct ByteArray * wout) {
	size_t aa;
	mfl->num_data_files = ByteArray_readInt(wout);
	for (aa = 0; aa < mfl->num_data_files; aa++)
		fgetnulltermstring(mfl->data_filenames[aa], wout, 50);
	mfl->num_files = ByteArray_readInt(wout);

	if (mfl->num_files > MAX_FILES) return -1;

	for (aa = 0; aa < mfl->num_files; aa++) {
		short nameLength = ByteArray_readShort(wout);
		nameLength /= 5;
		ByteArray_readMultiByte(wout, mfl->filenames[aa], nameLength);
		clib_decrypt_text(mfl->filenames[aa]);
	}
	for (aa = 0; aa < mfl->num_files; aa++)
		mfl->offset[aa] = ByteArray_readInt(wout);
	for (aa = 0; aa < mfl->num_files; aa++)
		mfl->length[aa] = ByteArray_readInt(wout);
	ByteArray_readMultiByte(wout, mfl->file_datafile, mfl->num_files);
	return 0;
}

static int read_new_format_clib(struct MultiFileLib * mfl, struct ByteArray * wout, int libver) {
	mfl->num_data_files = ByteArray_readInt(wout);
	ByteArray_readMultiByte(wout, (char*) mfl->data_filenames, 20U * mfl->num_data_files);
	mfl->num_files = ByteArray_readInt(wout);

	if (mfl->num_files > MAX_FILES) return -1;
	
	ByteArray_readMultiByte(wout, (char*) mfl->filenames, 25U * mfl->num_files);
	
	fread_data_intarray(wout, mfl->offset, mfl->num_files);
	fread_data_intarray(wout, mfl->length, mfl->num_files);
	ByteArray_readMultiByte(wout, mfl->file_datafile, mfl->num_files);

	if (libver >= 11) {
		size_t aa;
		for (aa = 0; aa < mfl->num_files; aa++)
			clib_decrypt_text(mfl->filenames[aa]);
	}
	return 0;
}

void AgsFile_close(struct AgsFile *f) {
	unsigned i;
	for (i=0; i < AgsFile_getDataFileCount(f); i++)
		ByteArray_close_file(&f->f[i]);
}

int AgsFile_getVersion(struct AgsFile *f) {
	return f->libversion;
}

void AgsFile_setVersion(struct AgsFile *f, int version) {
	f->libversion = version;
}

void AgsFile_setSourceDir(struct AgsFile *f, char* sourcedir) {
	f->dir = sourcedir;
}

void AgsFile_setFileCount(struct AgsFile *f, size_t count) {
	f->mflib.num_files = count;
}

static off_t filelength(int fd) {
	struct stat st;
	fstat(fd, &st);
	return st.st_size;
}

int AgsFile_setFile(struct AgsFile *f, size_t index, char* fn) {
	strncpy(f->mflib.filenames[index], fn, 100);
	char fnbuf[512];
	snprintf(fnbuf, sizeof(fnbuf), "%s%c%s", f->dir, PSEP, f->mflib.filenames[index]);
	int fd = open(fnbuf, O_RDONLY|O_BINARY);
	if(fd == -1) return 0;
	off_t fl = filelength(fd);
	close(fd);
	f->mflib.length[index] = fl;
	return 1;
}

int AgsFile_setDataFile(struct AgsFile *f, size_t index, char* fn) {
	size_t l = strlen(fn);
	if(l >= 20) return 0;
	strncpy(f->mflib.data_filenames[index], fn, 20);
	return 1;
}

void AgsFile_setDataFileCount(struct AgsFile *f, size_t count) {
	f->mflib.num_data_files = count;
}

static void write_ull(int fd, unsigned long long val) {
	unsigned long long le = end_htole64(val);
	write(fd, &le, sizeof(le));
}

static int get_int_le(int val) {
	return end_htole32(val);
}

static void write_int(int fd, int val) {
	int le = get_int_le(val);
	write(fd, &le, sizeof(le));
}

static short get_short_le(short val) {
	return end_htole16(val);
}

static void write_short(int fd, short val) {
	short le = get_short_le(val);
	write(fd, &le, sizeof(le));
}

static void write_int_array(int fd, int* arr, size_t len) {
	size_t i = 0;
	for(; i < len; i++)
		write_int(fd, arr[i]);
}

/* if *bytes == -1L, read entire file, and store the total amount read there */
static int copy_into_file(int fd, const char *dir, const char *fn, size_t *bytes) {
	char fnbuf[512];
	snprintf(fnbuf, sizeof(fnbuf), "%s%c%s", dir, PSEP, fn);
	int f = open(fnbuf, O_RDONLY|O_BINARY);
	if(f == -1) return 0;
	size_t filesize = *bytes;
	char readbuf[4096];
	int ret = 1;
	if(filesize == -1L) {
		*bytes = 0;
		while(1) {
			ssize_t r = read(f, readbuf, sizeof readbuf);
			if(r < 0) { ret=0; goto end; }
			if(r == 0) break;
			*bytes += r;
			if(write(fd, readbuf, r) != r) { ret=0; goto end; }
		}
	} else do {
		size_t togo = filesize > sizeof(readbuf) ? sizeof(readbuf) : filesize;
		if((size_t) read(f, readbuf, togo) != togo) { ret = 0; goto end; }
		filesize -= togo;
		ssize_t wret;
		size_t nwritten = 0;
		while(togo) {
			wret = write(fd, readbuf + nwritten, togo);
			if(wret <= 0) { ret = 0; goto end; }
			nwritten += wret;
			togo -= wret;
		}
	} while(filesize);
	end:
	close(f);
	return ret;
}

#define WH_EWRITE(X,Y,Z) do {if(Z != write(X, Y, Z)) return 0;} while(0)
static size_t write_header(struct AgsFile *f, int fd) {
	int myversion = f->libversion >= 30 ? 30 : 20; //f->libversion;
	unsigned char version = myversion;
	WH_EWRITE(fd, "CLIB\x1a", 5);
	WH_EWRITE(fd, &version, 1);
	version = 0;
	if(myversion >= 10) WH_EWRITE(fd, &version, 1);
	size_t written = 7;
	size_t i,l;
	if(myversion == 30) {
		write_int(fd, 0); /*reservedFlags*/
		written += 4;
	}
	write_int(fd, f->mflib.num_data_files);
	written += sizeof(int);
	for(i = 0; i < f->mflib.num_data_files; i++) {
		l = strlen(f->mflib.data_filenames[i]) + 1;
		written += l;
		if(l != (size_t) write(fd, f->mflib.data_filenames[i], l))
			return 0;
	}

	write_int(fd, f->mflib.num_files);
	written += sizeof(int);

	if(myversion == 30) {
		for(i = 0; i < f->mflib.num_files; i++) {
			l = strlen(f->mflib.filenames[i]) + 1;
			written += l;
			if(l != (size_t) write(fd, f->mflib.filenames[i], l))
				return 0;
			/* write another 0 byte as libuid */
			WH_EWRITE(fd, &version, 1);
			++written;

			write_ull(fd, f->mflib.offset[i]);
			written += 8;

			write_ull(fd, f->mflib.length[i]);
			written += 8;
		}
		return written;
	}

	unsigned char encbuf[100];
	for(i = 0; i < f->mflib.num_files; i++) {
		l = strlen(f->mflib.filenames[i]) + 1;
		write_short(fd, l * 5);
		clib_encrypt_text((unsigned char*)f->mflib.filenames[i], encbuf);
		if(l != (size_t) write(fd, encbuf, l)) return 0;
		written += sizeof(short) + l;
	}
	l = f->mflib.num_files;
	for(i = 0; i < l; ++i)
		write_int(fd, f->mflib.offset[i]);
	written += sizeof(int) * l;

	for(i = 0; i < l; ++i)
		write_int(fd, f->mflib.length[i]);
	written += sizeof(int) * l;

	if(l != (size_t) write(fd, f->mflib.file_datafile, l))
		return 0;
	written += l;
	return written;
}

static void write_footer(int fd, unsigned offset, int v30) {
	write_int(fd, offset);
	if(v30) write_int(fd, 0);
	write(fd, clibendfilesig, 12);
}

void AgsFile_setExeStub(struct AgsFile *f, const char *fn) {
	f->exestub_fn = fn;
}

int AgsFile_write(struct AgsFile *f) {
	int fd = open(f->fn, O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, 0660);
	if(fd == -1) return 0;
	size_t i;
	unsigned long long off;
	if(!(off = write_header(f, fd))) return 0;
	for(i = 0; i < f->mflib.num_files; i++) {
		f->mflib.offset[i] = off;
		off += f->mflib.length[i];
	}
	lseek(fd, 0, SEEK_SET);
	unsigned header_offset = 0; /* we can safely assume the exe stub fits into 4 GB */
	if(f->exestub_fn) {
		size_t stubsize = -1L;
		if(!copy_into_file(fd, f->dir, f->exestub_fn, &stubsize))
			return 0;
		header_offset = stubsize;
	}
	write_header(f, fd);
	for(i = 0; i < f->mflib.num_files; i++) {
		size_t fs = f->mflib.length[i];
		if(!copy_into_file(fd, f->dir, f->mflib.filenames[i], &fs))
			return 0;
	}
	write_footer(fd, header_offset, f->libversion >= 30);
	close(fd);
	return 1;
}

static int prep_multifiles(struct AgsFile *f) {
	unsigned aa;
	struct ByteArray *ba;
	/* open each datafile as a byte array */
	assert(MAXMULTIFILES >= f->mflib.num_data_files);
	for (aa = 1; aa < f->mflib.num_data_files; aa++) {
		ba = &f->f[aa];
		ByteArray_ctor(ba);
		if(!ByteArray_open_file(ba, f->mflib.data_filenames[aa])) {
			fprintf(stderr, "error opening mfl datafile %s\n", f->mflib.data_filenames[aa]);
			return -1;
		}
		ByteArray_set_endian(ba, BAE_LITTLE); // all ints etc are saved in little endian.
	}
	return 0;
}

static int csetlib(struct AgsFile* f, char *filename)  {
	char clbuff[20];
	if (!filename)
		return -11;
	
	int passwmodifier = 0;
	size_t aa;
	size_t cc, l;
	
	struct ByteArray *ba = &f->f[0];
	ByteArray_ctor(ba);
	if(!ByteArray_open_file(ba, filename)) return -1;
	ByteArray_set_endian(ba, BAE_LITTLE); // all ints etc are saved in little endian.
	off_t ba_len = ByteArray_get_length(ba);
	ByteArray_readMultiByte(ba, clbuff, 5);

	uint32_t absoffs = 0; /* we read 4 bytes- so our datatype 
	must be 4 bytes as well, since we use a pointer to it */

	f->pack_off = 0;
	if (strncmp(clbuff, "CLIB", 4) != 0) {
		/* CLIB signature not found at the beginning of file
		   so it's probably an exe and we need to lookup the tail
		   signature. */
		ByteArray_set_position(ba,  ba_len - 12);
		ByteArray_readMultiByte(ba, clbuff, 12);

		if (strncmp(clbuff, clibendfilesig, 12) != 0)
			return -2;
		// it's an appended-to-end-of-exe thing.
		// there's a new format using a 64bit offset, we need to try
		// that first.

		uint64_t off64;
		ByteArray_set_position(ba,  ba_len - 20);
		off64 = ByteArray_readUnsignedLongLong(ba);
		if(off64 < ba_len &&
		   ByteArray_set_position(ba, off64) &&
		   ByteArray_readMultiByte(ba, clbuff, 5) &&
		   !strncmp(clbuff, "CLIB", 4)) {
			if(off64 > 0xffffffff) {
				fprintf(stderr, "error: gamepacks > 4GB not supported at this time\n");
				return -21;
			}
			absoffs = off64;
		} else {
			ByteArray_set_position(ba,  ba_len - 16);
			absoffs = ByteArray_readUnsignedInt(ba);
		}

		ByteArray_set_position(ba, absoffs);
		ByteArray_readMultiByte(ba, clbuff, 5);
		if(strncmp(clbuff, "CLIB", 4)) {
			fprintf(stderr, "error: ags clib header signature not found\n");
			return -22;
		}

		f->pack_off = absoffs;
	}

	f->libversion = ByteArray_readUnsignedByte(ba);
	switch (f->libversion) {
		/* enum MFLVersion (kMFLVersion_MultiV21 ...)  in newer AGS */
		case 6: case 10: case 11: case 15: case 20: case 21: case 30:
			break;
		default:
			// unsupported version
			return -3;
	}

	// remove slashes so that the lib name fits in the buffer
	while (filename[0] == '\\' || filename[0] == '/') filename++;

	if (f->libversion >= 10) {
		if (ByteArray_readUnsignedByte(ba) != 0)
			return -4;  // not first datafile in chain

		if (f->libversion >= 30) {
			if (read_v30_clib(&f->mflib, ba)) return -5;
		} else if (f->libversion >= 21) {
			if (read_new_new_enc_format_clib(&f->mflib, ba))
			return -5;
		} else if (f->libversion == 20) {
			if (read_new_new_format_clib(&f->mflib, ba))
			return -5;
		} else  {
			struct MultiFileLib mflibOld_b, *mflibOld = &mflibOld_b;

			if (read_new_format_clib(mflibOld, ba, f->libversion))
				return -5;
			// convert to newer format
			f->mflib.num_files = mflibOld->num_files;
			f->mflib.num_data_files = mflibOld->num_data_files;
			for(aa = 0; aa < f->mflib.num_files; ++aa)
				f->mflib.offset[aa] = mflibOld->offset[aa];
			for(aa = 0; aa < f->mflib.num_files; ++aa)
				f->mflib.length[aa] = mflibOld->length[aa];
			memcpy(f->mflib.file_datafile, mflibOld->file_datafile, sizeof(char) * f->mflib.num_files);
			assert(MAXMULTIFILES >= f->mflib.num_data_files);
			for (aa = 0; aa < f->mflib.num_data_files; aa++)
				strcpy(f->mflib.data_filenames[aa], mflibOld->data_filenames[aa]);
			for (aa = 0; aa < f->mflib.num_files; aa++)
				strcpy(f->mflib.filenames[aa], mflibOld->filenames[aa]);
		}

		strcpy(f->mflib.data_filenames[0], filename);
		for (aa = 0; aa < f->mflib.num_files; aa++) {
			// correct offsets for EXE file
			if (f->mflib.file_datafile[aa] == 0)
				f->mflib.offset[aa] += absoffs;
		}
		return prep_multifiles(f);
	}

	passwmodifier = ByteArray_readUnsignedByte(ba);
	ByteArray_readUnsignedByte(ba); // unused byte
	f->mflib.num_data_files = 1;
	strcpy(f->mflib.data_filenames[0], filename);

	short tempshort = ByteArray_readShort(ba);
	f->mflib.num_files = tempshort;

	if (f->mflib.num_files > MAX_FILES) return -4;

	ByteArray_readMultiByte(ba, clbuff, 13);  // skip password dooberry
	for (aa = 0; aa < f->mflib.num_files; aa++) {
		ByteArray_readMultiByte(ba, f->mflib.filenames[aa], 13);
		l = strlen(f->mflib.filenames[aa]);
		for (cc = 0; cc < l; cc++)
			f->mflib.filenames[aa][cc] -= passwmodifier;
	}
	for(cc = 0; cc < f->mflib.num_files; cc++)
		f->mflib.length[cc] = ByteArray_readUnsignedInt(ba);
	
	ByteArray_set_position_rel(ba, 2 * f->mflib.num_files); // skip flags & ratio
	
	f->mflib.offset[0] = ByteArray_get_position(ba);

	for (aa = 1; aa < f->mflib.num_files; aa++) {
		f->mflib.offset[aa] = f->mflib.offset[aa - 1] + f->mflib.length[aa - 1];
		f->mflib.file_datafile[aa] = 0;
	}
	f->mflib.file_datafile[0] = 0;

	return prep_multifiles(f);
}

static int checkIndex(struct AgsFile *f, size_t index) {
	if (index >= AgsFile_getFileCount(f)) return 0;
	return 1;
}

static int checkDataIndex(struct AgsFile *f, size_t index) {
	if (index >= AgsFile_getDataFileCount(f)) return 0;
	return 1;
}

size_t AgsFile_getFileCount(struct AgsFile *f) {
	return f->mflib.num_files;
}

size_t AgsFile_getDataFileCount(struct AgsFile *f) {
	return f->mflib.num_data_files;
}

int AgsFile_getFileNumber(struct AgsFile *f, size_t index) {
	if (!checkIndex(f, index)) return -1;
	return f->mflib.file_datafile[index];
}

void AgsFile_setFileNumber(struct AgsFile *f, size_t index, int number) {
	*(unsigned char*)(&f->mflib.file_datafile[index]) = number;
}

char *AgsFile_getFileName(struct AgsFile *f, size_t index) {
	if (!checkIndex(f, index)) return 0;
	return f->mflib.filenames[index];
}

char *AgsFile_getDataFileName(struct AgsFile *f, size_t index) {
	if (!checkDataIndex(f, index)) return 0;
	return f->mflib.data_filenames[index];
}

size_t AgsFile_getFileSize(struct AgsFile *f, size_t index) {
	if (!checkIndex(f, index)) return 0;
	return f->mflib.length[index];
}

size_t AgsFile_getOffset(struct AgsFile *f, size_t index) {
	if (!checkIndex(f, index)) return 0;
	return f->mflib.offset[index];
}

static int AgsFile_seek(struct AgsFile *f, int multifileno, off_t pos) {
	return ByteArray_set_position(&f->f[multifileno], pos);
}

static ssize_t AgsFile_read(struct AgsFile *f, int multifileno, void* buf, size_t count) {
	return ByteArray_readMultiByte(&f->f[multifileno], buf, count);
}

int AgsFile_extract(struct AgsFile* f, int multifileno, off_t start, size_t len, const char* outfn) {
	FILE *fo = fopen(outfn, "wb");
	if(fo == 0) return 0;
	char buf[4096];
	size_t written = 0, l = len;
	off_t save_pos = ByteArray_get_position(&f->f[multifileno]);
	AgsFile_seek(f, multifileno, start);
	while(written < l) {
		size_t togo = l - written;
		if(togo > sizeof(buf)) togo = sizeof(buf);
		if(togo == 0) break;
		ssize_t ret = AgsFile_read(f, multifileno, buf, togo);
		if(ret <= 0) break;
		ret = fwrite(buf, 1, togo, fo);
		if(ret != togo) {
			fprintf(stderr, "short write\n");
			break;
		}
		written += togo;
	}
	fclose(fo);
	AgsFile_seek(f, multifileno, save_pos);
	return written == l;
}

int AgsFile_dump(struct AgsFile* f, size_t index, const char* outfn) {
	if (!checkIndex(f, index)) return 0;
	return AgsFile_extract(f, AgsFile_getFileNumber(f, index), AgsFile_getOffset(f, index), AgsFile_getFileSize(f, index), outfn);
}

void AgsFile_init(struct AgsFile *buf, char* filename) {
	memset(buf, 0, sizeof *buf);
	buf->fn = filename;
}

int AgsFile_open(struct AgsFile *buf) {
	int ret = csetlib(buf, buf->fn);
	
	return ret == 0;
}

