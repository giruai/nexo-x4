/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
/* Pure ownership policy; caller serializes transitions. At most one producer
 * and one converter. There is at most one READY job (latest wins), plus one
 * COPYING and one CONVERTING lease. No transition recycles an owned lease. */
enum { X4_CONVERSION_SLOTS = 3 };
enum { X4_CONVERSION_FREE, X4_CONVERSION_COPYING, X4_CONVERSION_READY, X4_CONVERSION_ACTIVE };
typedef struct { unsigned state[X4_CONVERSION_SLOTS]; int replaced; } X4ConversionQueue;
static inline int x4_conversion_reserve(X4ConversionQueue *q)
{
    q->replaced = 0;
    for (int i = 0; i < X4_CONVERSION_SLOTS; ++i)
        if (q->state[i] == X4_CONVERSION_READY) {
            q->state[i] = X4_CONVERSION_COPYING; q->replaced = 1; return i;
        }
    for (int i = 0; i < X4_CONVERSION_SLOTS; ++i)
        if (q->state[i] == X4_CONVERSION_FREE) { q->state[i] = X4_CONVERSION_COPYING; return i; }
    return -1;
}
static inline int x4_conversion_commit(X4ConversionQueue *q, int slot)
{
    if (slot < 0 || slot >= X4_CONVERSION_SLOTS || q->state[slot] != X4_CONVERSION_COPYING) return -1;
    int discarded = -1;
    for (int i = 0; i < X4_CONVERSION_SLOTS; ++i)
        if (q->state[i] == X4_CONVERSION_READY) { q->state[i] = X4_CONVERSION_FREE; discarded = i; }
    q->state[slot] = X4_CONVERSION_READY;
    return discarded;
}
static inline int x4_conversion_take(X4ConversionQueue *q)
{
    for (int i = 0; i < X4_CONVERSION_SLOTS; ++i)
        if (q->state[i] == X4_CONVERSION_READY) { q->state[i] = X4_CONVERSION_ACTIVE; return i; }
    return -1;
}
static inline void x4_conversion_release(X4ConversionQueue *q, int slot)
{
    if (slot >= 0 && slot < X4_CONVERSION_SLOTS && q->state[slot] == X4_CONVERSION_ACTIVE)
        q->state[slot] = X4_CONVERSION_FREE;
}
