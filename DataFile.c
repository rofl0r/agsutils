#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "DataFile.h"
#include "Script.h"

void ADF_close(ADF* a) {
	AF_close(a->f);
}

ASI *ADF_get_global_script(ADF* a) {
	return &a->globalscript;
}

ASI *ADF_get_dialog_script(ADF* a) {
	return &a->dialogscript;
}

ASI *ADF_get_script(ADF* a, size_t index) {
	if(index >= a->scriptcount) return 0;
	return &a->scripts[index];
}

size_t ADF_get_scriptcount(ADF* a) {
	return a->scriptcount;
}

typedef enum interaction_type {
	it_char = 0,
	it_inventory,
	it_max
} interaction_type;

static int ADF_read_interaction(ADF *a, interaction_type t) {
	/* deserialize_interaction_scripts */
	static const size_t iter_start[it_max] = {
		[it_char] = 0,
		[it_inventory] = 1,
	};
	size_t *countmap[it_max] = { 
		[it_char] = &a->game.charactercount,
		[it_inventory] = &a->game.inventorycount,
	}, l = *countmap[t], i = iter_start[t];
	char buf[200];
	for(; i < l; i++) {
		size_t j = 0, evcnt = AF_read_uint(a->f);
		for(; j < evcnt; j++) {
			if(!AF_read_string(a->f, buf, 200)) return 0; /* function names of interaction scripts */
		}
	}
	return 1;
}

static int deserialize_command_list(ADF *a) {
	size_t l = AF_read_uint(a->f);
	AF_read_uint(a->f); /*timesrun*/
	size_t i;
	char childs[1024];
	assert(l < sizeof(childs));
	for(i = 0; i < l; i++) {
		AF_read_uint(a->f); // unused
		AF_read_uint(a->f); // type
		size_t j = 0;
		for(; j < 5; j++) {
			char buf[4];
			if(1 != AF_read(a->f, buf, 1)) // valType
				return 0;
			if(3 != AF_read(a->f, buf, 3)) // padding
				return 0;
			AF_read_uint(a->f); // val
			AF_read_uint(a->f); // extra
		}
		childs[i] = !!AF_read_uint(a->f); // children
		AF_read_uint(a->f); // parent
	}
	for(i = 0; i < l; i++) {
		if(childs[i]) deserialize_command_list(a);
	}
	return 1;
}

static int ADF_read_interaction2x(ADF *a, interaction_type t) {
	/* deserialize_new_interaction */
	size_t *countmap[it_max] = { 
		[it_char] = &a->game.charactercount,
		[it_inventory] = &a->game.inventorycount,
	}, l = *countmap[t], i = 0;
	for(; i < l; i++) {
		int response[32];
		if(AF_read_uint(a->f) != 1) continue;
		size_t evcnt = AF_read_uint(a->f);
		assert(evcnt <= 30);
		AF_read_junk(a->f, evcnt * sizeof(int)); /*event types */
		size_t j = 0;
		for(; j < evcnt; j++)
			response[j] = AF_read_int(a->f);
		for(j = 0; j < evcnt; j++)
			if(response[j]) {
				if(!deserialize_command_list(a))
					return 0;
			}
	}
	return 1;
}

