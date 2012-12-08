#define _GNU_SOURCE
#include "DataFile.h"
#include "RoomFile.h"
#include "ByteArray.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define VERSION "0.0.1"
#define ADS ":::AGSinject " VERSION " by rofl0r:::"

__attribute__((noreturn))
void usage(char *argv0) {
	dprintf(2, 
	ADS "\nusage:\n%s index input.o inject_to.crm\n"
	"index is the number of script to replace, i.e. 0 for first script\n"
	"only relevant if the output file is a gamefile which contains multiple scripts\n"
	"for example gamescript is 0, dialogscript is 1 (if existing), etc\n"
	"a room file (.crm) only has one script so you can pass 0.\n", argv0);
	exit(1);
}

static void inject(char *o, char *inj, unsigned which) {
	//ARF_find_code_start
	AF f_b, *f = &f_b;
	size_t index = 0;
	size_t found = 0;
	int isroom = (which == 0 && !strcmp(".crm", inj + strlen(inj) - 4));
	if(AF_open(f, inj)) {
		ssize_t start;
		while(1) {
			if((start = ARF_find_code_start(f, index)) == -1) {
				dprintf(2, "error, only %zu scripts found\n", found);
				exit(1);
			}
			if(found == which) {
				AF_dump_chunk(f, 0, isroom ? start -4 : start, "/tmp/ags_chunk1.chnk");
				AF_set_pos(f, start);
				if(isroom) {
					/* room files, unlike game files, have a length field of size 4 before
					 * the compiled script starts. */
					struct ByteArray b;
					ByteArray_ctor(&b);
					ByteArray_open_file(&b, o);
					unsigned l = ByteArray_get_length(&b);
					ByteArray_close_file(&b);
					ByteArray_ctor(&b);
					ByteArray_open_mem(&b, 0, 0);
					ByteArray_set_flags(&b, BAF_CANGROW);
					ByteArray_set_endian(&b, BAE_LITTLE);
					ByteArray_writeInt(&b, l);
					ByteArray_dump_to_file(&b, "/tmp/ags_size.chunk");
					// TODO close/free b
				}
				ASI s;
				if(!ASI_read_script(f, &s)) {
					dprintf(2, "trouble finding script in %s\n", inj);
					exit(1);
				}
				AF_dump_chunk(f, start + s.len, ByteArray_get_length(f->b) - (start + s.len),
					      "/tmp/ags_chunk2.chnk");
				AF_close(f);
				char buf[1024];
				if(isroom)
					snprintf(buf, sizeof(buf), 
						 "cat /tmp/ags_chunk1.chnk /tmp/ags_size.chunk %s /tmp/ags_chunk2.chnk > %s", o, inj);
				else 
					snprintf(buf, sizeof(buf), 
					 "cat /tmp/ags_chunk1.chnk %s /tmp/ags_chunk2.chnk > %s", o, inj);
				system(buf);
				
				return;
			}
			
			found++;
			index = start + 4;
		}
		
	} else {
		perror(inj);
	}
}

int main(int argc, char**argv) {
	if(argc != 4) usage(argv[0]);
	char *o = argv[2], *inj = argv[3];
	int which = atoi(argv[1]);
	inject(o, inj, which);
	return 0;
}