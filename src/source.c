/* Copyright (c) 2016 Fabian Schuiki */
#include "util.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @file
 *
 * This file implements a source file abstraction that allows the compiler to
 * process text coming from a file, memory location, or any other source; with
 * the text encoded in UTF-8, ISO 8859-1, or any other encoding. The abstraction
 * provided in this file allows other code to assume all input files reside in
 * memory and use Unicode characters.
 *
 * Must be able to:
 * - read file as sequence of unicode code points (e.g. for lexing)
 * - extract portions of a file as UTF-8 string into memory (e.g. for error
 *   reporting)
 *
 * @author Fabian Schuiki <fschuiki@ethz.ch>
 */

enum source_mode {
	MODE_FILE,
	MODE_POINTER
};

struct source {
	int refcount;
	source_enc_t enc;
	enum source_mode mode;
	char *name;
	FILE *file;
	void *ptr;
	bool own_ptr;
	size_t size;
	size_t pos;
};

static void source_free(source_t *src) {
	assert(src);
	assert(src->refcount == 0);
	free(src->name);
	switch (src->mode) {
	case MODE_FILE:
		if (munmap(src->ptr, src->size) == -1) {
			perror("munmap");
		}
		break;
	case MODE_POINTER:
		if (src->own_ptr)
			free(src->ptr);
		break;
	}
	free(src);
}

void source_ref(source_t *src) {
	assert(src);
	++src->refcount;
}

void source_unref(source_t *src) {
	assert(src);
	assert(src->refcount > 0);
	if (--src->refcount == 0)
		source_free(src);
}

source_t *
source_new_from_file(const char *filename, source_enc_t enc) {
	assert(filename);

	/// @todo Improve error handling. Or rather: Actually implement proper error
	/// handling.

	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		perror("open");
		return NULL;
	}

	struct stat fs;
	if (fstat(fd, &fs) == -1) {
		perror("fstat");
		close(fd);
		return NULL;
	}

	if (!S_ISREG(fs.st_mode)) {
		fprintf(stderr, "not a regular file: %s\n", filename);
		close(fd);
		return NULL;
	}

	void *ptr = mmap(0, fs.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		close(fd);
		return NULL;
	}

	if (close(fd) == -1) {
		perror("close");
		return NULL;
	}

	source_t *src = calloc(1, sizeof(*src));
	src->refcount = 1;
	src->enc = enc;
	src->mode = MODE_FILE;
	src->ptr = ptr;
	src->size = fs.st_size;

	return src;
}

source_t *
source_new_from_pointer(void *ptr, size_t size, source_enc_t enc, bool own) {
	assert(ptr);

	source_t *src = calloc(1, sizeof(*src));
	src->refcount = 1;
	src->enc = enc;
	src->mode = MODE_POINTER;
	src->ptr = ptr;
	src->own_ptr = own;
	src->size = size;

	return src;
}

int
source_next(source_t *src) {
	assert(src);
	if (src->pos < src->size) {
		return *(utf8_t*)(src->ptr + src->pos++);
	} else {
		return -1;
	}
}

int
source_peek(source_t *src, int adv) {
	assert(src);
	assert(adv >= 0);
	if (src->pos + adv >= src->size)
		return -1;
	return *(utf8_t*)(src->ptr + src->pos + adv);
}

size_t
source_pos(source_t *src) {
	assert(src);
	return src->pos;
}

bool
source_eof(source_t *src) {
	assert(src);
	return src->pos >= src->size;
}
