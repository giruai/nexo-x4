/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

#define X4_AUTO_RESTORE_BASE_US UINT64_C(60000000)
#define X4_AUTO_RESTORE_MAX_US UINT64_C(900000000)

/* Main-thread policy only. The auth worker owns the actual refresh request. */
typedef struct {
    uint64_t due_us, delay_us;
    int in_flight, authorized_once, stopped;
} X4AutoRestorePolicy;

static inline void x4_auto_restore_init(X4AutoRestorePolicy *p)
{
    *p = (X4AutoRestorePolicy){.delay_us = X4_AUTO_RESTORE_BASE_US};
}

static inline void x4_auto_restore_start(X4AutoRestorePolicy *p)
{
    p->in_flight = 1;
}

static inline void x4_auto_restore_stop(X4AutoRestorePolicy *p)
{
    p->in_flight = 0;
    p->stopped = 1;
}

static inline void x4_auto_restore_delay(X4AutoRestorePolicy *p, uint64_t now_us)
{
    p->in_flight = 0;
    p->due_us = now_us + p->delay_us;
    p->delay_us = p->delay_us >= X4_AUTO_RESTORE_MAX_US / 2 ?
        X4_AUTO_RESTORE_MAX_US : p->delay_us * 2;
}

/* Called after a coherent account snapshot. Only a completed, retryable
 * renewal schedules the next attempt; busy and user-owned states never do. */
static inline int x4_auto_restore_due(X4AutoRestorePolicy *p, uint64_t now_us,
    int authorized, int retryable, int busy, int eligible)
{
    if (authorized) {
        x4_auto_restore_init(p);
        p->authorized_once = 1;
        return 0;
    }
    if (p->in_flight) {
        if (busy) return 0;
        if (!retryable) {
            x4_auto_restore_stop(p);
            return 0;
        }
        x4_auto_restore_delay(p, now_us);
    }
    if (!retryable || busy || !eligible || p->stopped) return 0;
    /* An unsuccessful boot restoration gets a first one-minute grace period;
     * an access token that expired after a successful login renews now. */
    if (!p->authorized_once && !p->due_us)
        p->due_us = now_us + X4_AUTO_RESTORE_BASE_US;
    return !p->due_us || now_us >= p->due_us;
}
