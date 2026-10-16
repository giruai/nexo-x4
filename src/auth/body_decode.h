/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <zlib.h>

enum X4BodyDecodeResult {
    X4_BODY_OK = 0,
    X4_BODY_OVERFLOW = -1,
    X4_BODY_INVALID = -2,
    X4_BODY_MEMORY = -3,
};

/* Decodes one bounded response into caller-owned storage. A gzip magic prefix
 * selects gzip; otherwise the bytes pass through unchanged. This also accepts
 * a native HTTP implementation that has already decompressed the response. */
typedef struct {
    z_stream stream;
    char *output;
    size_t capacity;
    size_t used;
    unsigned char prefix[2];
    unsigned prefix_used;
    unsigned mode; /* 0 undecided, 1 identity, 2 gzip */
    bool initialized;
    bool complete;
} X4BodyDecoder;

int x4_body_decoder_init(X4BodyDecoder *decoder, char *output, size_t capacity);
int x4_body_decoder_consume(X4BodyDecoder *decoder, const void *bytes, size_t size);
int x4_body_decoder_finish(X4BodyDecoder *decoder, size_t *decoded_size);
void x4_body_decoder_close(X4BodyDecoder *decoder);
