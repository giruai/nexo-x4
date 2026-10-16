/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/media/live_scale.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { IN_W = 1280, IN_H = 720, OUT_W = 1920, OUT_H = 1080, GUARD = 32 };

static uint32_t pattern(size_t index)
{
    uint32_t x = (uint32_t)index + 0x9e3779b9u;
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    return x ^ (x >> 16);
}

static int check_case(unsigned source_offset, unsigned target_offset, bool stream_stores)
{
    const size_t source_bytes = (size_t)IN_W * IN_H * sizeof(uint32_t);
    const size_t target_bytes = (size_t)OUT_W * OUT_H * sizeof(uint32_t);
    uint8_t *source_allocation = malloc(source_bytes + GUARD * 2 + 16);
    uint8_t *target_allocation = malloc(target_bytes + GUARD * 2 + 16);
    if (!source_allocation || !target_allocation) {
        free(source_allocation); free(target_allocation);
        return 1;
    }
    uint8_t *source_data = source_allocation + GUARD + source_offset;
    uint8_t *target_data = target_allocation + GUARD + target_offset;
    uint32_t *source = (uint32_t *)(void *)source_data;
    uint32_t *target = (uint32_t *)(void *)target_data;
    memset(source_allocation, 0xa5, source_bytes + GUARD * 2 + 16);
    memset(target_allocation, 0x5a, target_bytes + GUARD * 2 + 16);
    for (size_t i = 0; i < (size_t)IN_W * IN_H; ++i) source[i] = pattern(i);
    x4_live_scale_720p_to_1080p_sse2(target, source, stream_stores);
    for (size_t y = 0; y < OUT_H; ++y) {
        for (size_t x = 0; x < OUT_W; ++x) {
            uint32_t expected = source[(y * 2 / 3) * IN_W + x * 2 / 3];
            if (target[y * OUT_W + x] != expected) {
                fprintf(stderr, "pixel mismatch offsets=%u,%u stream=%u at %zu,%zu\n",
                    source_offset, target_offset, stream_stores, x, y);
                free(source_allocation); free(target_allocation);
                return 1;
            }
        }
    }
    for (size_t i = 0; i < GUARD; ++i) {
        if (source_data[-(ptrdiff_t)GUARD + (ptrdiff_t)i] != 0xa5 ||
            source_data[source_bytes + i] != 0xa5 ||
            target_data[-(ptrdiff_t)GUARD + (ptrdiff_t)i] != 0x5a ||
            target_data[target_bytes + i] != 0x5a) {
            fprintf(stderr, "guard mismatch offsets=%u,%u stream=%u\n",
                source_offset, target_offset, stream_stores);
            free(source_allocation); free(target_allocation);
            return 1;
        }
    }
    for (size_t i = 0; i < (size_t)IN_W * IN_H; ++i) {
        if (source[i] != pattern(i)) {
            fprintf(stderr, "source changed at %zu\n", i);
            free(source_allocation); free(target_allocation);
            return 1;
        }
    }
    free(source_allocation); free(target_allocation);
    return 0;
}

static int check_copy_case(unsigned source_offset, unsigned target_offset, bool stream_stores)
{
    const size_t pixels = (size_t)OUT_W * OUT_H;
    const size_t bytes = pixels * sizeof(uint32_t);
    uint8_t *source_allocation = malloc(bytes + GUARD * 2 + 32);
    uint8_t *target_allocation = malloc(bytes + GUARD * 2 + 32);
    if (!source_allocation || !target_allocation) {
        free(source_allocation); free(target_allocation);
        return 1;
    }
    uintptr_t source_aligned = ((uintptr_t)(source_allocation + GUARD + 15) & ~(uintptr_t)15);
    uintptr_t target_aligned = ((uintptr_t)(target_allocation + GUARD + 15) & ~(uintptr_t)15);
    uint8_t *source_data = (uint8_t *)(source_aligned + source_offset);
    uint8_t *target_data = (uint8_t *)(target_aligned + target_offset);
    uint32_t *source = (uint32_t *)(void *)source_data;
    uint32_t *target = (uint32_t *)(void *)target_data;
    memset(source_allocation, 0xa5, bytes + GUARD * 2 + 32);
    memset(target_allocation, 0x5a, bytes + GUARD * 2 + 32);
    for (size_t i = 0; i < pixels; ++i) source[i] = pattern(i);
    x4_live_copy_1080p_sse2(target, source, stream_stores);
    for (size_t i = 0; i < pixels; ++i) {
        if (target[i] != pattern(i) || source[i] != pattern(i)) {
            fprintf(stderr, "copy mismatch offsets=%u,%u stream=%u at %zu\n",
                source_offset, target_offset, stream_stores, i);
            free(source_allocation); free(target_allocation);
            return 1;
        }
    }
    for (size_t i = 0; i < GUARD; ++i) {
        if (source_data[-(ptrdiff_t)GUARD + (ptrdiff_t)i] != 0xa5 ||
            source_data[bytes + i] != 0xa5 ||
            target_data[-(ptrdiff_t)GUARD + (ptrdiff_t)i] != 0x5a ||
            target_data[bytes + i] != 0x5a) {
            fprintf(stderr, "copy guard mismatch offsets=%u,%u stream=%u\n",
                source_offset, target_offset, stream_stores);
            free(source_allocation); free(target_allocation);
            return 1;
        }
    }
    free(source_allocation); free(target_allocation);
    return 0;
}

int main(void)
{
    /* malloc's 16-byte alignment plus a four-byte offset exercises both
     * aligned streaming and unaligned ordinary stores, and unaligned loads. */
    for (unsigned source_offset = 0; source_offset <= 4; source_offset += 4)
        for (unsigned target_offset = 0; target_offset <= 4; target_offset += 4)
            for (unsigned stream = 0; stream < 2; ++stream)
                if (check_case(source_offset, target_offset, stream != 0)) return 1;
    for (unsigned source_offset = 0; source_offset <= 4; source_offset += 4)
        for (unsigned target_offset = 0; target_offset <= 4; target_offset += 4)
            for (unsigned stream = 0; stream < 2; ++stream)
                if (check_copy_case(source_offset, target_offset, stream != 0)) return 1;
    puts("live_scale_test: 8 scale + 8 copy cases passed");
    return 0;
}
