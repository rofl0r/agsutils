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

enum BlockType {
	BLOCKTYPE_MAIN = 1,
	BLOCKTYPE_MIN = BLOCKTYPE_MAIN,
	BLOCKTYPE_SCRIPT = 2,
	BLOCKTYPE_COMPSCRIPT = 3,
	BLOCKTYPE_COMPSCRIPT2 = 4,
	BLOCKTYPE_OBJECTNAMES = 5,
	BLOCKTYPE_ANIMBKGRND = 6,
	BLOCKTYPE_COMPSCRIPT3 = 7, /* only bytecode script type supported by released engine code */
	BLOCKTYPE_PROPERTIES = 8,
	BLOCKTYPE_OBJECTSCRIPTNAMES = 9,
	BLOCKTYPE_MAX = BLOCKTYPE_OBJECTSCRIPTNAMES,
	BLOCKTYPE_EOF = 0xFF
};

static void roomfile_decrypt_text(char *s, int len) {
	unsigned i = 0;
	while (i < len) {
		*s += "Avis Durgan"[i % 11];
		if (!*s) break;
		++i; ++s;
	}
}


int RoomFile_read(AF *f, struct RoomFile *r, int flags) {
	if(!AF_set_pos(f, 0)) return 0;
	r->version = AF_read_short(f);
	while(1) {
		unsigned char blocktype;
		if((size_t) -1 == AF_read(f, &blocktype, 1)) return 0;
		if(blocktype == BLOCKTYPE_EOF) break;
		if(blocktype < BLOCKTYPE_MIN || blocktype > BLOCKTYPE_MAX) return 0;
		int blocklen = AF_read_int(f);
		off_t curr_pos = AF_get_pos(f), next_block = curr_pos + blocklen;
		switch(blocktype) {
			case BLOCKTYPE_SCRIPT:
				if(flags & RF_FLAGS_EXTRACT_CODE) {
					int scriptlen = AF_read_int(f);
					assert(blocklen == scriptlen + 4);
					r->sourcecode = malloc(scriptlen+1);
					if((size_t) -1 == AF_read(f, r->sourcecode, scriptlen)) return 0;
					roomfile_decrypt_text(r->sourcecode, scriptlen);
					r->sourcecode[scriptlen] = 0;
					r->sourcecode_len = scriptlen;
				}
				break;
			case BLOCKTYPE_COMPSCRIPT3:
				{
					char sig[4];
					AF_read(f, sig, 4);
					assert(!memcmp(sig, "SCOM", 4));
					r->scriptpos = curr_pos;
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
