#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "DataFile.h"
#include "Script.h"

#ifdef _WIN32
#define PSEP '\\'
#else
#define PSEP '/'
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

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
	size_t l = t == it_char ? a->game.charactercount : a->game.inventorycount;
	size_t i = t == it_char ? 0 : 1;
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
	size_t l = t == it_char ? a->game.charactercount : a->game.inventorycount;
	size_t i = 0;
	for(; i < l; i++) {
		int response[32];
		if(AF_read_uint(a->f) != 1) continue;
		size_t evcnt = AF_read_uint(a->f);
		if(evcnt > 30) return 0;
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
	a->game.dialogcount = AF_read_uint(a->f); /* numdialog */
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
		if(strlen(character.name) >= sizeof(character.name)) return 0;
		if(strlen(character.scrname) >= sizeof(character.scrname)) return 0;
		a->characternames[i] = strdup(character.name);
		a->characterscriptnames[i] = strdup(character.scrname);
	}
	return 1;
}

int ADF_find_datafile(const char *dir, char *fnbuf, size_t flen)
{
	static const struct {const char name[12];} gfn[]= {
	"game28.dta", "ac2game.dta", "AC2GAME.DTA"
	};
	size_t i, l = strlen(dir);
	if(l >= flen - 20) return 0;
	memcpy(fnbuf, dir, l);
	char* p = fnbuf + l;
	*p = PSEP; p++;
	for(i = 0; i < ARRAY_SIZE(gfn); ++i) {
		strcpy(p, gfn[i].name);
		AF f;
		if(!AF_open(&f, fnbuf)) continue;
		AF_close(&f);
		return 1;
	}
	return 0;
}

