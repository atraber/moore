/* Copyright (c) 2016 Fabian Schuiki */
#include "util.h"

void
buffer_init(buffer_t *buf, size_t cap) {
	assert(buf);
	memset(buf, 0, sizeof(*buf));
	if (cap < 16) {
		cap = 16;
	}
	buf->cap = cap;
	buf->data = malloc(cap);
}

void
buffer_dispose(buffer_t *buf) {
	assert(buf);
	if (buf->data) {
		free(buf->data);
	}
	memset(buf, 0, sizeof(*buf));
}

void *
buffer_append(buffer_t *buf, size_t size, void *data) {
	assert(buf);
	void *ptr = buf->data + buf->size;
	size_t req = buf->size + size;

	if (req > buf->cap) {
		buf->cap *= 2;
		if (buf->cap < req)
			buf->cap = req;
		buf->data = realloc(buf->data, buf->cap);
	}

	buf->size += size;
	if (data) {
		memcpy(ptr, data, size);
	}

	return ptr;
}

void
buffer_clear(buffer_t *buf) {
	assert(buf);
	buf->size = 0;
}
