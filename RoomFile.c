#include "RoomFile.h"

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
