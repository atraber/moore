/* Copyright (c) 2016 Fabian Schuiki */

/**
 * @file
 *
 * A lexer for SystemVerilog source files based on section 5 of IEEE 1800-2009.
 *
 * @author Fabian Schuiki <fschuiki@ethz.ch>
 */

#include "util.h"
#include "lexer.h"
#include <pthread.h>


struct symbol {
	const char *seq;
	enum sv_token tkn;
};

static struct symbol symbols[] = {
	{"(",    SV_LPAREN},
	{")",    SV_RPAREN},
	{"[",    SV_LBRACK},
	{"]",    SV_RBRACK},
	{"{",    SV_LBRACE},
	{"}",    SV_RBRACE},
	{"'",    SV_APOSTROPHE},
	{"=",    SV_AS},
	{"+=",   SV_ASADD},
	{"-=",   SV_ASSUB},
	{"*=",   SV_ASMUL},
	{"/=",   SV_ASDIV},
	{"%=",   SV_ASMOD},
	{"&=",   SV_ASAND},
	{"|=",   SV_ASOR},
	{"^=",   SV_ASXOR},
	{"<<=",  SV_ASLSL},
	{">>=",  SV_ASLSR},
	{"<<<=", SV_ASASL},
	{">>>=", SV_ASASR},
	{"?",    SV_QUEST},
	{":",    SV_COLON},
	{";",    SV_SEMICOLON},
	{".",    SV_PERIOD},
	{",",    SV_COMMA},
	{"+",    SV_ADD},
	{"-",    SV_SUB},
	{"*",    SV_MUL},
	{"/",    SV_DIV},
	{"**",   SV_POW},
	{"%",    SV_MOD},
	{"!",    SV_EXCL},
	{"~",    SV_TILDA},
	{"&",    SV_AND},
	{"~&",   SV_NAND},
	{"|",    SV_OR},
	{"~|",   SV_NOR},
	{"^",    SV_XOR},
	{"~^",   SV_NXOR},
	{"^~",   SV_XNOR},
	{"==",   SV_LEQ},
	{"!=",   SV_LNEQ},
	{"===",  SV_CEQ},
	{"!==",  SV_CNEQ},
	{"==?",  SV_WCEQ},
	{"!=?",  SV_WCNEQ},
	{"&&",   SV_LAND},
	{"||",   SV_LOR},
	{"->",   SV_IMPL},
	{"<->",  SV_EQUIV},
	{"<",    SV_LT},
	{"<=",   SV_LE},
	{">",    SV_GT},
	{">=",   SV_GE},
	{">>",   SV_LSL},
	{"<<",   SV_LSR},
	{">>>",  SV_ASL},
	{"<<<",  SV_ASR},
	{"++",   SV_INC},
	{"--",   SV_DEC},
};
static const unsigned num_symbols = sizeof(symbols) / sizeof(*symbols);

static int
compare_symbols(const void *pa, const void *pb) {
	return strcmp(
		((const struct symbol*)pa)->seq,
		((const struct symbol*)pb)->seq
	);
}

static int
compare_key_and_symbol(const void *pstr, const void *psym) {
	const unichar_t *str = pstr;
	const char *sym = ((const struct symbol*)psym)->seq;
	while (*sym) {
		if (*str < *sym) return -1;
		if (*str > *sym) return  1;
		++str;
		++sym;
	}
	return 0;
}

static pthread_once_t init_symbols_once = PTHREAD_ONCE_INIT;
static void
init_symbols() {
	qsort(symbols, num_symbols, sizeof(*symbols), compare_symbols);
}

static void
find_symbol_bounds(const unichar_t *key, unsigned pos, unsigned base, unsigned num, unsigned *lower, unsigned *upper) {
	assert(key && lower && upper);

	// Find a location in the symbol array where the first character of the
	// symbol agrees with the first character of the key. This is the initial
	// guess.
	unsigned start = base, end = base+num;
	while (start < end) {
		unsigned mid = start + (end - start) / 2;
		if (key[pos] < symbols[mid].seq[pos]) {
			end = mid;
		} else if (key[pos] > symbols[mid].seq[pos]) {
			start = mid + 1;
		} else {
			start = mid;
			end = mid;
		}
	}

	// Find the upper and lower bound of symbols that have the same first
	// character as the key.
	while (start > base && key[pos] == symbols[start-1].seq[pos]) --start;
	while (end < base+num && key[pos] == symbols[end].seq[pos]) ++end;

	*lower = start;
	*upper = end;
}

