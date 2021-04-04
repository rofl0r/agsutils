/*
 *  minishilka, (C) 2021 rofl0r
 *
 *  licensed under the LGPL 2.1+
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define INLINE_MEMCMP \
"static inline size_t shilka_inline_memcmp(const void* restrict pa, const void* restrict pb, size_t n) {\n" \
"\tconst unsigned char *l=pa, *r=pb;\n" \
"\tfor (; n && *l == *r; n--, l++, r++);\n" \
"\treturn n ? *l-*r : 0;" \
"}\n"

#define EXPORT_EXPORT 0
#define EXPORT_STATIC 1
#define EXPORT_INLINE 2
static void print_header(FILE* out, char *pfx, char *return_type, int expflags, int inlinememcmp) {
	static const char *vistab[] = {
		[EXPORT_EXPORT] = "",
		[EXPORT_STATIC] = "static ",
		[EXPORT_INLINE] = "inline ",
		[EXPORT_STATIC|EXPORT_INLINE] = "static inline ",
	};
	fprintf(out,
		"#include <string.h>\n"
		"%s"
		"%svoid %sreset (void) {}\n"
		"%svoid %soutput_statistics (void) {}\n"
		"%s%s  %sfind_keyword (const char *keyword, unsigned length) {\n",
		inlinememcmp ? INLINE_MEMCMP : "",
		vistab[expflags], pfx,
		vistab[expflags], pfx,
		vistab[expflags], return_type, pfx);
}

static int usage() {
	fprintf(stderr, "%s",
		"minishilka [OPTIONS] descriptionfile.shilka\n\n"
		"minishilka (c) rofl0r is a small replacement for shilka which generates code\n"
		"for fast keyword search. minishilka only supports a single usecase\n"
		"of shilka, namely the one i use.\n"
		"it's meant to be used if compiling the real shilka program would be too much\n"
		"of a hassle. minishilka produces somewhat slower, but still quite fast code.\n"
		"output is written to XXX.c, where XXX is the descriptionfile sans extension.\n"
		"\n"
		"a supported input file starts with a declaration section, followed by a line\n"
		"containing '%%' signifying the start of the keywords section, consisting of\n"
		"one or more lines containing a plain keyword followed by whitespace\n"
		"and an action statement written in C surrounded by curly braces, e.g.\n\n"
		"while              {return KEYWORD_TOKEN_WHILE;}\n\n"
		"the last line in the file may be %other {action-if-no-keyword-matches;}\n"
		"if the %other statement is not provided, 0 is returned if none of the\n"
		"keywords match.\n"
		"the declarations section may contain the statement `%type T`, where T is\n"
		"the type to be returned by the KR_find_keyword function.\n"
		"\nOPTIONS\n"
		"-pprefix - use prefix instead of KR_ for the generated function names.\n"
		"-case - use strcasecmp for keyword comparison instead of memcmp.\n"
		"-no-definitions - this shilka mode is default for mini-shilka and ignored.\n"
		"-inline - emit inline functions plus an inlined memcpy if avg token len<=16.\n"
		"\nNOTES\n"
		"minishilka tries to detect whether the action code for the keywords meets\n"
		"certain simplicity criteria, for example if all statements look like\n"
		"{return SOME_VALUE;}, the return value is being put into a table, which improves\n"
		"both codesize and performance. you can suppress that by making a single statement\n"
		"with 2 semicolons, which results in your actions being literally copied into a\n"
		"less efficient huge switch statement. codesize can be further improved by selecting\n"
		"the smallest C type that can hold all return values, for example an unsigned short\n"
		"if all of the returned values are < 65536, using the mentioned %type declaration.\n"
	);
	return 1;
}

struct item {
	char *kw;
	char *action;
	size_t length;
	struct item* next;
};

static int listcmp(const void *pa, const void *pb) {
	const struct item *a = pa, *b = pb;
	return (int)a->length - (int)b->length;
}

/* to piss off newchurch zealots :-) */
#define master main

