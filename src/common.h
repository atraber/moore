/* Copyright (c) 2016 Fabian Schuiki */
#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

typedef struct source source_t;
typedef enum source_enc source_enc_t;
typedef int32_t unichar_t;
typedef uint8_t utf8_t;
typedef struct vhdl_lexer vhdl_lexer_t;
typedef enum vhdl_token vhdl_token_t;
typedef struct buffer buffer_t;
typedef struct sv_lexer sv_lexer_t;
typedef enum sv_token sv_token_t;