static struct symbol *
find_symbol_recursive(const unichar_t *key, unsigned pos, unsigned base, unsigned num) {
	unsigned start, end;
	/// @todo Properly document how this works.

	// Find the range of symbols whose first character agrees with the first
	// character of the key.
	find_symbol_bounds(key, pos, base, num, &start, &end);

	unsigned num_found = end-start;
	if (num_found == 1) {
		return symbols+start;
	} else if (num_found > 1 && pos < 4) {
		struct symbol *proposed = find_symbol_recursive(key, pos+1, start, num_found);
		if (proposed) {
			return proposed;
		} else if (symbols[start].seq[pos+1] == 0) {
			return symbols+start;
		} else {
			return NULL;
		}
	} else {
		return NULL;
	}
}

static struct symbol *
find_symbol(const unichar_t *key) {
	return find_symbol_recursive(key, 0, 0, num_symbols);
}


struct sv_lexer {
	source_t *src;
	sv_token_t tkn;
	size_t base, end;
	buffer_t buf;
};


sv_lexer_t *
sv_lexer_new(source_t *src) {
	assert(src);
	source_ref(src);
	sv_lexer_t *lex = calloc(1, sizeof(*lex));
	buffer_init(&lex->buf, 1024);
	lex->src = src;
	sv_lexer_next(lex);
	pthread_once(&init_symbols_once, init_symbols); // init symbol table once
	return lex;
}

void
sv_lexer_free(sv_lexer_t *lex) {
	assert(lex);
	buffer_dispose(&lex->buf);
	source_unref(lex->src);
	free(lex);
}

sv_token_t
sv_lexer_token(sv_lexer_t *lex) {
	assert(lex);
	return lex->tkn;
}

const utf8_t *
sv_lexer_text(sv_lexer_t *lex) {
	assert(lex);
	return lex->tkn == SV_EOF ? NULL : lex->buf.data;
}

static void
start(sv_lexer_t *lex) {
	lex->base = lex->end;
	buffer_clear(&lex->buf);
}

static void
next(sv_lexer_t *lex) {
	unichar_t c = source_next(lex->src);
	*(utf8_t*)buffer_append(&lex->buf, 1, NULL) = c;
	/// @todo Fix this to actually convert the unichar to a utf8 byte sequence.
}

static void
finish(sv_lexer_t *lex) {
	lex->end = source_pos(lex->src);
	*(utf8_t*)buffer_append(&lex->buf, 1, NULL) = 0;
}

static bool
is_whitespace(unichar_t c) {
	return c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == 0xA0;
}

static void
lex_line_comment(sv_lexer_t *lex) {
	unichar_t c;
	while ((c = source_peek(lex->src, 0)) >= 0 && c != '\n') {
		next(lex);
	}
}

static void
lex_block_comment(sv_lexer_t *lex) {
	unichar_t c0 = -1, c1;
	while ((c1 = source_peek(lex->src, 0)) >= 0 && (c0 != '*' || c1 != '/')) {
		next(lex);
	}
	/// @todo Emit warning upon nested block comment.
}

static bool
is_identifier(unichar_t c) {
	return (c >= 'A' && c <= 'Z') ||
	       (c >= 'a' && c <= 'z') ||
	       (c >= '0' && c <= '9') ||
	        c == '_' || c == '$';
}

static void
lex_identifier(sv_lexer_t *lex) {
	unichar_t c;
	while ((c = source_peek(lex->src, 0)) >= 0 && is_identifier(c)) {
		next(lex);
	}
	/// @todo Emit error if identifier starts with digit or dollar sign.
}

