/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef X4_LIVE_SCALE_H
#define X4_LIVE_SCALE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <emmintrin.h>

/* Exact nearest-neighbor 1280x720 -> 1920x1080 scaling. Source and target
 * contain at least their full frame sizes and must not overlap. Both may be
 * unaligned to a SIMD boundary. stream_stores is intended for a WC target;
 * aligned rows use non-temporal stores and are fenced before return. */
static inline void x4_live_scale_720p_to_1080p_sse2(
    uint32_t *target, const uint32_t *source, bool stream_stores)
{
    bool aligned_stream = stream_stores && (((uintptr_t)target & 15u) == 0);
    for (size_t y = 0; y < 1080; ++y) {
        const uint32_t *in = source + ((y * 2) / 3) * 1280;
        uint32_t *out = target + y * 1920;
        /* Eight input pixels become twelve: a,a,b,c,c,d,e,e,f,g,g,h.
         * The final block reads through input pixel 1279 and writes through
         * output pixel 1919. Both row strides preserve 16-byte alignment. */
        for (size_t block = 0; block < 160; ++block) {
            __m128i a = _mm_loadu_si128((const __m128i *)(const void *)(in + block * 8));
            __m128i b = _mm_loadu_si128((const __m128i *)(const void *)(in + block * 8 + 4));
            __m128i first = _mm_shuffle_epi32(a, _MM_SHUFFLE(2, 1, 0, 0));
            __m128i middle = _mm_unpacklo_epi64(
                _mm_srli_si128(a, 8), _mm_shuffle_epi32(b, _MM_SHUFFLE(0, 0, 0, 0)));
            __m128i last = _mm_shuffle_epi32(b, _MM_SHUFFLE(3, 2, 2, 1));
            __m128i *dst = (__m128i *)(void *)(out + block * 12);
            if (aligned_stream) {
                _mm_stream_si128(dst, first);
                _mm_stream_si128(dst + 1, middle);
                _mm_stream_si128(dst + 2, last);
            } else {
                _mm_storeu_si128(dst, first);
                _mm_storeu_si128(dst + 1, middle);
                _mm_storeu_si128(dst + 2, last);
            }
        }
    }
    if (aligned_stream) _mm_sfence();
}

/* Full 1920x1080 RGBA frame copy from normal cached memory to VideoOut.
 * Source and target must contain 1920*1080 pixels and must not overlap.
 * The fixed bound prevents a caller from copying beyond a scanout frame. */
static inline void x4_live_copy_1080p_sse2(uint32_t *target,
    const uint32_t *source, bool stream_stores)
{
    const size_t vectors = (size_t)1920 * 1080 * sizeof(uint32_t) / 16;
    const bool aligned_stream = stream_stores && (((uintptr_t)target & 15u) == 0);
    const uint8_t *in = (const uint8_t *)(const void *)source;
    uint8_t *out = (uint8_t *)(void *)target;
    for (size_t i = 0; i < vectors; ++i) {
        __m128i value = _mm_loadu_si128((const __m128i *)(const void *)(in + i * 16));
        __m128i *dest = (__m128i *)(void *)(out + i * 16);
        if (aligned_stream) _mm_stream_si128(dest, value);
        else _mm_storeu_si128(dest, value);
    }
    if (aligned_stream) _mm_sfence();
}

#endif
