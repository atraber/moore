/* Copyright (c) 2016 Fabian Schuiki */
#include "util.h"
#include "lexer.h"

struct vhdl_lexer {
	source_t *src;
	vhdl_token_t tkn;
	size_t base, end;
	buffer_t buf;
};

vhdl_lexer_t *
vhdl_lexer_new(source_t *src) {
	assert(src);
	source_ref(src);
	vhdl_lexer_t *lex = calloc(1, sizeof(*lex));
	buffer_init(&lex->buf, 256);
	lex->src = src;
	vhdl_lexer_next(lex);
	return lex;
}

void
vhdl_lexer_free(vhdl_lexer_t *lex) {
	assert(lex);
	buffer_dispose(&lex->buf);
	source_unref(lex->src);
	free(lex);
}

vhdl_token_t
vhdl_lexer_token(vhdl_lexer_t *lex) {
	assert(lex);
	return lex->tkn;
}

static void
start(vhdl_lexer_t *lex) {
	lex->base = lex->end;
	buffer_clear(&lex->buf);
}

static void
next(vhdl_lexer_t *lex) {
	*(utf8_t*)buffer_append(&lex->buf, 1, NULL) = source_next(lex->src);
}

static void
finish(vhdl_lexer_t *lex) {
	lex->end = source_pos(lex->src);
	*(utf8_t*)buffer_append(&lex->buf, 1, NULL) = 0;
}

static bool
is_letter(utf8_t chr0, utf8_t chr1) {

	/// @todo This check is rather pedantic. Based on unicode, add an option to
	/// allow for any unicode codepoint categorized as "letter" to pass this
	/// test. Make this permissive mode the default, and add a "pedantic" option
	/// to the command line.

	unichar_t c;
	if ((chr0 & 0x80) == 0) {
		c = chr0 & 0x7F;
	} else if ((chr0 & 0xE0) == 0xC0) {
		c = (chr0 & 0x1F) << 6 | (chr1 & 0x3F);
	} else {
		return false;
	}
	return (c >= 0x41 && c <= 0x5A) ||
	       (c >= 0x61 && c <= 0x7A) ||
	       (c >= 0xC0 && c <= 0xFF && c != 0xD7 && c != 0xF7);
}

static bool
is_digit(utf8_t c) {
	return (c >= 0x30 && c <= 0x39);
}

static void
skip_whitespace(vhdl_lexer_t *lex) {
	for (;;) {
		int c = source_peek(lex->src, 0);
		if (c == ' ' || c == '\n' || c == '\t' || c == '\r') {
			source_next(lex->src);
			continue;
		}
		else if (c == 0xC2 && source_peek(lex->src, 1) == 0xA0) {
			source_next(lex->src);
			source_next(lex->src);
			continue;
		}
		break;
	};
}

static void
lex_comment(vhdl_lexer_t *lex) {
	int c;
	while ((c = source_peek(lex->src, 0)) >= 0 && c != '\n') {
		next(lex);
	}
}

static void
lex_ident_basic(vhdl_lexer_t *lex) {
	int chr0;
	while ((chr0 = source_peek(lex->src, 0)) >= 0) {
		int chr1 = source_peek(lex->src, 1);
		if ((chr0 & 0xC0) != 0x80 && !is_letter(chr0, chr1) && !is_digit(chr0) && chr0 != '_')
			break;
		next(lex);
	}
}

static void
lex_ident_extended(vhdl_lexer_t *lex) {
	int c;
	while ((c = source_peek(lex->src, 0)) >= 0) {
		if (c == '\\' && source_peek(lex->src, 1) != '\\')
			break;
		next(lex);
	}
}

void
vhdl_lexer_next(vhdl_lexer_t *lex) {
	assert(lex);

	// No need to continue lexing if we've reached the end of the source file.
	if (source_eof(lex->src)) {
		lex->tkn = VHDL_EOF;
		return;
	}

	/// @todo Insert lexer magic here.

	skip_whitespace(lex);

	start(lex);
	int chr0 = source_peek(lex->src, 0);
	int chr1 = source_peek(lex->src, 1);

	/* IEEE 1076-2000 13.8 */
	if (chr0 == '-' && chr1 == '-') {
		lex->tkn = VHDL_COMMENT;
		lex_comment(lex);
		finish(lex);
		printf("  comment '%s'\n", (char*)lex->buf.data);
		return;
	}

	/* IEEE 1076-2000 13.2 */
	vhdl_token_t special = 0;
	switch (chr0) {
		case '&':  special = VHDL_AMPERSAND; break;
		case '\'': special = VHDL_APOSTROPHE; break;
		case '(':  special = VHDL_LPAREN; break;
		case ')':  special = VHDL_RPAREN; break;
		case '+':  special = VHDL_PLUS; break;
		case ',':  special = VHDL_COMMA; break;
		case '-':  special = VHDL_MINUS; break;
		case '.':  special = VHDL_PERIOD; break;
		case ';':  special = VHDL_SEMICOLON; break;
		case '|':  special = VHDL_PIPE; break;
		case '[':  special = VHDL_LBRACK; break;
		case ']':  special = VHDL_RBRACK; break;
		case '=':
			if (chr1 == '>') {
				special = VHDL_ARROW;
				next(lex);
			} else {
				special = VHDL_EQUAL;
			}
			break;
		case '*':
			if (chr1 == '*') {
				special = VHDL_DOUBLESTAR;
				next(lex);
			} else {
				special = VHDL_ASTERISK;
			}
			break;
		case ':':
			if (chr1 == '=') {
				special = VHDL_VARASSIGN;
				next(lex);
			} else {
				special = VHDL_COLON;
			}
			break;
		case '/':
			if (chr1 == '=') {
				special = VHDL_NOTEQUAL;
				next(lex);
			} else {
				special = VHDL_SOLIDUS;
			}
			break;
		case '>':
			if (chr1 == '=') {
				special = VHDL_GREATEREQUAL;
				next(lex);
			} else {
				special = VHDL_GREATER;
			}
			break;
		case '<':
			if (chr1 == '=') {
				special = VHDL_LESSEQUAL;
				next(lex);
			} if (chr1 == '>') {
				special = VHDL_BOX;
				next(lex);
			} else {
				special = VHDL_LESS;
			}
			break;
	}
	if (special != 0) {
		lex->tkn = special;
		next(lex);
		finish(lex);
		printf("  symbol '%s'\n", (char*)lex->buf.data);
		return;
	}

	/* IEEE 1076-2000 13.3.1 */
	if (is_letter(chr0, chr1)) {
		lex->tkn = VHDL_IDENT_BASIC;
		lex_ident_basic(lex);
		finish(lex);
		printf("  identifier (basic) '%s'\n", (char*)lex->buf.data);
		return;
	}

	/* IEEE 1076-2000 13.3.2 */
	if (chr0 == '\\') {
		lex->tkn = VHDL_IDENT_EXTENDED;
		lex_ident_extended(lex);
		finish(lex);
		printf("  identifier (extended) '%s'\n", (char*)lex->buf.data);
		return;
	}

	printf("  read (0x%02x) '%c'\n", chr0, chr0);
	fprintf(stderr, "invalid character\n");
	lex->tkn = VHDL_EOF;
}