static int ADF_read_gamebase(ADF *a) {
	/* acroom.h: 2881. void ReadFile() */
	size_t l;
	unsigned x;
	char game_name[52];
	l = 50 /* game name */ + 2 /*padding*/;
	if(!AF_read(a->f, game_name, l)) return 0;
	/* 100 options */
	if(!AF_read_junk(a->f, 100*4)) return 0;
	l = 256; /* 256 "paluses", unsigned char*/
	if(!AF_read_junk(a->f, l)) return 0;
	l = 256 * 4; /*sizeof color, read into defpal*/
	if(!AF_read_junk(a->f, l)) return 0;
	a->game.viewcount = AF_read_uint(a->f);
	a->game.charactercount = AF_read_uint(a->f);
	AF_read_uint(a->f); /* character of player*/
	AF_read_uint(a->f); /* totalscore*/
	a->game.inventorycount = AF_read_ushort(a->f);
	AF_read_ushort(a->f); /* padding */
	a->game.dialogcount = AF_read_uint(a->f);
	AF_read_uint(a->f); /* numdlgmessage*/
	a->game.fontcount = AF_read_uint(a->f);
	a->game.color_depth = AF_read_uint(a->f); /* color depth - offset 0x710 into game28.dta */
	AF_read_uint(a->f); /* target_win */
	AF_read_uint(a->f);/* dialog_bullet */
	AF_read_ushort(a->f);/* hotdot */
	AF_read_ushort(a->f);/* hotdotouter */
	AF_read_uint(a->f);/* uniqueid */
	AF_read_uint(a->f);/* numgui */
	a->game.cursorcount = AF_read_uint(a->f);
	x = AF_read_uint(a->f);/* default_resolution */
	if(a->version >= 43 /* 3.3.0 */ && x == 8 /* custom resolution */) {
		/* 2 ints with the custom width and height */
		if(!AF_read_junk(a->f, 8)) return 0;
	}
	AF_read_uint(a->f);/* default_lipsync_frame */
	AF_read_uint(a->f);/* invhotdotsprite */
	l = 4 * 17; /* reserved */
	if(!AF_read_junk(a->f, l)) return 0;
	a->game.globalmessagecount = 0;
	{
		int buf[500], n; /* 500 global message numbers */;
		if(!AF_read(a->f, buf, sizeof(buf))) return 0;
		for(n = 0; n < 500; ++n)
			if(buf[n]) a->game.globalmessagecount++;
	}
	a->game.hasdict = !!AF_read_int(a->f);/* dict */
	AF_read_uint(a->f);/* globalscript */
	AF_read_uint(a->f);/* chars */
	AF_read_uint(a->f);/* compiled_script */
	return 1;
}

static int ADF_read_dictionary(ADF *a) {
	/* acroom.h:1547 */
	size_t i,l = AF_read_uint(a->f);
	for(i = 0; i < l; i++) {
		/* length of encrypted string */
		size_t e = AF_read_uint(a->f);
		if(!AF_read_junk(a->f, e)) return 0;
		AF_read_short(a->f); /* wordnum */
	}
	return 1;
}

static int ADF_read_view(ADF *a) {
	size_t i, numloops = AF_read_ushort(a->f);
	for(i=0; i<numloops; ++i) {
		size_t numframes = AF_read_ushort(a->f);
		size_t l = 4 /* flags */ + numframes * (4/*pic*/+2/*xoffs*/+2/*yoffs*/+2/*speed*/+2/*padding*/+4/*flags*/+4/*sound*/+8/*reserved*/);
		if(!AF_read_junk(a->f, l)) return 0;
	}
	return 1;
}

static int ADF_read_view2x(ADF *a) {
	size_t l = 2 /* numloops */ + (16*2) /* numframes */
	           + 2 /* padding? */ + (16*4) /* loopflags */
	           + (16*20*(4/*pic*/+2/*xoffs*/+2/*yoffs*/+2/*speed*/+2/*padding*/+4/*flags*/+4/*sound*/+8/*reserved*/)) /* viewframes */;
	return AF_read_junk(a->f, l);
}

static int ADF_read_characters(ADF *a) {
#define MAX_INV             301
#define MAX_SCRIPT_NAME_LEN 20
	struct CharacterInfo {
		int   defview;
		int   talkview;
		int   view;
		int   room, prevroom;
		int   x, y, wait;
		int   flags;
		short following;
		short followinfo;
		int   idleview;
		short idletime, idleleft;
		short transparency;
		short baseline;
		int   activeinv;
		int   talkcolor;
		int   thinkview;
		short blinkview, blinkinterval;
		short blinktimer, blinkframe;
		short walkspeed_y, pic_yoffs;
		int   z;
		int   walkwait;
		short speech_anim_speed, reserved1;
		short blocking_width, blocking_height;
		int   index_id;
		short pic_xoffs, walkwaitcounter;
		short loop, frame;
		short walking, animating;
		short walkspeed, animspeed;
		short inv[MAX_INV];
		short actx, acty;
		char  name[40];
		char  scrname[MAX_SCRIPT_NAME_LEN];
		char  on;
	} character;
	size_t i;
	a->characternames = malloc(a->game.charactercount*sizeof(char*));
	a->characterscriptnames = malloc(a->game.charactercount*sizeof(char*));
	for(i=0; i<a->game.charactercount; ++i) {
		if(sizeof(character) != AF_read(a->f, &character, sizeof(character))) return 0;
		assert(strlen(character.name) < sizeof(character.name));
		assert(strlen(character.scrname) < sizeof(character.scrname));
		a->characternames[i] = strdup(character.name);
		a->characterscriptnames[i] = strdup(character.scrname);
	}
	return 1;
}

