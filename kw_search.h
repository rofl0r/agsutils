#include "ags_cpu.h"
#include <string.h>

#ifdef USE_SHILKA

#ifndef KW_TOK_SCMD_BASE
#define KW_TOK_SCMD_BASE 0
#endif

#define __KW_SCMD_DEBUG__
#include "scmd_tok.h"
#include "scmd_tok.c"

static void kw_init(void) {
	KW_SCMD_reset();
}

static void kw_finish(void) {
	KW_SCMD_output_statistics();
}

static unsigned kw_find_insn(char* sym, size_t l) {
	return KW_SCMD_find_keyword(sym, l);
}

#else

static size_t mnemolen[SCMD_MAX];
static int mnemolen_initdone = 0;

static void init_mnemolen(void) {
	size_t i = 0;
	for(; i< SCMD_MAX; i++)
		mnemolen[i] = strlen(opcodes[i].mnemonic);
	mnemolen_initdone = 1;
}

static void kw_init(void) {
	if(!mnemolen_initdone) init_mnemolen();
}

static void kw_finish(void) {}

static unsigned kw_find_insn(char* sym, size_t l) {
	size_t i = 0;
	for(; i< SCMD_MAX; i++)
		if(l == mnemolen[i] && memcmp(sym, opcodes[i].mnemonic, l) == 0)
			return i;
	return 0;
}

#endif


