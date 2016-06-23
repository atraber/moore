/* Copyright (c) 2016 Fabian Schuiki */
#include "lexer.h"
#include "util.h"
#include <strings.h>

static const char *
get_suffix(const char *str) {
	void *p = strrchr(str, '.');
	return p && p != str ? p+1 : NULL;
}

int
main(int argc, char **argv) {

	for (int i = 1; i < argc; ++i) {
		char *arg = argv[i];
		source_t *src = source_new_from_file(arg, ENC_UTF8);

		const char *suffix = get_suffix(arg);

		if (strcasecmp(suffix, "vhd") == 0) {
			vhdl_lexer_t *lexer = vhdl_lexer_new(src);
			printf("processing %s\n", arg);
			while (vhdl_lexer_token(lexer) != VHDL_EOF) {
				printf("token %04x\n", vhdl_lexer_token(lexer));
				vhdl_lexer_next(lexer);
			}
			vhdl_lexer_free(lexer);
		}

		else if (strcasecmp(suffix, "sv") == 0) {
			sv_lexer_t *lexer = sv_lexer_new(src);
			while (sv_lexer_token(lexer) != SV_EOF) {
				printf("token %04x \"%s\"\n", sv_lexer_token(lexer), sv_lexer_text(lexer));
				sv_lexer_next(lexer);
			}
			sv_lexer_free(lexer);
		}

		else {
			fprintf(stderr, "unknown file type \"%s\"\n", arg);
			exit(1);
		}

		source_unref(src);
	}

	return 0;
}