int ADF_find_datafile(const char *dir, char *fnbuf, size_t flen)
{
	size_t l = strlen(dir);
	if(l >= flen - 20) return 0;
	memcpy(fnbuf, dir, l);
	char* p = fnbuf + l;
	*p = '/'; p++;
	memcpy(p, "game28.dta", sizeof("game28.dta"));
	AF f;
	if(!AF_open(&f, fnbuf)) {
		memcpy(p, "ac2game.dta", sizeof("ac2game.dta"));
		if(!AF_open(&f, fnbuf)) return 0;
	}
	AF_close(&f);
	return 1;
}

int ADF_read_cursors(ADF* a) {
	a->cursornames = malloc(a->game.cursorcount * sizeof(char*));
	if(!a->cursornames) return 0;

	unsigned i;
	for(i=0; i<a->game.cursorcount; ++i) {
		char buf[24];
		if(24 != AF_read(a->f, buf, 24)) return 0;
		assert(buf[19] == 0);
		a->cursornames[i] = strdup(buf+10);
	}
	return 1;
}

int ADF_read_dialogtopics(ADF *a) {
	/* Common/acroom.h:2722 */
	a->dialog_codesize = malloc(a->game.dialogcount*sizeof(short));
	#define MAXTOPICOPTIONS 30
	size_t i, l; // = (150*MAXTOPICOPTIONS)+(4*MAXTOPICOPTIONS)+4+(2*MAXTOPICOPTIONS)+2+2+4+4;
	for(i=0; i<a->game.dialogcount; ++i) {
		/* optionnames + optionflags + optionscripts + entrypoints*/
		l = (150*MAXTOPICOPTIONS)+(4*MAXTOPICOPTIONS)+4+(2*MAXTOPICOPTIONS);
		if(!AF_read_junk(a->f, l)) return 0;
		unsigned short startupentrypoint = AF_read_ushort(a->f);
		a->dialog_codesize[i] = AF_read_ushort(a->f);
		if(!AF_read_junk(a->f, 8 /* numoptions+topicFlags*/)) return 0;
	}
	return 1;
}

static void dialog_decrypt_text(char *s, int len) {
	unsigned i = 0;
	while (i < len) {
		*s -= "Avis Durgan"[i % 11];
		if (!*s) break;
		++i; ++s;
	}
}

static int is_zeroterminated(char *s, size_t maxsize) {
	char *p = s, *e = s + maxsize;
	while(p < e) if(!*(p++)) return 1;
	return 0;
}

#define BASEGOBJ_SIZE 7
#define MAX_GUIOBJ_SCRIPTNAME_LEN 25
#define MAX_GUIOBJ_EVENTHANDLER_LEN 30
static int ADF_read_gui_object(ADF *a, unsigned guiver) {
	if(!AF_read_junk(a->f, BASEGOBJ_SIZE*4)) return 0;
	char buf[MAX_GUIOBJ_SCRIPTNAME_LEN];
	size_t i;
	if(guiver >= 106) {
		if(!AF_read_string(a->f, buf, sizeof buf)) return 0;
	}
	if(guiver >= 108) {
		unsigned numev = AF_read_uint(a->f);
		for(i=0; i<numev; ++i) {
			char buf[MAX_GUIOBJ_EVENTHANDLER_LEN+1];
			if(!AF_read_string(a->f, buf, sizeof buf)) return 0;
		}
	}
	return 1;
}

