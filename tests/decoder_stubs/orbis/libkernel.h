/* SPDX-License-Identifier: GPL-3.0-only */
/* Native video ABI fixture only. Never add this path to a PS4 build. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <pthread.h>
typedef pthread_t OrbisPthread;
uint64_t sceKernelGetProcessTime(void);
int sceKernelUsleep(unsigned);
int scePthreadCreate(OrbisPthread *, const pthread_attr_t *, void *(*)(void *), void *, const char *);
int scePthreadJoin(OrbisPthread, void **);
size_t sceKernelGetDirectMemorySize(void);
int sceKernelAllocateDirectMemory(off_t, off_t, size_t, size_t, int, off_t *);
int sceKernelMapDirectMemory(void **, size_t, int, int, off_t, size_t);
int sceKernelMunmap(void *, size_t);
int sceKernelReleaseDirectMemory(off_t, size_t);
