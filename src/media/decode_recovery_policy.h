/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef X4_DECODE_RECOVERY_POLICY_H
#define X4_DECODE_RECOVERY_POLICY_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "../video/stream_sps.h"

/* Bounded restarts for one observed native Decode failure. The numerical
 * code's SDK meaning is unknown; callers must also verify the Decode stage. */
enum { X4_DECODE_RECOVERY_MAX = 3 };
#define X4_DECODE_RECOVERY_WINDOW_US UINT64_C(30000000)
#define X4_DECODE_OBSERVED_ERROR UINT32_C(0x811d0300)

typedef struct {
    uint64_t window_begin_us;
    unsigned attempts;
} X4DecodeRecoveryPolicy;

/* Only parsed SPS contract changes and the previously observed Decode error
 * may use the owner-thread teardown/restart path. Malformed SPS is fatal. */
static inline bool x4_decode_recovery_matches(int error, const char *stage)
{
    if (!stage) return false;
    return ((uint32_t)error == X4_DECODE_OBSERVED_ERROR &&
            !strcmp(stage, "DECODIFICAR JUEGO H264")) ||
        (error == X4_SPS_CHANGED && !strcmp(stage, "SPS H264 CAMBIADO"));
}

static inline bool x4_decode_recovery_allow(X4DecodeRecoveryPolicy *p, uint64_t now_us)
{
    if (!p || !now_us) return false;
    if (!p->window_begin_us || now_us < p->window_begin_us ||
        now_us - p->window_begin_us >= X4_DECODE_RECOVERY_WINDOW_US) {
        p->window_begin_us = now_us;
        p->attempts = 0;
    }
    if (p->attempts >= X4_DECODE_RECOVERY_MAX) return false;
    ++p->attempts;
    return true;
}

#endif
