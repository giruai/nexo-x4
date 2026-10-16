/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef X4_GPU_READBACK_H
#define X4_GPU_READBACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct X4GpuReadback X4GpuReadback;

/* Allocates persistent GPU-visible command/label storage and a CPU-cached
 * WB_ONION destination. The caller must validate the first real transfer
 * against its existing CPU copy before enabling this path. */
int x4_gpu_readback_open(X4GpuReadback **out, size_t capacity);

/* Returns 0 for a completed copy, positive for a safe rejection before GPU
 * submission, and negative if submission/completion may be unresolved. The
 * source must be a GPU-readable direct-memory allocation and must remain
 * alive while x4_gpu_readback_inflight() is true. */
int x4_gpu_readback_transfer(X4GpuReadback *g, const void *wc_source, size_t length);
const uint8_t *x4_gpu_readback_data(const X4GpuReadback *g);
bool x4_gpu_readback_inflight(const X4GpuReadback *g);

/* Refuses to release any reservation if a GPU submission may still own it. */
int x4_gpu_readback_close(X4GpuReadback **g);

/* Serialize all this process's GNM submit/SubmitDone calls. The display path
 * should hold this guard around its sceGnmSubmitDone call. */
int x4_gpu_gnm_enter(void);
void x4_gpu_gnm_leave(void);

#endif
