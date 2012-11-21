/* CLIB32 - DJGPP implemention of the CLIB reader.
  (c) 1998-99 Chris Jones
  
  22/12/02 - Shawn's Linux changes approved and integrated - CJ

  v1.2 (Apr'01)  added support for new multi-file CLIB version 10 files
  v1.1 (Jul'99)  added support for appended-to-exe data files

  This is UNPUBLISHED PROPRIETARY SOURCE CODE;
  the contents of this file may not be disclosed to third parties,
  copied or duplicated in any form, in whole or in part, without
  prior express permission from Chris Jones.
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

#define CLIB_IS_INSTALLED
char clib32copyright[] = "CLIB32 v1.21 (c) 1995,1996,1998,2001,2007 Chris Jones";
char lib_file_name[255] = " ";
char base_path[255] = ".";
char original_base_filename[255];
char clbuff[20];
const int RAND_SEED_SALT = 9338638;  // must update editor agsnative.cpp if this changes
#define MAX_FILES 10000
#define MAXMULTIFILES 25

static off_t filelength(int fd) {
  struct stat st;
  fstat(fd, &st);
  return st.st_size;
}

struct MultiFileLib {
	char data_filenames[MAXMULTIFILES][20];
	size_t num_data_files;
	char filenames[MAX_FILES][25];
	unsigned offset[MAX_FILES];
	unsigned length[MAX_FILES];
	char file_datafile[MAX_FILES];        // number of datafile
	size_t num_files;
};

struct MultiFileLibNew {
	char data_filenames[MAXMULTIFILES][50];
	size_t num_data_files;
	char filenames[MAX_FILES][100];
	unsigned offset[MAX_FILES];
	unsigned length[MAX_FILES];
	char file_datafile[MAX_FILES];        // number of datafile
	size_t num_files;
};

MultiFileLibNew mflib;
NATIVESTATIC char *clibendfilesig = "CLIB\x1\x2\x3\x4SIGE";
NATIVESTATIC char *clibpasswencstring = "My\x1\xde\x4Jibzle";
NATIVESTATIC int _last_rand;

void init_pseudo_rand_gen(int seed) {
	_last_rand = seed;
}

int get_pseudo_rand() {
	return( ((_last_rand = _last_rand * 214013L
	+ 2531011L) >> 16) & 0x7fff );
}

void clib_decrypt_text(char *toenc) {
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

void fgetnulltermstring(char *sss, ByteArray *ddd, int bufsize) {
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

int fread_data_enc_byte(struct ByteArray *ba) {
	return ByteArray_readUnsignedByte(ba) - get_pseudo_rand();
}

uint32_t fread_data_enc_int(struct ByteArray *ba) {
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

void fread_data_intarray_enc(struct ByteArray *ba, unsigned* dest, size_t count) {
	size_t i = 0;
	for(; i < count; i++)
		dest[i] = fread_data_enc_int(ba);
}

void fread_data_intarray(struct ByteArray *ba, unsigned* dest, size_t count) {
	size_t i = 0;
	for(; i < count; i++)
		dest[i] = ByteArray_readInt(ba);
}

void fread_data_enc(void *data, size_t dataSize, size_t dataCount, struct ByteArray *ooo) {
	ByteArray_readMultiByte(ooo, (char*)data, dataSize * dataCount);
	unsigned char *dataChar = (unsigned char*)data;
	for (int i = 0; i < dataSize * dataCount; i++)
		dataChar[i] -= get_pseudo_rand();
}

void fgetstring_enc(char *sss, struct ByteArray *ooo, int maxLength) {
	int i = 0;
	while ((i == 0) || (sss[i - 1] != 0)) {
		sss[i] = ByteArray_readByte(ooo) - get_pseudo_rand();
		if (i < maxLength - 1) i++;
	}
}

int getw_enc(struct ByteArray *ooo) {
	return fread_data_enc_int(ooo);
}

int read_new_new_enc_format_clib(MultiFileLibNew * mfl, struct ByteArray * wout, int libver) {
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

int read_new_new_format_clib(MultiFileLibNew * mfl, struct ByteArray * wout, int libver) {
	int aa;
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

int read_new_format_clib(MultiFileLib * mfl, struct ByteArray * wout, int libver) {
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

int csetlib(char *namm, char *passw)  {
	original_base_filename[0] = 0;

	if (namm == NULL) {
		lib_file_name[0] = ' ';
		lib_file_name[1] = 0;
		return 0;
	}
	strcpy(base_path, ".");

	int passwmodifier = 0, aa;
	size_t cc, l;
	
	struct ByteArray ba_b, *ba = &ba_b; // allocate on stack so we dont have to deal with mem leaks.
	ByteArray_ctor(ba);
	if(!ByteArray_open_file(ba, namm)) return -1;
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

	int lib_version = ByteArray_readUnsignedByte(ba);
	if ((lib_version != 6) && (lib_version != 10) &&
		(lib_version != 11) && (lib_version != 15) &&
		(lib_version != 20) && (lib_version != 21))
	return -3;  // unsupported version

	char *nammwas = namm;
	// remove slashes so that the lib name fits in the buffer
	while (namm[0] == '\\' || namm[0] == '/') namm++;

	if (namm != nammwas) {
		// store complete path
		snprintf(base_path, sizeof(base_path), "%s", nammwas);
		base_path[namm - nammwas] = 0;
		l = strlen(base_path);
		if ((base_path[l - 1] == '\\') || (base_path[l - 1] == '/'))
			base_path[l - 1] = 0;
	}

	if (lib_version >= 10) {
		if (ByteArray_readUnsignedByte(ba) != 0)
			return -4;  // not first datafile in chain

		if (lib_version >= 21) {
			if (read_new_new_enc_format_clib(&mflib, ba, lib_version))
			return -5;
		}
		else if (lib_version == 20) {
			if (read_new_new_format_clib(&mflib, ba, lib_version))
			return -5;
		} else  {
			// PSP: Allocate struct on the heap to avoid overflowing the stack.
			MultiFileLib* mflibOld = (MultiFileLib*)malloc(sizeof(MultiFileLib));

			if (read_new_format_clib(mflibOld, ba, lib_version))
			return -5;
			// convert to newer format
			mflib.num_files = mflibOld->num_files;
			mflib.num_data_files = mflibOld->num_data_files;
			memcpy(mflib.offset, mflibOld->offset, sizeof(int) * mflib.num_files);
			memcpy(mflib.length, mflibOld->length, sizeof(int) * mflib.num_files);
			memcpy(mflib.file_datafile, mflibOld->file_datafile, sizeof(char) * mflib.num_files);
			for (aa = 0; aa < mflib.num_data_files; aa++)
			strcpy(mflib.data_filenames[aa], mflibOld->data_filenames[aa]);
			for (aa = 0; aa < mflib.num_files; aa++)
			strcpy(mflib.filenames[aa], mflibOld->filenames[aa]);

			free(mflibOld);
		}

		ByteArray_close_file(ba);
		strcpy(lib_file_name, namm);

		// make a backup of the original file name
		strcpy(original_base_filename, mflib.data_filenames[0]);
		strlwr(original_base_filename);

		strcpy(mflib.data_filenames[0], namm);
		for (aa = 0; aa < mflib.num_files; aa++) {
			// correct offsetes for EXE file
			if (mflib.file_datafile[aa] == 0)
			mflib.offset[aa] += absoffs;
		}
		return 0;
	}

	passwmodifier = ByteArray_readUnsignedByte(ba);
	ByteArray_readUnsignedByte(ba); // unused byte
	mflib.num_data_files = 1;
	strcpy(mflib.data_filenames[0], namm);

	short tempshort = ByteArray_readShort(ba);
	mflib.num_files = tempshort;

	if (mflib.num_files > MAX_FILES) return -4;

	ByteArray_readMultiByte(ba, clbuff, 13);  // skip password dooberry
	for (aa = 0; aa < mflib.num_files; aa++) {
		ByteArray_readMultiByte(ba, mflib.filenames[aa], 13);
		l = strlen(mflib.filenames[aa]);
		for (cc = 0; cc < l; cc++)
			mflib.filenames[aa][cc] -= passwmodifier;
	}
	for(cc = 0; cc < mflib.num_files; cc++)
		mflib.length[cc] = ByteArray_readUnsignedInt(ba);
	
	ByteArray_set_position_rel(ba, 2 * mflib.num_files); // skip flags & ratio
	
	mflib.offset[0] = ByteArray_get_position(ba);
	strcpy(lib_file_name, namm);
	ByteArray_close_file(ba);

	for (aa = 1; aa < mflib.num_files; aa++) {
		mflib.offset[aa] = mflib.offset[aa - 1] + mflib.length[aa - 1];
		mflib.file_datafile[aa] = 0;
	}
	mflib.file_datafile[0] = 0;
	return 0;
}

int clibGetNumFiles() {
	if (lib_file_name[0] == ' ') return 0;
	return mflib.num_files;
}

const char *clibGetFileName(int index) {
	if (lib_file_name[0] == ' ') return 0;

	if ((index < 0) || (index >= mflib.num_files)) return 0;

	return mflib.filenames[index];
}

int clibfindindex(char *fill) {
	if (lib_file_name[0] == ' ') return -1;

	size_t bb;
	for (bb = 0; bb < mflib.num_files; bb++) {
		if (stricmp(mflib.filenames[bb], fill) == 0)
			return bb;
	}
	return -1;
}

int clibfilesize(char *fill) {
	int idxx = clibfindindex(fill);
	if (idxx >= 0) return mflib.length[idxx];
	return -1;
}

int cliboffset(char *fill) {
	int idxx = clibfindindex(fill);
	if (idxx >= 0)
		return mflib.offset[idxx];
	return -1;
}

const char *clibgetoriginalfilename() {
	return original_base_filename;
}

char actfilename[250];
char *clibgetdatafile(char *fill) {
	int idxx = clibfindindex(fill);
	if (idxx >= 0) {
		#if defined(LINUX_VERSION) || defined(MAC_VERSION) 
		sprintf(actfilename, "%s/%s", base_path, mflib.data_filenames[mflib.file_datafile[idxx]]);
		#else
		sprintf(actfilename, "%s\\%s", base_path, mflib.data_filenames[mflib.file_datafile[idxx]]);
		#endif
		return actfilename;
	}
	return 0;
}

FILE *tfil;
FILE *clibopenfile(char *filly, char *readmode) {
	int bb;
	for (bb = 0; bb < mflib.num_files; bb++) {
		if (stricmp(mflib.filenames[bb], filly) == 0) {
			char actfilename[250];
		#if defined(ANDROID_VERSION)
			sprintf(actfilename, "%s/%s", base_path, mflib.data_filenames[mflib.file_datafile[bb]]);
		#else
			sprintf(actfilename, "%s\\%s", base_path, mflib.data_filenames[mflib.file_datafile[bb]]);
		#endif
			tfil = ci_fopen(actfilename, readmode);
			if (tfil == NULL)
			return NULL;
			fseek(tfil, mflib.offset[bb], SEEK_SET);
			return tfil;
		}
	}
	return ci_fopen(filly, readmode);
}

#define PR_DATAFIRST 1
#define PR_FILEFIRST 2
int cfopenpriority = PR_DATAFIRST;

FILE *clibfopen(char *filnamm, char *fmt) {
	last_opened_size = -1;
	if (cfopenpriority == PR_FILEFIRST) {
		// check for file, otherwise use datafile
		if (fmt[0] != 'r') {
			tfil = ci_fopen(filnamm, fmt);
		} else {
			tfil = ci_fopen(filnamm, fmt);

			if ((tfil == NULL) && (lib_file_name[0] != ' ')) {
				tfil = clibopenfile(filnamm, fmt);
				last_opened_size = clibfilesize(filnamm);
			}
		}

	} else {
		// check datafile first, then scan directory
		if ((cliboffset(filnamm) < 1) | (fmt[0] != 'r'))
			tfil = ci_fopen(filnamm, fmt);
		else {
			tfil = clibopenfile(filnamm, fmt);
			last_opened_size = clibfilesize(filnamm);
		}

	}

	if ((last_opened_size < 0) && (tfil != NULL))
		last_opened_size = filelength(fileno(tfil));

	return tfil;
}
