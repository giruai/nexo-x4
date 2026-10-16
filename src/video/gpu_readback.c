/* SPDX-License-Identifier: GPL-3.0-only */
/* PM4 packet fields follow the public OpenGNM implementation and the
 * hardware-tested Eden composite DMA sample (MIT). This implementation uses
 * only public packet facts, not proprietary SDK source.
 * https://github.com/PS4-OpenGNM/freegnm-examples/blob/ffd055563e5b849ebef1b276ba29e730199670f8/eden-composite/src/main.c
 * https://github.com/PS4-OpenGNM/opengnm/blob/4b295ca54c82c83acf308d1c646a2dfa9ae57350/src/drawcommandbuffer.c
 */
#include "gpu_readback.h"
#include <orbis/GnmDriver.h>
#include <orbis/libkernel.h>
#include <emmintrin.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

enum {
    X4_GPU_MAX_CAPACITY = 64 * 1024 * 1024,
    X4_GPU_ALIGNMENT = 0x4000,
    X4_GPU_COMMAND_BYTES = 0x4000,
    X4_GPU_MAX_CHUNK = 0x1ffffc,
    X4_GPU_WAIT_US = 100000,
    X4_GPU_LOCK_US = 100000,
    X4_GPU_SAFE_REJECT = 1,
    X4_GPU_UNRESOLVED = -1,
    X4_GPU_DRIVER_ERROR = -2,
};

typedef struct {
    void *p;
    off_t offset;
    size_t size;
    bool allocated;
} X4GpuMemory;

struct X4GpuReadback {
    X4GpuMemory destination;
    X4GpuMemory commands;
    volatile uint64_t *label;
    size_t capacity;
    uint64_t sequence;
    bool inflight;
    bool poisoned;
};

static atomic_flag g_gnm_guard = ATOMIC_FLAG_INIT;

int x4_gpu_gnm_enter(void)
{
    uint64_t start = sceKernelGetProcessTime();
    while (atomic_flag_test_and_set_explicit(&g_gnm_guard, memory_order_acquire)) {
        if (sceKernelGetProcessTime() - start >= X4_GPU_LOCK_US) return X4_GPU_SAFE_REJECT;
        sceKernelUsleep(50);
    }
    return 0;
}

void x4_gpu_gnm_leave(void)
{
    atomic_flag_clear_explicit(&g_gnm_guard, memory_order_release);
}

static int allocate_direct(X4GpuMemory *m, size_t size, int type)
{
    m->size = (size + X4_GPU_ALIGNMENT - 1) & ~(size_t)(X4_GPU_ALIGNMENT - 1);
    int rc = sceKernelAllocateDirectMemory(0, sceKernelGetDirectMemorySize(),
        m->size, X4_GPU_ALIGNMENT, type, &m->offset);
    if (rc != 0) return rc;
    m->allocated = true;
    rc = sceKernelMapDirectMemory(&m->p, m->size, 0x33, 0,
        m->offset, X4_GPU_ALIGNMENT);
    if (rc != 0) m->p = NULL;
    return rc;
}

static int release_direct(X4GpuMemory *m)
{
    if (m->p) {
        int rc = sceKernelMunmap(m->p, m->size);
        if (rc != 0) return rc;
        m->p = NULL;
    }
    if (m->allocated) {
        int rc = sceKernelReleaseDirectMemory(m->offset, m->size);
        if (rc != 0) return rc;
        m->allocated = false;
    }
    return 0;
}

int x4_gpu_readback_open(X4GpuReadback **out, size_t capacity)
{
    if (!out || *out || !capacity || capacity > X4_GPU_MAX_CAPACITY) return X4_GPU_SAFE_REJECT;
    X4GpuReadback *g = calloc(1, sizeof(*g));
    if (!g) return X4_GPU_SAFE_REJECT;
    g->capacity = capacity;
    /* WB_ONION = 0, WC_GARLIC = 3 in OpenOrbis direct-memory types. The
     * command buffer and fence label are uncached WC so CPU polling does not
     * retain a stale label. GPU read/write permissions are 0x30. */
    int rc = allocate_direct(&g->destination, capacity, 0);
    if (rc == 0) rc = allocate_direct(&g->commands, X4_GPU_COMMAND_BYTES, 3);
    if (rc != 0) {
        printf("Nexo X4: GPU readback allocation rc=0x%08x\n", (unsigned)rc);
        /* A native release failure keeps the context visible for retry. */
        *out = g;
        int close_rc = x4_gpu_readback_close(out);
        return close_rc == 0 ? X4_GPU_SAFE_REJECT : close_rc;
    }
    g->label = (volatile uint64_t *)((uint8_t *)g->commands.p + X4_GPU_COMMAND_BYTES - 8);
    *g->label = 0;
    _mm_sfence();
    *out = g;
    return 0;
}

static void flush_cpu_lines(const void *base, size_t length)
{
    const uint8_t *p = base;
    for (size_t offset = 0; offset < length; offset += 64) _mm_clflush(p + offset);
    _mm_mfence();
}