int ADF_read_guis(ADF *a) {
#define MAX_OBJS_ON_GUI 30
	/* Engine/acgui.cpp:1471 */
	unsigned x = AF_read_uint(a->f);
	assert(x == 0xCAFEBEEF);
	int guiver, n = AF_read_uint(a->f);
	if(n >= 100) {
		guiver = n;
		n = AF_read_uint(a->f);
	} else {
		guiver = 0;
	}
	if(n < 0 || n > 1000) return 0;
	a->guicount = n;
	a->guinames = malloc(sizeof(char*)*a->guicount);
	size_t i;
	for(i = 0; i < a->guicount; ++i) {
		char buf[16];
		if(!AF_read_junk(a->f, 4 /*vtext*/)) return 0;
		if(16 != AF_read(a->f, buf, 16)) return 0;
		if(!buf[0]) snprintf(buf, sizeof buf, "GUI%zu", i);
		assert(is_zeroterminated(buf, 16));
		a->guinames[i] = strdup(buf);
		size_t l = 20 /* clickEventHandler */ + 27*4 /* some ints */
		           +MAX_OBJS_ON_GUI*4+MAX_OBJS_ON_GUI*4;
		if(!AF_read_junk(a->f, l)) return 0;
	}
	unsigned n_buttons = AF_read_uint(a->f);

	for(i = 0; i < n_buttons; ++i) {
		if(!ADF_read_gui_object(a, guiver)) return 0;
		if(!AF_read_junk(a->f, 12*4/*pic*/+50/*text*/)) return 0;
		if(guiver >= 111) {
			if(!AF_read_junk(a->f, 4+4 /*alignment,reserved*/)) return 0;
		}
	}

	unsigned n_labels = AF_read_uint(a->f);

	for(i = 0; i < n_labels; ++i) {
		if(!ADF_read_gui_object(a, guiver)) return 0;
	}

	unsigned n_invs = AF_read_uint(a->f);
	for(i = 0; i < n_invs; ++i) {
		if(!ADF_read_gui_object(a, guiver)) return 0;
		if(guiver >= 109) {
			if(!AF_read_junk(a->f, 4*4 /*charid,itemw,itemh,topindex*/)) return 0;
		}
	}

	if(guiver >= 100) {
		unsigned n_sliders = AF_read_uint(a->f);
		for(i = 0; i < n_sliders; ++i) {
			if(!ADF_read_gui_object(a, guiver)) return 0;
		}
	}

	if(guiver >= 101) {
		unsigned n_texts = AF_read_uint(a->f);
		for(i = 0; i < n_texts; ++i) {
			if(!ADF_read_gui_object(a, guiver)) return 0;
		}
	}

	if(guiver >= 102) {
		unsigned n_lists = AF_read_uint(a->f);
		for(i = 0; i < n_lists; ++i) {
			if(!ADF_read_gui_object(a, guiver)) return 0;
		}
	}
	return 1;
}

int ADF_read_custom_property(ADF *a) {
	unsigned i, x = AF_read_uint(a->f);
	assert(x == 1);
	x = AF_read_uint(a->f); /* numprops */
	for(i=0;i<x;++i) {
		char buf[500]; /* MAX_CUSTOM_PROPERTY_VALUE_LENGTH */
		if(!AF_read_string(a->f, buf, 200)) return 0; // propname
		if(!AF_read_string(a->f, buf, 500)) return 0; // propval
	}
	return 1;
}

