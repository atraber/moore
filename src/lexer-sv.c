/* Copyright (c) 2016 Fabian Schuiki */

/**
 * @file
 *
 * A lexer for SystemVerilog source files.
 *
 * @author Fabian Schuiki <fschuiki@ethz.ch>
 */

#include "util.h"
#include "lexer.h"


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
	unichar_t c2 = source_peek(lex->src, 2);

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

	printf("read '%c' (0x%02x)\n", c0, c0);
	assert(false && "invalid character");
}
