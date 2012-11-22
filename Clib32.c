/* CLIB32 - DJGPP implemention of the CLIB reader.
  (c) 1998-99 Chris Jones
  
  22/12/02 - Shawn's Linux changes approved and integrated - CJ

  v1.2 (Apr'01)  added support for new multi-file CLIB version 10 files
  v1.1 (Jul'99)  added support for appended-to-exe data files

  Adventure Game Studio source code Copyright 1999-2011 Chris Jones.
  All rights reserved.

  The AGS Editor Source Code is provided under the Artistic License 2.0
  http://www.opensource.org/licenses/artistic-license-2.0.php

  You MAY NOT compile your own builds of the engine without making it EXPLICITLY
  CLEAR that the code has been altered from the Standard Version.
  
  v1.3 code cleaned up by rofl0r for usage in agsutils.
  
*/

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
#include "Clib32.h"

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

static void clib_decrypt_text(char *toenc) {
	int adx = 0;

	while (1) {
		toenc[0] -= clibpasswencstring[adx];
		if (toenc[0] == 0)
			break;

		adx++;
		toenc++;

		if (adx > 10)
			adx = 0;
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
	
	fread_data_intarray_enc(wout, mfl->offset, mfl->num_files);
	fread_data_intarray_enc(wout, mfl->length, mfl->num_files);
	for(aa = 0; aa < mfl->num_files; aa++)
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
	fread_data_intarray(wout, mfl->offset, mfl->num_files);
	fread_data_intarray(wout, mfl->length, mfl->num_files);
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
	ByteArray_close_file(&f->f);
}

int AgsFile_getVersion(struct AgsFile *f) {
	return f->libversion;
}

static int csetlib(struct AgsFile* f, char *filename)  {
	char clbuff[20];
	if (!filename)
		return -11;
	
	int passwmodifier = 0;
	size_t aa;
	size_t cc, l;
	
	struct ByteArray *ba = &f->f;
	ByteArray_ctor(ba);
	if(!ByteArray_open_file(ba, filename)) return -1;
	ByteArray_set_endian(ba, BAE_LITTLE); // all ints etc are saved in little endian.
	off_t ba_len = ByteArray_get_length(ba);
	ByteArray_readMultiByte(ba, clbuff, 5);

	uint32_t absoffs = 0; /* we read 4 bytes- so our datatype 
	must be 4 bytes as well, since we use a pointer to it */

	if (strncmp(clbuff, "CLIB", 4) != 0) {
		ByteArray_set_position(ba,  ba_len - 12);
		ByteArray_readMultiByte(ba, clbuff, 12);

		if (strncmp(clbuff, clibendfilesig, 12) != 0)
			return -2;
		// it's an appended-to-end-of-exe thing
		ByteArray_set_position(ba,  ba_len - 16);
		absoffs = ByteArray_readUnsignedInt(ba);
		ByteArray_set_position(ba, absoffs + 5);
	}

	f->libversion = ByteArray_readUnsignedByte(ba);
	switch (f->libversion) {
		case 6: case 10: case 11: case 15: case 20: case 21:
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

		if (f->libversion >= 21) {
			if (read_new_new_enc_format_clib(&f->mflib, ba))
			return -5;
		}
		else if (f->libversion == 20) {
			if (read_new_new_format_clib(&f->mflib, ba))
			return -5;
		} else  {
			struct MultiFileLib mflibOld_b, *mflibOld = &mflibOld_b;

			if (read_new_format_clib(mflibOld, ba, f->libversion))
				return -5;
			// convert to newer format
			f->mflib.num_files = mflibOld->num_files;
			f->mflib.num_data_files = mflibOld->num_data_files;
			memcpy(f->mflib.offset, mflibOld->offset, sizeof(int) * f->mflib.num_files);
			memcpy(f->mflib.length, mflibOld->length, sizeof(int) * f->mflib.num_files);
			memcpy(f->mflib.file_datafile, mflibOld->file_datafile, sizeof(char) * f->mflib.num_files);
			for (aa = 0; aa < f->mflib.num_data_files; aa++)
				strcpy(f->mflib.data_filenames[aa], mflibOld->data_filenames[aa]);
			for (aa = 0; aa < f->mflib.num_files; aa++)
				strcpy(f->mflib.filenames[aa], mflibOld->filenames[aa]);
		}

		strcpy(f->mflib.data_filenames[0], filename);
		for (aa = 0; aa < f->mflib.num_files; aa++) {
			// correct offsetes for EXE file
			if (f->mflib.file_datafile[aa] == 0)
				f->mflib.offset[aa] += absoffs;
		}
		return 0;
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
	return 0;
}

static int checkIndex(struct AgsFile *f, size_t index) {
	if (index >= AgsFile_getCount(f)) return 0;
	return 1;
}

size_t AgsFile_getCount(struct AgsFile *f) {
	return f->mflib.num_files;
}

char *AgsFile_getFileName(struct AgsFile *f, size_t index) {
	if (!checkIndex(f, index)) return 0;
	return f->mflib.filenames[index];
}

size_t AgsFile_getFileSize(struct AgsFile *f, size_t index) {
	if (!checkIndex(f, index)) return 0;
	return f->mflib.length[index];
}

size_t AgsFile_getOffset(struct AgsFile *f, size_t index) {
	if (!checkIndex(f, index)) return 0;
	return f->mflib.offset[index];
}

int AgsFile_seek(struct AgsFile *f, off_t pos) {
	return ByteArray_set_position(&f->f, pos);
}

ssize_t AgsFile_read(struct AgsFile *f, void* buf, size_t count) {
	return ByteArray_readMultiByte(&f->f, buf, count);
}

int AgsFile_dump(struct AgsFile* f, size_t index, char* outfn) {
	if (!checkIndex(f, index)) return 0;
	int fd = open(outfn, O_WRONLY | O_CREAT | O_TRUNC, 0660);
	if(fd == -1) return 0;
	char buf[4096];
	size_t written = 0, l = AgsFile_getFileSize(f, index);
	AgsFile_seek(f, AgsFile_getOffset(f, index));
	while(written < l) {
		size_t togo = l - written;
		if(togo > sizeof(buf)) togo = sizeof(buf);
		if(togo == 0) break;
		ssize_t ret = AgsFile_read(f, buf, togo);
		if(ret <= 0) break;
		write(fd, buf, togo);
		written += togo;
	}
	close(fd);
	return written == l;
}

int AgsFile_init(struct AgsFile *buf, char* filename) {
	int ret = csetlib(buf, filename);
	
	return ret == 0;
}

