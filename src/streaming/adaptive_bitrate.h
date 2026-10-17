/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef X4_ADAPTIVE_BITRATE_H
#define X4_ADAPTIVE_BITRATE_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "stream_profile.h"

enum {
    X4_ABR_MIN_KBPS = 3000,
    X4_ABR_MAX_KBPS = 8000,
    X4_ABR_START_KBPS = 8000,
    X4_ABR_RAISE_KBPS = 250,
    X4_ABR_MIN_WINDOW_US = 1500000,
    X4_ABR_MAX_WINDOW_US = 5000000,
    X4_ABR_WARMUP_US = 10000000,
    X4_ABR_HOLD_DOWN_US = 10000000,
    X4_ABR_RECOVERY_US = 20000000,
    X4_ABR_RETRY_US = 2000000,
    X4_ABR_STALE_INGRESS_US = 50000,
    X4_ABR_MIN_WINDOW_PACKETS = 100,
    X4_ABR_MIN_WARMUP_PACKETS = 250,
    X4_ABR_MIN_WARMUP_WINDOWS = 5,
    X4_ABR_MAX_SEND_ATTEMPTS = 3
};

/* Cumulative, monotonic counters for one session. received_video_bytes is
 * optional: pass zero consistently when unavailable. It is only displayed;
 * it never influences the desired bitrate. */
typedef struct {
    uint64_t valid_video_packets;
    uint64_t lost_packets;
    uint64_t local_dropped_packets;
    uint64_t damaged_frames;
    uint64_t pli_requests;
    uint64_t received_video_bytes;
    uint64_t ingress_oldest_us;
} X4AbrSnapshot;

typedef enum {
    X4_ABR_HOLD,
    X4_ABR_BASELINE,
    X4_ABR_WARMUP,
    X4_ABR_EMPTY,
    X4_ABR_GAP,
    X4_ABR_REGRESSION,
    X4_ABR_LOCAL_BACKLOG,
    X4_ABR_HOLD_DOWN,
    X4_ABR_LOSS_BURST,
    X4_ABR_LOSS_SUSTAINED,
    X4_ABR_CLEAN_RECOVERY
} X4AbrReason;

typedef struct {
    X4AbrReason reason;
    uint32_t desired_kbps;
    uint32_t sent_kbps;
    uint32_t send_kbps;
    uint32_t receive_kbps; /* observed receive rate; never a capacity estimate */
    bool send_due; /* Reserves one attempt; call x4_abr_note_send afterwards. */
} X4AbrDecision;

typedef struct {
    X4AbrSnapshot last;
    uint64_t warmup_start_us, last_sample_us, clean_since_us, last_reduce_us;
    uint64_t next_send_us;
    uint32_t desired_kbps, sent_kbps, pending_kbps;
    uint32_t min_kbps, max_kbps;
    uint16_t sampled_packets;
    uint8_t sampled_windows, loss_streak, send_attempts;
    bool have_snapshot, clean_active, has_reduced, send_pending;
} X4AdaptiveBitrate;

static inline void x4_abr_init_profile(X4AdaptiveBitrate *p, uint64_t now_us, int profile)
{
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->warmup_start_us = now_us;
    X4StreamProfile config = x4_stream_profile_get(profile);
    p->min_kbps = config.min_kbps;
    p->max_kbps = config.max_kbps;
    p->desired_kbps = config.max_kbps;
    /* Startup JSON announces a cap; it is not a local RTC rate request. */
    p->sent_kbps = 0;
}

static inline void x4_abr_init(X4AdaptiveBitrate *p, uint64_t now_us)
{
    x4_abr_init_profile(p, now_us, X4_STREAM_720);
}

static inline bool x4_abr_counters_regressed(const X4AbrSnapshot *now,
    const X4AbrSnapshot *last)
{
    return now->valid_video_packets < last->valid_video_packets ||
        now->lost_packets < last->lost_packets ||
        now->local_dropped_packets < last->local_dropped_packets ||
        now->damaged_frames < last->damaged_frames ||
        now->pli_requests < last->pli_requests ||
        now->received_video_bytes < last->received_video_bytes;
}

static inline void x4_abr_rebaseline(X4AdaptiveBitrate *p, uint64_t now_us,
    const X4AbrSnapshot *snapshot)
{
    p->last = *snapshot;
    p->last_sample_us = p->warmup_start_us = now_us;
    p->sampled_packets = p->sampled_windows = p->loss_streak = 0;
    p->clean_active = false;
    p->have_snapshot = true;
}

