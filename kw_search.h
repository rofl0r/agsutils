#include "ags_cpu.h"
#include <string.h>

#ifndef KW_TOK_SCMD_BASE
#define KW_TOK_SCMD_BASE 0
#endif

#define __KW_SCMD_DEBUG__
#include "scmd_tok.h"
#include "scmd_tok.c"

#ifndef RN_TOK_BASE
#define RN_TOK_BASE 0
#endif
#include "regname_tok.h"
#include "regname_tok.c"

static void kw_init(void) {
	KW_SCMD_reset();
	RN_reset();
}

static void kw_finish(void) {
	KW_SCMD_output_statistics();
	RN_output_statistics();
}

static unsigned kw_find_insn(char* sym, size_t l) {
	return KW_SCMD_find_keyword(sym, l);
}

static unsigned kw_find_reg(char* sym, size_t l) {
	return RN_find_keyword(sym, l);
}



