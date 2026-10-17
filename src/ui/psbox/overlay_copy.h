/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <emmintrin.h>
/* Opaque, clipped row copies: never read the WC destination. All source
 * pixels must already be opaque. Streaming stores are fenced before return. */
static inline void x4_overlay_copy(uint32_t *dst, int dw, int dh,
    const uint32_t *src, int sw, int sh, int left, int top)
{
    if (!dst || !src || dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
    int64_t lx = left, ty = top;
    if (lx >= dw || ty >= dh || lx + sw <= 0 || ty + sh <= 0) return;
    int x0 = lx < 0 ? (int)-lx : 0, y0 = ty < 0 ? (int)-ty : 0;
    int x1 = (int)((int64_t)sw < (int64_t)dw - lx ? sw : (int64_t)dw - lx);
    int y1 = (int)((int64_t)sh < (int64_t)dh - ty ? sh : (int64_t)dh - ty);
    if (x0 >= x1 || y0 >= y1) return;
    for (int y = y0; y < y1; ++y) {
        uint32_t *out = dst + (size_t)(top + y) * dw + left + x0;
        const uint32_t *in = src + (size_t)y * sw + x0;
        int n = x1 - x0;
        while (n && ((uintptr_t)out & 15)) { *out++ = *in++; --n; }
        while (n >= 4) {
            _mm_stream_si128((__m128i *)out, _mm_loadu_si128((const __m128i *)in));
            out += 4; in += 4; n -= 4;
        }
        while (n--) *out++ = *in++;
    }
    _mm_sfence();
}