int ADF_read_cursors(ADF* a) {
	a->cursornames = malloc(a->game.cursorcount * sizeof(char*));
	if(!a->cursornames) return 0;

	unsigned i;
	for(i=0; i<a->game.cursorcount; ++i) {
		char buf[24];
		if(24 != AF_read(a->f, buf, 24)) return 0;
		if(buf[19] != 0) return 0;
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

#define MAX_GUIOBJ_SCRIPTNAME_LEN 25
#define MAX_GUIOBJ_EVENTHANDLER_LEN 30
static int ADF_read_gui_object(ADF *a, unsigned guiver) {
	if(!AF_read_junk(a->f, guiver >= 119 ? 6*4 : 7*4)) return 0;
	char buf[512]; /* arbitrary limit.
		ags commit ed06eb64 made away with any length restrictions;
		that was done when kGuiVersion_340 = 118 was current. */
	size_t i;
	if(guiver >= 106) {
		if(!AF_read_string(a->f, buf, guiver >= 118 ? sizeof buf : MAX_GUIOBJ_SCRIPTNAME_LEN)) return 0;
	}
	if(guiver >= 108) {
		unsigned numev = AF_read_uint(a->f);
		for(i=0; i<numev; ++i) {
			if(!AF_read_string(a->f, buf, guiver >= 118 ? sizeof buf : MAX_GUIOBJ_EVENTHANDLER_LEN+1)) return 0;
		}
	}
	return 1;
}

static int AF_read_string_with_length(AF *f, char *buf, size_t maxlen) {
	unsigned len = AF_read_uint(f);
	if(len >= maxlen) return 0;
	if(len && !AF_read(f, buf, len)) return 0;
	buf[len] = 0;
	return 1;
}

int ADF_read_guis(ADF *a) {
#define MAX_OBJS_ON_GUI 30
#define FAIL() goto fail
#if 0
	volatile unsigned fail_line = 0;
#undef FAIL
#define FAIL() do { fail_line = __LINE__; goto fail; } while(0)
#endif
	/* Engine/acgui.cpp:1471 */
	unsigned x = AF_read_uint(a->f);
	if(x != 0xCAFEBEEF) return 0;
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
		char buf[512];
		if(guiver < 119 /* 3.5.0 */) {
			if(!AF_read_junk(a->f, 4 /*vtext*/)) FAIL();
		}
		if(guiver >= 118) /* 3.4.0 */ {
			if(!AF_read_string_with_length(a->f, buf, sizeof buf)) FAIL();
		} else {
			if(16 != AF_read(a->f, buf, 16)) FAIL();
			if(!is_zeroterminated(buf, 16)) FAIL();
		}
		if(!buf[0]) snprintf(buf, sizeof buf, "GUI%zu", i);
		a->guinames[i] = strdup(buf);
		if(guiver >= 118) {
			if(!AF_read_string_with_length(a->f, buf, sizeof buf)) FAIL();
		} else {
			if(!AF_read_junk(a->f, 20 /* clickEventHandler */)) FAIL();
		}
		if(!AF_read_junk(a->f, guiver<119 ? 5*4 : 4*4 )) FAIL();
		unsigned n_ctrls = AF_read_uint(a->f);
		if(guiver < 118)
			n_ctrls = MAX_OBJS_ON_GUI;
		// have read 6 of 27 legacy ints at this point

		size_t l;
		if(guiver >= 119)
			l = (10+n_ctrls) * 4;
		else if(guiver >= 118)
			l = (21+n_ctrls) * 4;
		else
			l = 21*4 /* some ints */
		           +MAX_OBJS_ON_GUI*4+MAX_OBJS_ON_GUI*4;
		if(!AF_read_junk(a->f, l)) FAIL();
	}
	unsigned n_buttons = AF_read_uint(a->f);

	for(i = 0; i < n_buttons; ++i) {
		if(!ADF_read_gui_object(a, guiver)) FAIL();
		char buf[512];
		if(!AF_read_junk(a->f, guiver>=119? 9*4 : 12*4/*pic*/)) FAIL();
		if(guiver < 119) {
			if(!AF_read(a->f, buf, 50 /*text*/)) FAIL(); // for debugging
		} else {
			if(!AF_read_string_with_length(a->f, buf, sizeof buf)) FAIL();
		}
		if(guiver >= 119) {
			if(!AF_read_junk(a->f, 4 /*alignment*/)) FAIL();
		} else if(guiver >= 111) {
			if(!AF_read_junk(a->f, 4+4 /*alignment,reserved*/)) FAIL();
		}
	}

	unsigned n_labels = AF_read_uint(a->f);

	for(i = 0; i < n_labels; ++i) {
		if(!ADF_read_gui_object(a, guiver)) FAIL();
		unsigned textlen = guiver < 113 ? 200 : AF_read_uint(a->f);
		if(!AF_read_junk(a->f, textlen + 4*3/*font*/)) FAIL();
	}

	unsigned n_invs = AF_read_uint(a->f);
	for(i = 0; i < n_invs; ++i) {
		if(!ADF_read_gui_object(a, guiver)) FAIL();
		if(guiver >= 109) {
			if(!AF_read_junk(a->f, guiver >= 119 ? 3*4 : 4*4 /*charid,itemw,itemh,topindex*/)) FAIL();
		}
	}

	if(guiver >= 100) {
		unsigned n_sliders = AF_read_uint(a->f);
		for(i = 0; i < n_sliders; ++i) {
			if(!ADF_read_gui_object(a, guiver)) FAIL();
			unsigned n;
			if(guiver >= 119) n = 6;
			else if(guiver >= 104) n = 7;
			else n = 4;
			if(!AF_read_junk(a->f, 4*n)) FAIL();
		}
	}

	if(guiver >= 101) {
		unsigned n_texts = AF_read_uint(a->f);
		for(i = 0; i < n_texts; ++i) {
			if(!ADF_read_gui_object(a, guiver)) FAIL();
			if(guiver >= 119) {
				char buf[2048];
				if(!AF_read_string_with_length(a->f, buf, sizeof buf)) FAIL();
				if(!AF_read_junk(a->f, 3*4/*font*/)) FAIL();
			} else {
				if(!AF_read_junk(a->f, 200/*text*/+3*4/*font*/)) FAIL();
			}
		}
	}

	if(guiver >= 102) {
		unsigned n_lists = AF_read_uint(a->f);
		size_t l_add1 = guiver >= 119 ? 4 : (guiver >= 112 ? 8 : 0);
		size_t l_add2 = guiver >= 107 ? 4 : 0;
		for(i = 0; i < n_lists; ++i) {
			if(!ADF_read_gui_object(a, guiver)) FAIL();
			unsigned j, n_items = AF_read_uint(a->f);
			if(!AF_read_junk(a->f, guiver>=119 ? 3*4 : 9*4)) FAIL();
			unsigned flags = AF_read_uint(a->f);
			if((l_add1 + l_add2) && !AF_read_junk(a->f, l_add1 + l_add2)) FAIL();
			for(j = 0; j < n_items; ++j) {
				char buf[1024];
				if(!AF_read_string(a->f, buf, sizeof buf)) FAIL();
			}
			if(guiver >= 114 && guiver < 119 && (flags & 4/* GLF_SGINDEXVALID */)) {
				if(!AF_read_junk(a->f, n_items*2)) FAIL();
			}
		}
	}
	return 1;
fail:
	free(a->guinames);
	a->guinames = 0;
	a->guicount = 0;
	return 0;
#undef FAIL
}

int ADF_read_custom_property(ADF *a) {
	unsigned i, x, propver = AF_read_uint(a->f);
	if(propver > 2) return 0;
	x = AF_read_uint(a->f); /* numprops */
	for(i=0;i<x;++i) {
		if(propver == 1) {
			char buf[500]; /* MAX_CUSTOM_PROPERTY_VALUE_LENGTH */
			if(!AF_read_string(a->f, buf, 200)) return 0; // propname
			if(!AF_read_string(a->f, buf, 500)) return 0; // propval
		} else {
			char buf[2048];
			if(!AF_read_string_with_length(a->f, buf, sizeof buf)) return 0;
			if(!AF_read_string_with_length(a->f, buf, sizeof buf)) return 0;
		}
	}
	return 1;
}

const char *AOE2str(enum ADF_open_error e) {
	static const char err[][12] = {
	[AOE_success] = "success",
	[AOE_open] = "open",
	[AOE_read] = "read",
	[AOE_sig] = "signature",
	[AOE_header] = "header",
	[AOE_gamebase] = "gamebase",
	[AOE_cursors] = "cursors",
	[AOE_interaction] = "interaction",
	[AOE_dictionary] = "dictionary",
	[AOE_script] = "script",
	[AOE_view] = "view",
	[AOE_viewjunk] = "viewjunk",
	[AOE_character] = "character",
	[AOE_lipsync] = "lipsync",
	[AOE_msg] = "message",
	[AOE_dialogtopic] = "dialogtopic",
	[AOE_dialog] = "dialog",
	[AOE_guis] = "guis",
	[AOE_props] = "props",
	[AOE_views] = "views",
	[AOE_inventories] = "inventories",
	};
	return err[e];
}

enum ADF_open_error ADF_open(ADF* a, const char *filename) {
#define FAIL(E) do { aoe = E; goto err_close; } while(0)
#define ERR(E) do { return E; } while(0)
	enum ADF_open_error aoe = AOE_success;

	char fnbuf[512];
	size_t l, i;

	memset(a, 0, sizeof(*a));
	a->f = &a->f_b;

	if(!AF_open(a->f, filename)) return AOE_open;

	if(30 != AF_read(a->f, fnbuf, 30)) {
		aoe = AOE_read;
		err_close:
		AF_close(a->f);
		return aoe;
	}
	if(memcmp("Adventure Creator Game File v2", fnbuf, 30)) FAIL(AOE_sig);
	/* the version read here is called kGameVersion_xxx in recent AGS
	   engine codebase, e.g. kGameVersion_350 == 50 */
	a->version = AF_read_int(a->f);
	/* here comes some version string with minor, major - we dont need it */
	/* FIXME? newer ags does this only with version >= 12 */
	/* 4 bytes containing the length of the string, followed by the string */
	l = AF_read_uint(a->f);
	if(l > 20) FAIL(AOE_header);
	if(l != (size_t) AF_read(a->f, fnbuf, l)) goto err_close;

	/* main_game_file.cpp:OpenMainGameFileBase */
	if(a->version >= 48 /* kGameVersion_341 */) {
		/* latest ags now has placed an int here: number of required capabilities */
		l = AF_read_uint(a->f);
		/* followed by l int/string pairs */
		for(i=0; i<l; i++) {
			size_t len = AF_read_uint(a->f);
			if(!AF_read_junk(a->f, len)) FAIL(AOE_header);
		}
	}
	if(!ADF_read_gamebase(a)) FAIL(AOE_gamebase);
	if(a->version > 32) {
		/* we're at line 11798 in Engine/ac.cpp, git revision:
		   205c56d693d903516b7b21beb454251e9489aabf - which is highly
		   recommended as the old C-style spaghetti code is more
		   readable than the current refactored OOP version. */
		l = 40 /* guid */ + 20 /*savegame extension*/ + 50 /*savegame dir*/;
		if(l != (size_t) AF_read(a->f, fnbuf, l)) FAIL(AOE_gamebase);
	}
	if(a->version < 50 /* 3.5.0 */) {
		l = a->game.fontcount * 2; /* fontflags and fontoutline arrays [each 1b/font] */
		if(!AF_read_junk(a->f, l)) FAIL(AOE_gamebase);
		if(a->version >= 48) {
			/* version == 3.4.1 have YOffset and versions >= 3.4.1.2 < 3.5.0 have YOffeset and LineSpacing ints */
			l = a->game.fontcount * 4;
			if(a->version == 49) l*=2;
			if(!AF_read_junk(a->f, l)) FAIL(AOE_gamebase);
		}
	} else {
		/* 3.5.0 has 5 ints per font, see gamesetupstruct.cpp */
		l = a->game.fontcount * 4 * 5;
		if(!AF_read_junk(a->f, l)) FAIL(AOE_gamebase);
	}
	// number of sprites
	l = a->numsprites = (a->version < 24) ? 6000 : AF_read_uint(a->f);
	a->spriteflagsstart = AF_get_pos(a->f);
	// array of spriteflags (1 char each, max: MAX_SPRITES==30000)
	if(!AF_read_junk(a->f, l)) FAIL(AOE_gamebase);
	l = 68 * a->game.inventorycount; /* sizeof(InventoryItemInfo) */
	if(!AF_read_junk(a->f, l)) FAIL(AOE_gamebase);

	if(!ADF_read_cursors(a)) FAIL(AOE_cursors);

	if(a->version > 32) {
		if(!ADF_read_interaction(a, it_char)) FAIL(AOE_interaction);
		if(!ADF_read_interaction(a, it_inventory)) FAIL(AOE_interaction);
	} else {
		if(!ADF_read_interaction2x(a, it_char)) FAIL(AOE_interaction);
		if(!ADF_read_interaction2x(a, it_inventory)) FAIL(AOE_interaction);
		a->globalvarcount = AF_read_uint(a->f);
		l = 28 /* sizeof(InteractionVariable)*/ * a->globalvarcount;
		if(!AF_read_junk(a->f, l)) FAIL(AOE_interaction);
	}
	if(a->game.hasdict)
		if(!ADF_read_dictionary(a)) FAIL(AOE_dictionary);
	a->scriptstart = AF_get_pos(a->f);
	if(!ASI_read_script(a->f, &a->globalscript)) FAIL(AOE_script);
	if(a->version > 37)
		if(!ASI_read_script(a->f, &a->dialogscript)) FAIL(AOE_script);
	if(a->version >= 31) {
		a->scriptcount = AF_read_uint(a->f);
		for(l = 0; l < a->scriptcount; l++)
			if(!ASI_read_script(a->f, &a->scripts[l])) FAIL(AOE_script);
	}
	a->scriptend = AF_get_pos(a->f);

	if(a->version > 32) {
		for(l = 0; l < a->game.viewcount; l++)
			if(!ADF_read_view(a)) FAIL(AOE_view);
	} else {
		for(l = 0; l < a->game.viewcount; l++)
			if(!ADF_read_view2x(a)) FAIL(AOE_view);
	}

	/*
	   from here on, we have the important bits and everything else is
	   "bonus", so we don't hard-fail anymore but skip the non-essential
	   parts who rely on the below info. */

	a->f->b->flags |= BAF_NONFATAL_READ_OOB;

	if(a->version <= 19) {
		/* skip version <= 2.51 unknown data */
		l = AF_read_uint(a->f) * 0x204;
		if(!AF_read_junk(a->f, l)) ERR(AOE_viewjunk);
	}

	/* ... we are around line 11977 in ac.cpp at this point*/

	if(!ADF_read_characters(a)) ERR(AOE_character);

	if(a->version > 20 /* 2.54+*/)
		if(!AF_read_junk(a->f, 50*20/*MAXLIPSYNCFRAMES*/)) ERR(AOE_lipsync);


	if(a->version < 26) {
		char buf[512];
		for(l = 0; l < a->game.globalmessagecount; ++l) {
			if(!AF_read_string(a->f, buf, sizeof buf)) ERR(AOE_msg);
		}
	} else {
		for(l = 0; l < a->game.globalmessagecount; ++l) {
			/* length of encrypted string */
			size_t e = AF_read_uint(a->f);
			if(!AF_read_junk(a->f, e)) ERR(AOE_msg);
		}
	}

	if(!ADF_read_dialogtopics(a)) ERR(AOE_dialogtopic);
	/* Engine/ac.cpp:12045 */
	a->old_dialogscripts = 0;
	if(a->version <= 37) {
		/* 24 bits should be the upper limit of a reasonable dialogcount,
		   else what we read was junk intended for a different purpose */
		if((a->game.dialogcount & 0xff000000) ||
		  !(a->old_dialogscripts = malloc(a->game.dialogcount*sizeof(char*))))
			ERR(AOE_dialog);
		for(i = 0; i<a->game.dialogcount; ++i) {
			AF_read_junk(a->f, a->dialog_codesize[i]);
			l = AF_read_int(a->f);
			char* buf;
			if((l & 0xff000000) || !(buf = malloc(l))) ERR(AOE_dialog);
			AF_read(a->f, buf, l);
			dialog_decrypt_text(buf, l);
			a->old_dialogscripts[i] = buf;
			//ASI scr;
			//ASI_read_script(a, &scr);
		}
		if(a->version <= 25) {
			// unencrypted dialog lines
			// we just seek till end marker
			int c;
			while((c = AF_read_uchar(a->f)) != 0xef)
				if(AF_is_eof(a->f)) ERR(AOE_dialog);
			AF_set_pos(a->f, AF_get_pos(a->f) -1);
		} else {
			// encrypted dialog lines
			while(1) {
				l = AF_read_uint(a->f);
				if(l == 0xCAFEBEEF) break;
				if(!AF_read_junk(a->f, l)) ERR(AOE_dialog);
			}
			AF_set_pos(a->f, AF_get_pos(a->f) -4);
		}
	}

	if(!ADF_read_guis(a)) ERR(AOE_guis);

	/* both properties and viewname stuff later on were added in 2.60 */
	if(a->version >= 25 /* kGameVersion_260 */) {
		unsigned prop_version, x = AF_read_uint(a->f);
		if(x != 1) ERR(AOE_props);
		x = AF_read_uint(a->f); /* numplugins */
		for(i = 0; i < x; ++i) {
			char buf[80];
			if(!AF_read_string(a->f, buf, sizeof buf)) ERR(AOE_props);
			unsigned psize = AF_read_uint(a->f);
			if(!AF_read_junk(a->f, psize)); /* plugin content */
				ERR(AOE_props);
		}
		/* CustomPropertySchema::UnSerialize */
		prop_version = AF_read_uint(a->f);
		if(prop_version > 2) ERR(AOE_props);
		x = AF_read_uint(a->f); /* numprops */
		for(i=0; i<x; ++i) {
			if(prop_version == 1) {
				char buf[500]; /* MAX_CUSTOM_PROPERTY_VALUE_LENGTH */
				/* note: that 20 might actually be 200,
				   because the AGS code reserves 200 for propname,
				   but then reads only 20 */
				/* new AGS has a comment about it:
				   NOTE: for some reason the property name stored in schema object was limited to only 20 characters, while the custom properties map could hold up to 200. Whether this was an error or design choice is unknown. */
				if(!AF_read_string(a->f, buf, 20)) ERR(AOE_props); // propname
				if(!AF_read_string(a->f, buf, 100)) ERR(AOE_props); //propdesc
				if(!AF_read_string(a->f, buf, 500)) ERR(AOE_props); //defvalue
				int foo = AF_read_uint(a->f); /* proptype */
			} else {
				char buf[2048];
				if(!AF_read_string_with_length(a->f, buf, sizeof buf)) ERR(AOE_props);
				AF_read_uint(a->f);
				if(!AF_read_string_with_length(a->f, buf, sizeof buf)) ERR(AOE_props);
				if(!AF_read_string_with_length(a->f, buf, sizeof buf)) ERR(AOE_props);
			}
		}

		for(i=0; i<a->game.charactercount; ++i)
			if(!ADF_read_custom_property(a)) ERR(AOE_props);

		for(i=0; i<a->game.inventorycount; ++i)
			if(!ADF_read_custom_property(a)) ERR(AOE_props);

	}
	if(a->version >= 25 /* kGameVersion_260 */) {
		if((a->game.viewcount & 0xff000000) ||
		   !(a->viewnames = malloc(a->game.viewcount * sizeof(char*))))
			ERR(AOE_views);
		for(i=0; i<a->game.viewcount; ++i) {
			char buf[255+1]; /* was originally MAXVIEWNAMELENGTH aka 15 bytes, however "meaningless name restrictions for Views and InvItems" was removed in 8bb1bfa30610f3c3291029b2c05e6e5b37415269 */
			if(!AF_read_string(a->f, buf, sizeof buf)) ERR(AOE_views);
			if(!is_zeroterminated(buf, sizeof buf)) ERR(AOE_views);
			a->viewnames[i] = strdup(buf);
		}
	}
	/* ags version 2.55 (and earlier?) at this point have only 1 field left,
	   plugin data version (32bit) == 1, and then num plugins and eventually
	   some more data, if num plugins is 0 then that's the EOF. */

	if(a->version >= 31) { // 2.7.0
		if((a->game.inventorycount & 0xff000000) ||
		  !(a->inventorynames = malloc(a->game.inventorycount*sizeof(char*))))
			ERR(AOE_inventories);
		for(i=0; i<a->game.inventorycount; ++i) {
			char buf[256]; /* arbitrary size chosen to hold a very long string */
			/* somewhere between 3.4.0 and 3.4.1 the 20 char limit was removed */
			if(!AF_read_string(a->f, buf, a->version >= 45 /* 3.4.0 */ ? sizeof buf : 20+1 /* MAX_SCRIPT_NAME_LEN */))
				ERR(AOE_inventories);
			if(!is_zeroterminated(buf, sizeof buf)) ERR(AOE_inventories);
			a->inventorynames[i] = strdup(buf);
		}

		if(a->version >= 32) { // 2.7.2
			/* here follow dialog script names */
		}
	}

	/* prevent usage of uninitialized data */
	if(!a->inventorynames) a->game.inventorycount = 0;
	if(!a->viewnames) a->game.viewcount = 0;

	/* at this point we have everything we need */
	return AOE_success;
#undef FAIL
#undef ERR
}