int ADF_open(ADF* a, const char *filename) {
	char fnbuf[512];
	size_t l, i;

	memset(a, 0, sizeof(*a));
	a->f = &a->f_b;

	if(!AF_open(a->f, filename)) return 0;

	if(30 != AF_read(a->f, fnbuf, 30)) {
		err_close:
		AF_close(a->f);
		return 0;
	}
	if(memcmp("Adventure Creator Game File v2", fnbuf, 30)) goto err_close;
	a->version = AF_read_int(a->f);
	/* here comes some version string with minor, major - we dont need it */
	/* FIXME? newer ags does this only with version >= 12 */
	/* 4 bytes containing the length of the string, followed by the string */
	l = AF_read_uint(a->f);
	if(l > 20) goto err_close;
	if(l != (size_t) AF_read(a->f, fnbuf, l)) goto err_close;

	/* main_game_file.cpp:OpenMainGameFileBase */
	if(a->version >= 48 /* kGameVersion_341 */) {
		/* latest ags now has placed an int here: number of required capabilities */
		l = AF_read_uint(a->f);
		/* followed by l int/string pairs */
		for(i=0; i<l; i++) {
			size_t len = AF_read_uint(a->f);
			if(!AF_read_junk(a->f, len)) goto err_close;
		}
	}
	if(!ADF_read_gamebase(a)) goto err_close;
	if(a->version > 32) {
		/* we're at line 11798 in Engine/ac.cpp, git revision:
		   205c56d693d903516b7b21beb454251e9489aabf - which is highly
		   recommended as the old C-style spaghetti code is more
		   readable than the current refactored OOP version. */
		l = 40 /* guid */ + 20 /*savegame extension*/ + 50 /*savegame dir*/;
		if(l != (size_t) AF_read(a->f, fnbuf, l)) goto err_close;
	}
	if(a->version < 50 /* 3.5.0 */) {
		l = a->game.fontcount * 2; /* fontflags and fontoutline arrays [each 1b/font] */
		if(!AF_read_junk(a->f, l)) goto err_close;
		if(a->version >= 48) {
			/* version == 3.4.1 have YOffset and versions >= 3.4.1.2 < 3.5.0 have YOffeset and LineSpacing ints */
			l = a->game.fontcount * 4;
			if(a->version == 49) l*=2;
			if(!AF_read_junk(a->f, l)) goto err_close;
		}
	} else {
		/* 3.5.0 has 5 ints per font, see gamesetupstruct.cpp */
		l = a->game.fontcount * 4 * 5;
		if(!AF_read_junk(a->f, l)) goto err_close;
	}
	// number of sprites
	l = a->numsprites = (a->version < 24) ? 6000 : AF_read_uint(a->f);
	a->spriteflagsstart = AF_get_pos(a->f);
	// array of spriteflags (1 char each, max: MAX_SPRITES==30000)
	if(!AF_read_junk(a->f, l)) goto err_close;
	l = 68 * a->game.inventorycount; /* sizeof(InventoryItemInfo) */
	if(!AF_read_junk(a->f, l)) goto err_close;

	if(!ADF_read_cursors(a)) goto err_close;

	if(a->version > 32) {
		if(!ADF_read_interaction(a, it_char)) goto err_close;
		if(!ADF_read_interaction(a, it_inventory)) goto err_close;
	} else {
		if(!ADF_read_interaction2x(a, it_char)) goto err_close;
		if(!ADF_read_interaction2x(a, it_inventory)) goto err_close;
		a->globalvarcount = AF_read_uint(a->f);
		l = 28 /* sizeof(InteractionVariable)*/ * a->globalvarcount;
		if(!AF_read_junk(a->f, l)) goto err_close;
	}
	if(a->game.hasdict)
		if(!ADF_read_dictionary(a)) goto err_close;
	a->scriptstart = AF_get_pos(a->f);
	if(!ASI_read_script(a->f, &a->globalscript)) goto err_close;
	if(a->version > 37)
		if(!ASI_read_script(a->f, &a->dialogscript)) goto err_close;
	if(a->version >= 31) {
		a->scriptcount = AF_read_uint(a->f);
		for(l = 0; l < a->scriptcount; l++)
			if(!ASI_read_script(a->f, &a->scripts[l])) goto err_close;
	}
	a->scriptend = AF_get_pos(a->f);

	if(a->version > 32) {
		for(l = 0; l < a->game.viewcount; l++)
			if(!ADF_read_view(a)) goto err_close;
	} else {
		for(l = 0; l < a->game.viewcount; l++)
			if(!ADF_read_view2x(a)) goto err_close;
	}

	if(a->version <= 19) {
		/* skip version <= 2.51 unknown data */
		l = AF_read_uint(a->f) * 0x204;
		if(!AF_read_junk(a->f, l)) goto err_close;
	}

	/* ... we are around line 11977 in ac.cpp at this point*/
	if(!ADF_read_characters(a)) return 0;

	if(a->version > 19 /* 2.51*/) // current AGS code says lipsync was added in 2.54==21
		if(!AF_read_junk(a->f, 50*20/*MAXLIPSYNCFRAMES*/)) goto err_close;


	if(a->version < 26) {
		for(l = 0; l < a->game.globalmessagecount; ++l) {
			char buf[512];
			if(!AF_read_string(a->f, buf, sizeof buf)) return 0;
		}
	} else {
		for(l = 0; l < a->game.globalmessagecount; ++l) {
			/* length of encrypted string */
			size_t e = AF_read_uint(a->f);
			if(!AF_read_junk(a->f, e)) return 0;
		}
	}

	if(!ADF_read_dialogtopics(a)) return 0;
	/* Engine/ac.cpp:12045 */
	a->old_dialogscripts = 0;
	if(a->version <= 37) {
		a->old_dialogscripts = malloc(a->game.dialogcount*sizeof(char*));
		for(i = 0; i<a->game.dialogcount; ++i) {
			AF_read_junk(a->f, a->dialog_codesize[i]);
			l = AF_read_int(a->f);
			char* buf = malloc(l);
			AF_read(a->f, buf, l);
			dialog_decrypt_text(buf, l);
			a->old_dialogscripts[i] = buf;
			//ASI scr;
			//ASI_read_script(a, &scr);
		}
		if(a->version <= 25) {
			// unencrypted dialog lines
			// we just seek till end marker
			// FIXME handle EOF
			int c;
			while((c = AF_read_uchar(a->f)) != 0xef);
			AF_set_pos(a->f, AF_get_pos(a->f) -1);
		} else {
			// encrypted dialog lines
			while(1) {
				l = AF_read_uint(a->f);
				if(l == 0xCAFEBEEF) break;
				if(!AF_read_junk(a->f, l)) return 0;
			}
			AF_set_pos(a->f, AF_get_pos(a->f) -4);
		}
	}

	if(!ADF_read_guis(a)) return 0;

	if(a->version >= 25) {
		unsigned x = AF_read_uint(a->f);
		assert(x == 1);
		x = AF_read_uint(a->f); /* numplugins */
		for(i = 0; i < x; ++i) {
			char buf[80];
			if(!AF_read_string(a->f, buf, sizeof buf)) return 0;
			unsigned psize = AF_read_uint(a->f);
			AF_read_junk(a->f, psize); /* plugin content */
		}
		/* CustomPropertySchema::UnSerialize */
		x = AF_read_uint(a->f);
		assert(x == 1);
		x = AF_read_uint(a->f); /* numprops */
		for(i=0; i<x; ++i) {
			char buf[500]; /* MAX_CUSTOM_PROPERTY_VALUE_LENGTH */
			if(!AF_read_string(a->f, buf, 20)) return 0; // propname
			if(!AF_read_string(a->f, buf, 100)) return 0; //propdesc
			if(!AF_read_string(a->f, buf, 500)) return 0; //defvalue
			x = AF_read_uint(a->f); /* proptype */
		}

		for(i=0; i<a->game.charactercount; ++i)
			if(!ADF_read_custom_property(a)) return 0;

		for(i=0; i<a->game.inventorycount; ++i)
			if(!ADF_read_custom_property(a)) return 0;

		a->viewnames = malloc(a->game.viewcount * sizeof(char*));
		for(i=0; i<a->game.viewcount; ++i) {
			char buf[15+1];
			if(!AF_read_string(a->f, buf, sizeof buf)) return 1;
			assert(is_zeroterminated(buf, sizeof buf));
			a->viewnames[i] = strdup(buf);
		}
		/* here follow inventory item script names and dialog script names */
	}

	/* at this point we have everything we need */
	return 1;
}


