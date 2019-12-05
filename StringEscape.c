#include <stddef.h>
#include <assert.h>
#include <stdlib.h>

//FIXME out gets silently truncated if outsize is too small

size_t escape(char* in, char* out, size_t outsize) {
	size_t l = 0;
	while(*in && l + 3 < outsize) {
		switch(*in) {
		case '\a': /* 0x07 */
		case '\b': /* 0x08 */
		case '\t': /* 0x09 */
		case '\n': /* 0x0a */
		case '\v': /* 0x0b */
		case '\f': /* 0x0c */
		case '\r': /* 0x0d */
		case '\"': /* 0x22 */
		case '\'': /* 0x27 */
		case '\?': /* 0x3f */
		case '\\': /* 0x5c */
			*out++ = '\\';
			l++;
			switch(*in) {
			case '\a': /* 0x07 */
				*out = 'a'; break;
			case '\b': /* 0x08 */
				*out = 'b'; break;
			case '\t': /* 0x09 */
				*out = 't'; break;
			case '\n': /* 0x0a */
				*out = 'n'; break;
			case '\v': /* 0x0b */
				*out = 'v'; break;
			case '\f': /* 0x0c */
				*out = 'f'; break;
			case '\r': /* 0x0d */
				*out = 'r'; break;
			case '\"': /* 0x22 */
				*out = '\"'; break;
			case '\'': /* 0x27 */
				*out = '\''; break;
			case '\?': /* 0x3f */
				*out = '\?'; break;
			case '\\': /* 0x5c */
				*out = '\\'; break;
			}
			break;
		default:
			*out = *in;
		}
		in++;
		out++;
		l++;
	}
	*out = 0;
	return l;
}

size_t unescape(char* in, char *out, size_t outsize) {
	size_t l = 0;
	while(*in && l + 1 < outsize) {
		switch (*in) {
			case '\\':
				++in;
				assert(*in);
				switch(*in) {
					case 'a':
						*out = '\a';
						break;
					case 'b':
						*out = '\b';
						break;
					case 't':
						*out='\t';
						break;
					case 'n':
						*out='\n';
						break;
					case 'v':
						*out='\v';
						break;
					case 'f':
						*out = '\f';
						break;
					case 'r':
						*out='\r';
						break;
					case '"':
						*out='"';
						break;
					case '\'':
						*out = '\'';
						break;
					case '?':
						*out = '\?';
						break;
					case '\\':
						*out='\\';
						break;
					// FIXME add handling of hex and octal
					default:
						abort();
				}
				break;
			default:
				*out=*in;
		}
		in++;
		out++;
		l++;
	}
	*out = 0;
	return l;
}
