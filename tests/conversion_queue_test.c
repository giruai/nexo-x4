/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include "../src/video/conversion_queue.h"
int main(void)
{
    X4ConversionQueue q = {0};
    assert(x4_conversion_take(&q) == -1);
    int a = x4_conversion_reserve(&q); assert(a >= 0 && !q.replaced);
    assert(x4_conversion_take(&q) == -1); /* Copy in progress is invisible. */
    assert(x4_conversion_commit(&q, a) == -1);
    int b = x4_conversion_reserve(&q); assert(b == a && q.replaced);
    assert(x4_conversion_take(&q) == -1); /* Superseded READY exclusively copied. */
    x4_conversion_commit(&q, b); assert(x4_conversion_take(&q) == b);
    a = x4_conversion_reserve(&q); assert(a != b && !q.replaced);
    x4_conversion_release(&q, a); assert(q.state[a] == X4_CONVERSION_COPYING);
    x4_conversion_commit(&q, a);
    for (unsigned i = 0; i < 100000; ++i) {
        int latest = x4_conversion_reserve(&q); assert(latest == a && q.replaced);
        assert(q.state[b] == X4_CONVERSION_ACTIVE);
        x4_conversion_commit(&q, latest);
    }
    x4_conversion_release(&q, b); assert(q.state[b] == X4_CONVERSION_FREE);
    assert(x4_conversion_take(&q) == a); x4_conversion_release(&q, a);
    for (int i = 0; i < X4_CONVERSION_SLOTS; ++i) assert(q.state[i] == X4_CONVERSION_FREE);
    puts("conversion queue ownership passed");
}