int master(int argc, char** argv) {
	int i = 0, j, k, o_case = 0, expflags = EXPORT_STATIC;
	char* pfx = "KR_";
	while(++i < argc) {
		if(argv[i][0] != '-') break;
		switch(argv[i][1]) {
		case 'p': pfx=argv[i]+2; break;
		case 'i':
			if(!strcmp(argv[i]+1, "inline")) expflags |= EXPORT_INLINE;
			else return usage();
			break;
		case 's':
			if(!strcmp(argv[i]+1, "strip")) ; /* o_strip = 1; */
			else return usage();
			break;
		case 'c':
			if(!strcmp(argv[i]+1, "case")) o_case = 1;
			else return usage();
			break;
		case 'n':
			if(!strcmp(argv[i]+1, "no-definitions")) ; /* default */
			else return usage();
			break;
		default:
			return usage();
		}
	}
	if(argc == i) return usage();

	char buf[1024], *other = 0, *p, *q;
	FILE *in = fopen(argv[i], "r"), *out;
	if(!in) {
		perror("fopen");
		return 1;
	}
	snprintf(buf, sizeof buf, "%s", argv[i]);
	if((p = strrchr(buf, '.'))) {
		*(++p) = 'c';
		*(++p) = 0;
	} else strcat(buf, ".c");
	out = fopen(buf, "w");
	if(!out) {
		perror("fopen");
		return 1;
	}

	int seen = 0, use_inline_memcmp = 0, simple_value = 1, simple_return_type = 1;
	size_t maxlen = 0, listcount = 0, strlensum = 0;
	char *return_type = "int";
	struct item *list = 0, *last = 0;
	while(fgets(buf, sizeof buf, in)) {
		p = strrchr(buf, '\n');
		if(p) *p = 0;
		p = buf + strlen(buf)-1;
		while(p > buf && isspace(*p)) *(p--) = 0;

		if(buf[0] == 0) continue;

		if(!seen && !memcmp(buf, "%type", 5)) {
			p = buf+5;
			while(isspace(*p)) ++p;
			return_type = strdup(p);
			if(strchr(p, '*') || !memcmp(p, "struct", 6))
				simple_return_type = 0;
			else if(!memcmp(p, "unsigned", 8)  || !memcmp(p, "signed", 6) ||
				!memcmp(p, "enum", 4) || !memcmp(p, "char", 4) ||
				!memcmp(p, "uint", 4) || !memcmp(p, "short", 5) ||
				!memcmp(p, "int", 3) || !memcmp(p, "long", 4))
				simple_return_type = 1;
			else
				simple_return_type = 0;
			continue;
		}
		if(!seen && strcmp(buf, "%%")) {
			fprintf(stderr, "error: mini-shilka file must begin with %%\n");
			return 1;
		}
		if(!seen) {
			seen = 1;
			continue;
		}
		p = buf;
		while(!isspace(*p))++p;
		*p = 0;

		q = p+1;
		while(isspace(*q))++q;
		if(!strcmp(buf, "%other")) {
			other = q;
			continue;
		}
		if(*q != '{') {
			fprintf(stderr, "error: shilka C statements need to be surrounded by {}!\n");
			return 1;
		} else if (simple_return_type && simple_value != 0) {
			p = q+1;
			while(isspace(*p)) ++p;
			if(!memcmp(p, "return", 6)) {
				p += 6;
				while(isspace(*p)) ++p;
				int commas = 0;
				while(*p && !(*p == '}' && p[1] == 0)) {
					if(*p == ';') ++commas;
					else if(!strchr("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-+ \t", *p)) {
						simple_value = 0;
						break;
					}
					++p;
				}
				if(commas > 1) simple_value = 0;
				if(!commas) {
					fprintf(stderr, "error: C action statement lacks ';'\n");
					return 1;
				}
			} else simple_value = 0;
		}
		struct item *it = calloc(1, sizeof(*it));
		it->kw = strdup(buf);
		it->action = strdup(q);
		it->length = strlen(buf);
		it->next = 0;
		if(!list) {
			list=it;
			last=it;
		} else {
			last->next = it;
			last = it;
		}
		if(it->length > maxlen) maxlen = it->length;
		strlensum += it->length + o_case;
		++listcount;
	}

	struct item *it, *flat_list = calloc(listcount, sizeof *list);
	for(i = 0, it = list; it; ++i, it = it->next)
		flat_list[i] = *it;

	qsort(flat_list, listcount, sizeof *list, listcmp);

	use_inline_memcmp = o_case ? 0 : (expflags & EXPORT_INLINE) && strlensum/listcount <= 16;
	print_header(out, pfx, return_type, expflags, use_inline_memcmp);

	/* index into string table */
	char *stridx_type = strlensum > 255 ? "unsigned short" : "unsigned char";
	fprintf(out, "\tstatic const %s sitab[%zu] = {\n\t\t", stridx_type, maxlen+1);
	for(i = j = k = 0; i <= maxlen; ++i) {
		if(j >= listcount || flat_list[j].length > i) fprintf(out, "0, ");
		else {
			fprintf(out, "%d, ", k);
			while(flat_list[j].length == i)
				k += flat_list[j++].length + o_case;
		}
	}
	fprintf(out, "\n\t};\n");

	/* count of items of length */
	fprintf(out, "\tstatic const unsigned char sctab[%zu] = {\n\t\t", maxlen+1);
	for(i = j = 0; i <= maxlen; ++i) {
		if(j >= listcount || flat_list[j].length > i) fprintf(out, "0, ");
		else {
			k = j;
			while(flat_list[++j].length == i);
			fprintf(out, "%d, ", j-k);
		}
	}
	fprintf(out, "\n\t};\n");

	/* array index for return value assignment */
	fprintf(out, "\tstatic const unsigned char itab[%zu] = {\n\t\t", maxlen+1);
	for(i = j = 0; i <= maxlen; ++i) {
		if(flat_list[j].length > i) fprintf(out, "0, ");
		else {
			fprintf(out, "%d, ", j);
			while(flat_list[++j].length == i);
		}
	}
	fprintf(out, "\n\t};\n");

	fprintf(out, "\tstatic const char keywords[] = {\n");
	for(i = 0; i < listcount; ++i) {
		it = &flat_list[i];
		fprintf(out, "\t\t\"%s%s\"\n", it->kw, o_case ? "\\000" : "");
	}
	fprintf(out, "\t};\n");

	if(simple_value) {
		fprintf(out, "\tstatic const %s retval[] = {\n", return_type);
		for(i = 0; i < listcount; ++i) {
			char *p = flat_list[i].action, *q;
			while(*p != 'r') ++p;
			p += 6;
			while(isspace(*p)) ++p;
			q = p+1;
			while(*q != ';') ++q;
			fprintf(out, "\t\t%.*s,\n", (int)(q-p), p);
		}
		fprintf(out, "\t};\n");
	}

	fprintf(out,
		"\tif(length>%d) goto fail;\n"
		"\tint i, count = sctab[length];\n"
		"\tconst char *p = keywords + sitab[length];\n"
		"\tfor(i = 0; i<count; ++i, p+=length+%d)\n"
		"\t\tif(!%s(p, keyword%s)) goto lookup;\n"
		"\tgoto fail;\n",
		(int) maxlen,
		(int) o_case,
		o_case ? "strcasecmp" : use_inline_memcmp ? "shilka_inline_memcmp" : "memcmp", o_case ? "" : ", length"
	);
	if(simple_value) {
		fprintf(out, "\tlookup:; return retval[itab[length]+i];\n\tfail:; %s\n}\n",
			other ? other : "return 0;");
	} else {
		fprintf(out, "\tlookup:; switch(itab[length]+i) {\n");
		for(i = 0; i < listcount ; ++i) {
			it = &flat_list[i];
			fprintf(out, "\tcase %d: %s ; break;\n", i, it->action);
		}
		fprintf(out, "\tdefault: goto fail;\n\t}\n\tfail:; %s\n}\n", other ? other : "return 0;");
	}

	fclose(in);
	fclose(out);
	return 0;
}