static inline X4AbrDecision x4_abr_update(X4AdaptiveBitrate *p, uint64_t now_us,
    const X4AbrSnapshot *snapshot)
{
    X4AbrDecision d = {0};
    if (!p || !snapshot) return d;
    d.desired_kbps = p->desired_kbps;
    d.sent_kbps = p->sent_kbps;
    if (!p->have_snapshot) {
        x4_abr_rebaseline(p, now_us, snapshot);
        d.reason = X4_ABR_BASELINE;
        return d;
    }
    if (now_us < p->last_sample_us || x4_abr_counters_regressed(snapshot, &p->last)) {
        x4_abr_rebaseline(p, now_us, snapshot);
        d.reason = X4_ABR_REGRESSION;
        return d;
    }
    uint64_t elapsed = now_us - p->last_sample_us;
    if (elapsed < X4_ABR_MIN_WINDOW_US) return d;
    if (elapsed > X4_ABR_MAX_WINDOW_US) {
        x4_abr_rebaseline(p, now_us, snapshot);
        d.reason = X4_ABR_GAP;
        return d;
    }
    uint64_t valid = snapshot->valid_video_packets - p->last.valid_video_packets;
    uint64_t lost = snapshot->lost_packets - p->last.lost_packets;
    uint64_t local_dropped = snapshot->local_dropped_packets - p->last.local_dropped_packets;
    uint64_t damaged = snapshot->damaged_frames - p->last.damaged_frames;
    uint64_t pli = snapshot->pli_requests - p->last.pli_requests;
    uint64_t bytes = snapshot->received_video_bytes - p->last.received_video_bytes;
    uint64_t window_start = p->last_sample_us;
    p->last = *snapshot;
    p->last_sample_us = now_us;
    if (bytes <= UINT64_MAX / 8000u) {
        uint64_t rate = bytes * 8000u / elapsed;
        d.receive_kbps = rate > UINT32_MAX ? UINT32_MAX : (uint32_t)rate;
    } else d.receive_kbps = UINT32_MAX;
    if (valid > UINT64_MAX - lost || valid + lost < X4_ABR_MIN_WINDOW_PACKETS) {
        p->loss_streak = 0;
        p->clean_active = false;
        d.reason = X4_ABR_EMPTY;
        return d;
    }
    uint64_t total = valid + lost;
    if (local_dropped || snapshot->ingress_oldest_us > X4_ABR_STALE_INGRESS_US) {
        p->loss_streak = 0;
        p->clean_active = false;
        d.reason = X4_ABR_LOCAL_BACKLOG;
        return d;
    }
    if (p->sampled_windows < X4_ABR_MIN_WARMUP_WINDOWS) ++p->sampled_windows;
    uint64_t remaining = X4_ABR_MIN_WARMUP_PACKETS - p->sampled_packets;
    p->sampled_packets = total >= remaining ? X4_ABR_MIN_WARMUP_PACKETS :
        (uint16_t)(p->sampled_packets + total);
    bool burst = lost >= total / 20u + (total % 20u != 0);
    bool loss = lost >= total / 50u + (total % 50u != 0);
    if (loss) { if (p->loss_streak < 2) ++p->loss_streak; }
    else p->loss_streak = 0;
    bool clean = lost == 0 && damaged == 0 && pli == 0;
    if (clean) {
        if (!p->clean_active) { p->clean_active = true; p->clean_since_us = window_start; }
    } else p->clean_active = false;
    if (now_us - p->warmup_start_us < X4_ABR_WARMUP_US ||
        p->sampled_windows < X4_ABR_MIN_WARMUP_WINDOWS ||
        p->sampled_packets < X4_ABR_MIN_WARMUP_PACKETS) {
        d.reason = X4_ABR_WARMUP;
        return d;
    }
    if (burst || (loss && p->loss_streak >= 2)) {
        if (p->has_reduced && now_us - p->last_reduce_us < X4_ABR_HOLD_DOWN_US)
            d.reason = X4_ABR_HOLD_DOWN;
        else if (p->desired_kbps > p->min_kbps) {
            uint32_t reduced = p->desired_kbps * 4u / 5u;
            p->desired_kbps = reduced < p->min_kbps ? p->min_kbps : reduced;
            p->last_reduce_us = now_us;
            p->has_reduced = true;
            p->clean_active = false;
            d.reason = burst ? X4_ABR_LOSS_BURST : X4_ABR_LOSS_SUSTAINED;
        }
    } else if (clean && p->clean_active && p->desired_kbps < p->max_kbps &&
        now_us - p->clean_since_us >= X4_ABR_RECOVERY_US &&
        (!p->has_reduced || now_us - p->last_reduce_us >= X4_ABR_HOLD_DOWN_US)) {
        uint32_t raised = p->desired_kbps + X4_ABR_RAISE_KBPS;
        p->desired_kbps = raised > p->max_kbps ? p->max_kbps : raised;
        p->clean_since_us = now_us;
        d.reason = X4_ABR_CLEAN_RECOVERY;
    }
    if (p->desired_kbps != d.desired_kbps) {
        p->send_attempts = 0;
        p->next_send_us = now_us;
    }
    d.desired_kbps = p->desired_kbps;
    if (p->desired_kbps != p->sent_kbps && !p->send_pending &&
        p->send_attempts < X4_ABR_MAX_SEND_ATTEMPTS && now_us >= p->next_send_us) {
        p->pending_kbps = p->desired_kbps;
        p->send_pending = true;
        ++p->send_attempts;
        p->next_send_us = now_us > UINT64_MAX - X4_ABR_RETRY_US ? UINT64_MAX :
            now_us + X4_ABR_RETRY_US;
        d.send_kbps = p->pending_kbps;
        d.send_due = true;
    }
    return d;
}

/* Acknowledges only the rate reserved by the most recent send_due. Failed
 * sends leave sent_kbps unchanged and permit at most three attempts per rate. */
static inline void x4_abr_note_send(X4AdaptiveBitrate *p, uint64_t now_us,
    uint32_t kbps, bool success)
{
    (void)now_us;
    if (!p || !p->send_pending || kbps != p->pending_kbps) return;
    p->send_pending = false;
    if (success) { p->sent_kbps = kbps; p->send_attempts = 0; }
}

#endif
