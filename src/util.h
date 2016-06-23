/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include "common.h"

struct buffer {
	void *data;
	size_t size;
	size_t cap;
};

void buffer_init(buffer_t*, size_t);
void buffer_dispose(buffer_t*);
void *buffer_append(buffer_t*, size_t, void*);
void buffer_clear(buffer_t*);

enum source_enc {
	ENC_UTF8,
	ENC_UTF16,
	ENC_UTF32,
	ENC_ISO8859_1,
};

source_t *source_new_from_file(const char*, source_enc_t);
source_t *source_new_from_pointer(void*, size_t, source_enc_t, bool);
void source_ref(source_t*);
void source_unref(source_t*);
unichar_t source_next(source_t*);
unichar_t source_peek(source_t*, int);
size_t source_pos(source_t*);
bool source_eof(source_t*);
void source_extract(source_t*, size_t, size_t, utf8_t**, size_t*);

/* A lexer is required that converts a file into a stream of tokens, followed by
 * a special EOF token. Tokens contain their corresponding section of the source
 * text as a UTF-8 string. */