static size_t emit_commands(X4GpuReadback *g, uintptr_t dst, uintptr_t src, size_t length)
{
    uint32_t *p = g->commands.p;
    size_t remaining = length;
    while (remaining) {
        uint32_t chunk = remaining > X4_GPU_MAX_CHUNK ? X4_GPU_MAX_CHUNK : (uint32_t)remaining;
        /* DMA_DATA: type-3 packet, six-word body, TC_L2 address mode for
         * source/destination, CP_SYNC. Byte count occupies bits 0..20. */
        *p++ = 0xc0055000u;
        *p++ = 0xe0300000u;
        *p++ = (uint32_t)src;
        *p++ = (uint32_t)(src >> 32);
        *p++ = (uint32_t)dst;
        *p++ = (uint32_t)(dst >> 32);
        *p++ = chunk;
        src += chunk;
        dst += chunk;
        remaining -= chunk;
    }
    uintptr_t label = (uintptr_t)g->label;
    /* EVENT_WRITE_EOP: CACHE_FLUSH_TS (event 4, index 5), write 64-bit
     * sequence to memory after write confirmation (INT_SEL 3). */
    *p++ = 0xc0044700u;
    *p++ = 0x00000504u;
    *p++ = (uint32_t)label;
    *p++ = ((uint32_t)(label >> 32) & 0xffffu) | 0x43000000u;
    *p++ = (uint32_t)g->sequence;
    *p++ = (uint32_t)(g->sequence >> 32);
    return (size_t)((uint8_t *)p - (uint8_t *)g->commands.p);
}

int x4_gpu_readback_transfer(X4GpuReadback *g, const void *wc_source, size_t length)
{
    if (!g) return X4_GPU_SAFE_REJECT;
    if (g->inflight) return X4_GPU_UNRESOLVED;
    if (g->poisoned) return X4_GPU_DRIVER_ERROR;
    if (!wc_source || !length ||
        length > g->capacity || (length & 3) || ((uintptr_t)wc_source & 3))
        return X4_GPU_SAFE_REJECT;
    if (((uintptr_t)g->label >> 48) || ((uintptr_t)g->label & 7))
        return X4_GPU_SAFE_REJECT;
    size_t packets = (length + X4_GPU_MAX_CHUNK - 1) / X4_GPU_MAX_CHUNK;
    if (packets > (X4_GPU_COMMAND_BYTES - 8) / (7 * sizeof(uint32_t)) ||
        packets * 7 * sizeof(uint32_t) + 6 * sizeof(uint32_t) > X4_GPU_COMMAND_BYTES - 8)
        return X4_GPU_SAFE_REJECT;
    int guard = x4_gpu_gnm_enter();
    if (guard != 0) return guard;
    /* The WB destination may hold dirty/stale CPU lines from its previous
     * use. Invalidate before GPU writes, then again after the EOP fence. */
    flush_cpu_lines(g->destination.p, length);
    if (++g->sequence == 0) ++g->sequence;
    *g->label = 0;
    size_t bytes = emit_commands(g, (uintptr_t)g->destination.p,
        (uintptr_t)wc_source, length);
    _mm_sfence();
    void *dcb[1] = {g->commands.p};
    uint32_t dcb_size[1] = {(uint32_t)bytes};
    g->inflight = true; /* Even a submit error does not prove nonexecution. */
    int rc = sceGnmSubmitCommandBuffers(1, dcb, dcb_size, NULL, NULL);
    if (rc != 0) {
        printf("Nexo X4: GPU readback submit rc=0x%08x\n", (unsigned)rc);
        g->poisoned = true;
        x4_gpu_gnm_leave();
        return X4_GPU_UNRESOLVED;
    }
    uint64_t start = sceKernelGetProcessTime();
    while (*g->label != g->sequence) {
        if (sceKernelGetProcessTime() - start >= X4_GPU_WAIT_US) {
            printf("Nexo X4: GPU readback EOP timeout bytes=%zu\n", length);
            g->poisoned = true;
            x4_gpu_gnm_leave();
            return X4_GPU_UNRESOLVED;
        }
        sceKernelUsleep(50);
    }
    _mm_mfence();
    g->inflight = false;
    rc = sceGnmSubmitDone();
    x4_gpu_gnm_leave();
    if (rc != 0) {
        printf("Nexo X4: GPU readback submitDone rc=0x%08x\n", (unsigned)rc);
        g->poisoned = true;
        return X4_GPU_DRIVER_ERROR;
    }
    flush_cpu_lines(g->destination.p, length);
    return 0;
}

const uint8_t *x4_gpu_readback_data(const X4GpuReadback *g)
{
    return g && !g->inflight ? g->destination.p : NULL;
}

bool x4_gpu_readback_inflight(const X4GpuReadback *g)
{
    return g && g->inflight;
}

int x4_gpu_readback_close(X4GpuReadback **gptr)
{
    if (!gptr || !*gptr) return 0;
    X4GpuReadback *g = *gptr;
    if (g->inflight) return X4_GPU_UNRESOLVED;
    int rc = release_direct(&g->commands);
    if (rc != 0) return rc;
    rc = release_direct(&g->destination);
    if (rc != 0) return rc;
    free(g);
    *gptr = NULL;
    return 0;
}