static void
lex_string_literal(sv_lexer_t *lex) {
	unichar_t c;
	while ((c = source_peek(lex->src, 0)) >= 0) {
		if (c == '\\') {
			source_next(lex->src);
			c = source_peek(lex->src, 0);
			switch (c) {
				case '\n':
					break;
				case 'n':
					buffer_append(&lex->buf, 1, (utf8_t[]){'\n'});
					break;
				case 't':
					buffer_append(&lex->buf, 1, (utf8_t[]){'\t'});
					break;
				case -1:
					goto fail_eof;
				default:
					buffer_append(&lex->buf, 1, (utf8_t[]){c});
					break;
				/// @todo Complain about unknown escape sequences.
				/// @todo Implement \v, \f, \a, \ddd \xdd
			}
			source_next(lex->src);
		} else if (c == '"') {
			return;
		} else {
			next(lex);
		}
	}

fail_eof:
	assert(false && "end of file in the middle of string literal");
}

void
sv_lexer_next(sv_lexer_t *lex) {
	assert(lex);

	// No need to continue lexing if we've reached the end of the source file.
	if (source_eof(lex->src)) {
		lex->tkn = SV_EOF;
		return;
	}

	/* IEEE 1800-2009 5.3 */
	while (is_whitespace(source_peek(lex->src, 0))) {
		source_next(lex->src);
	}

	start(lex);
	unichar_t c0 = source_peek(lex->src, 0);
	unichar_t c1 = source_peek(lex->src, 1);

	if (c0 == -1) {
		lex->tkn = SV_EOF;
		return;
	}

	/* IEEE 1800-2009 5.4 Comments */
	if (c0 == '/' && c1 == '/') {
		lex->tkn = SV_COMMENT;
		next(lex);
		next(lex);
		lex_line_comment(lex);
		finish(lex);
		return;
	}
	if (c0 == '/' && c1 == '*') {
		lex->tkn = SV_COMMENT;
		next(lex);
		next(lex);
		lex_block_comment(lex);
		finish(lex);
		return;
	}

	/* IEEE 1800-2009 5.6.4 Compiler directives */
	if (c0 == 0x60) {
		lex->tkn = SV_COMP_DIR;
		next(lex);
		lex_identifier(lex);
		finish(lex);
		return;
	}

	/* IEEE 1800-2009 5.9 String literals */
	if (c0 == '"') {
		lex->tkn = SV_LIT_STRING;
		source_next(lex->src);
		lex_string_literal(lex);
		source_next(lex->src);
		finish(lex);
		return;
	}

	/* IEEE 1800-2009 5.6 Identifiers, keywords, and system names */
	if (is_identifier(c0)) {
		lex->tkn = SV_IDENT;
		lex_identifier(lex);
		finish(lex);

		/* IEEE 1800-2009 5.6.2 Keywords */
		/// @todo Check whether the identifier represents a keyword.

		return;
	}

	/* IEEE 1800-2009 5.6.1 Escaped identifiers */
	if (c0 == '\\') {
		lex->tkn = SV_ESC_IDENT;
		source_next(lex->src);
		while ((c0 = source_peek(lex->src, 0)) >= 0 && c0 >= 0x21 && c0 <= 0x7E)
			next(lex);
		finish(lex);
		return;
	}

	/* IEEE 1800-2009 5.6.3 System tasks and system functions */
	if (c0 == '$') {
		lex->tkn = SV_SYSNAME;
		lex_identifier(lex);
		finish(lex);
		return;
	}

	/* IEEE 1800-2009 5.5 Operators & 11.3 Operators */
	unichar_t c2 = source_peek(lex->src, 2);
	unichar_t c3 = source_peek(lex->src, 3);
	struct symbol *sym = find_symbol((unichar_t[]){c0,c1,c2,c3});
	if (sym) {
		lex->tkn = sym->tkn;
		const char *seq = sym->seq;
		while (*seq++)
			next(lex);
		finish(lex);
		return;
	}

	printf("read '%c' (0x%02x)\n", c0, c0);
	assert(false && "invalid character");
}
