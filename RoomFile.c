#include "RoomFile.h"
#include <stdlib.h>

ssize_t ARF_find_code_start(AF* f, size_t start) {
	if(!AF_set_pos(f, start)) return -1;
	char buf[4];
	unsigned match = 0;
	while(1) {
		if(AF_read(f, buf, 1) != 1) break;;
		switch(match) {
			case 0:
				if(*buf == 'S') match++;
				else match = 0;
				break;
			case 1:
				if(*buf == 'C') match++;
				else match = 0;
				break;
			case 2:
				if(*buf == 'O') match++;
				else match = 0;
				break;
			case 3:
				if(*buf == 'M') return AF_get_pos(f) - 4;
				else match = 0;
				break;
			default:
				assert(0);
		}
	}
	return -1;
}

static void roomfile_decrypt_text(char *s, int len) {
	unsigned i = 0;
	while (i < len) {
		*s += "Avis Durgan"[i % 11];
		if (!*s) break;
		++i; ++s;
	}
}

char *RoomFile_extract_source(AF *f, struct RoomFile *r, size_t *sizep) {
	*sizep = 0;
	off_t pos = r->blockpos[BLOCKTYPE_SCRIPT];
	if(!pos || !AF_set_pos(f, pos)) return 0;
	int scriptlen = AF_read_int(f);
	assert(r->blocklen[BLOCKTYPE_SCRIPT] == scriptlen + 4);
	char* out = malloc(scriptlen+1);
	if(!out) return out;
	if((size_t) -1 == AF_read(f, out, scriptlen)) {
		free(out);
		return 0;
	}
	*sizep = scriptlen;
	roomfile_decrypt_text(out, scriptlen);
	out[scriptlen] = 0;
	return out;
}


int RoomFile_read(AF *f, struct RoomFile *r) {
	if(!AF_set_pos(f, 0)) return 0;
	r->version = AF_read_short(f);
	while(1) {
		unsigned char blocktype;
		if((size_t) -1 == AF_read(f, &blocktype, 1)) return 0;
		if(blocktype == BLOCKTYPE_EOF) break;
		if(blocktype < BLOCKTYPE_MIN || blocktype > BLOCKTYPE_MAX) return 0;
		int blocklen = AF_read_int(f);
		off_t curr_pos = AF_get_pos(f), next_block = curr_pos + blocklen;
		r->blockpos[blocktype] = curr_pos;
		r->blocklen[blocktype] = blocklen;
		switch(blocktype) {
			case BLOCKTYPE_COMPSCRIPT3:
				{
					char sig[4];
					AF_read(f, sig, 4);
					assert(!memcmp(sig, "SCOM", 4));
				}
				break;
			/* the older script types weren't supported by the released AGS sources ever */
			default:
				break;
		}
		if(!AF_set_pos(f, next_block)) return 0;
	}
	return 1;
}
