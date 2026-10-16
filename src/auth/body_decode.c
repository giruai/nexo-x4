/* SPDX-License-Identifier: GPL-3.0-only */
#include "body_decode.h"
#include <limits.h>
#include <string.h>

static int append_identity(X4BodyDecoder *d, const unsigned char *bytes, size_t size)
{
    if (size > d->capacity - 1 - d->used) return X4_BODY_OVERFLOW;
    memcpy(d->output + d->used, bytes, size);
    d->used += size;
    return X4_BODY_OK;
}

static int append_gzip(X4BodyDecoder *d, const unsigned char *bytes, size_t size)
{
    if (d->complete && size) return X4_BODY_INVALID;
    if (size > UINT_MAX) return X4_BODY_INVALID;
    d->stream.next_in = (Bytef *)(void *)bytes;
    d->stream.avail_in = (uInt)size;
    while (d->stream.avail_in) {
        unsigned char overflow_probe = 0;
        size_t room = d->capacity - 1 - d->used;
        d->stream.next_out = room ? (Bytef *)(d->output + d->used) : &overflow_probe;
        d->stream.avail_out = room ? (uInt)room : 1u;
        uInt before_in = d->stream.avail_in;
        uInt before_out = d->stream.avail_out;
        int rc = inflate(&d->stream, Z_NO_FLUSH);
        size_t produced = (size_t)(before_out - d->stream.avail_out);
        if (!room && produced) return X4_BODY_OVERFLOW;
        d->used += produced;
        if (rc == Z_STREAM_END) {
            d->complete = true;
            return d->stream.avail_in ? X4_BODY_INVALID : X4_BODY_OK;
        }
        if (rc == Z_MEM_ERROR) return X4_BODY_MEMORY;
        if (rc != Z_OK && rc != Z_BUF_ERROR) return X4_BODY_INVALID;
        if (d->stream.avail_in == before_in && !produced) return X4_BODY_INVALID;
    }
    return X4_BODY_OK;
}

int x4_body_decoder_init(X4BodyDecoder *d, char *output, size_t capacity)
{
    if (!d || !output || !capacity || capacity - 1 > UINT_MAX) return X4_BODY_INVALID;
    memset(d, 0, sizeof(*d));
    d->output = output;
    d->capacity = capacity;
    output[0] = 0;
    return X4_BODY_OK;
}

int x4_body_decoder_consume(X4BodyDecoder *d, const void *bytes, size_t size)
{
    if (!d || !d->output || (size && !bytes)) return X4_BODY_INVALID;
    const unsigned char *p = bytes;
    if (!size) return X4_BODY_OK;
    if (d->mode == 1) return append_identity(d, p, size);
    if (d->mode == 2) return append_gzip(d, p, size);
    while (d->prefix_used < 2 && size) {
        d->prefix[d->prefix_used++] = *p++;
        --size;
    }
    if (d->prefix_used < 2) return X4_BODY_OK;
    if (d->prefix[0] != 0x1f || d->prefix[1] != 0x8b) {
        d->mode = 1;
        int rc = append_identity(d, d->prefix, 2);
        return rc ? rc : append_identity(d, p, size);
    }
    d->mode = 2;
    int rc = inflateInit2(&d->stream, 16 + MAX_WBITS);
    if (rc != Z_OK) return rc == Z_MEM_ERROR ? X4_BODY_MEMORY : X4_BODY_INVALID;
    d->initialized = true;
    rc = append_gzip(d, d->prefix, 2);
    return rc ? rc : append_gzip(d, p, size);
}

int x4_body_decoder_finish(X4BodyDecoder *d, size_t *decoded_size)
{
    if (!d || !d->output || !decoded_size) return X4_BODY_INVALID;
    if (d->mode == 0) {
        int rc = append_identity(d, d->prefix, d->prefix_used);
        if (rc) return rc;
    } else if (d->mode == 2 && !d->complete) return X4_BODY_INVALID;
    d->output[d->used] = 0;
    *decoded_size = d->used;
    return X4_BODY_OK;
}

void x4_body_decoder_close(X4BodyDecoder *d)
{
    if (d && d->initialized) {
        inflateEnd(&d->stream);
        d->initialized = false;
    }
}
