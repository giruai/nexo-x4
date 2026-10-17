/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef X4_BITRATE_CONTROL_H
#define X4_BITRATE_CONTROL_H

/* Main-thread bridge between media/session snapshots and the pure ABR policy.
 * A queued mailbox write is not evidence of an RTC send or a server ACK. */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../auth/device_auth.h"
#include "../media/live_media.h"
#include "../streaming/adaptive_bitrate.h"

enum {
    X4_BITRATE_REPORT_US = 2000000,
    X4_BITRATE_FEEDBACK_TIMEOUT_US = 5000000,
    X4_BITRATE_PROBE_US = 30000000,
    X4_BITRATE_PROBE_MAX_ATTEMPTS = 3,
    X4_BITRATE_REASON_PROBE = 100,
    X4_BITRATE_REASON_INACTIVE = 101,
    X4_BITRATE_REASON_UNSUPPORTED = 102,
    X4_BITRATE_REASON_FEEDBACK_TIMEOUT = 103,
    X4_BITRATE_REASON_SEND_FAILED = 104,
    X4_BITRATE_REASON_RESTORE = 105
};

typedef struct {
    X4AdaptiveBitrate policy;
    int stream_profile;
    uint64_t report_at_us, report_bytes, report_frames, report_valid, report_lost;
    uint64_t report_local_drops, pending_deadline_us, probe_until_us;
    uint64_t next_probe_attempt_us, next_restore_attempt_us;
    unsigned pending_kbps, pending_serial_base, probe_target_kbps;
    unsigned probe_sent_kbps, probe_attempts, restore_attempts;
    unsigned receive_kbps, fps_new_x100, loss_pct_x100;
    unsigned reason;
    bool have_report, pending, pending_is_probe, pending_is_restore;
    bool probe_active, restore_active;
} X4BitrateControl;

static inline uint64_t x4_bitrate_deadline(uint64_t now_us, uint64_t delta_us)
{
    return now_us > UINT64_MAX - delta_us ? UINT64_MAX : now_us + delta_us;
}

static inline unsigned x4_bitrate_scaled_rate(uint64_t count, uint64_t scale,
    uint64_t elapsed_us)
{
    if (!elapsed_us) return 0;
    if (count > UINT64_MAX / scale) return UINT32_MAX;
    uint64_t value = count * scale / elapsed_us;
    return value > UINT32_MAX ? UINT32_MAX : (unsigned)value;
}

static inline void x4_bitrate_init_profile(X4BitrateControl *control, uint64_t now_us,
    int profile)
{
    if (!control) return;
    memset(control, 0, sizeof(*control));
    control->stream_profile = x4_stream_profile_valid(profile) ? profile : X4_STREAM_720;
    x4_abr_init_profile(&control->policy, now_us, control->stream_profile);
}

static inline void x4_bitrate_init(X4BitrateControl *control, uint64_t now_us)
{
    x4_bitrate_init_profile(control, now_us, X4_STREAM_720);
}

/* 4000/8000 force a 30-second rate probe. Zero begins an immediate, bounded
 * restore to the selected profile's ceiling, followed by a fresh automatic-policy warmup. Switching
 * modes invalidates the old mailbox observation; the latest request wins. */
static inline void x4_bitrate_probe(X4BitrateControl *control, uint64_t now_us,
    unsigned kbps)
{
    if (!control || (kbps != 0 && kbps != 4000 && kbps != 8000)) return;
    control->pending = false;
    control->pending_is_probe = false;
    control->pending_is_restore = false;
    control->pending_kbps = 0;
    control->probe_active = kbps != 0;
    control->restore_active = kbps == 0;
    control->probe_target_kbps = kbps;
    control->probe_sent_kbps = 0;
    control->probe_attempts = 0;
    control->next_probe_attempt_us = now_us;
    control->next_restore_attempt_us = now_us;
    control->restore_attempts = 0;
    control->probe_until_us = kbps ? x4_bitrate_deadline(now_us, X4_BITRATE_PROBE_US) : 0;
    control->reason = kbps ? X4_BITRATE_REASON_PROBE : X4_BITRATE_REASON_RESTORE;
    x4_abr_init_profile(&control->policy, now_us, control->stream_profile);
}

static inline bool x4_bitrate_eligible(const X4SessionSnapshot *session,
    const X4LiveMediaSnapshot *live)
{
    return session && live && session->state == X4_SESSION_STREAMING &&
        session->error == 0 && session->rtc_connected && session->bitrate_supported &&
        session->video_packets && live->started && live->video_ready &&
        live->video_error == 0 && live->video_packets;
}

static inline void x4_bitrate_finish_pending(X4BitrateControl *control,
    uint64_t now_us, bool success)
{
    unsigned kbps = control->pending_kbps;
    if (control->pending_is_restore) {
        if (success) {
            /* This is local RTC send feedback, independent of policy warmup. */
            control->policy.sent_kbps = control->policy.max_kbps;
            control->restore_active = false;
            control->reason = X4_ABR_BASELINE;
        } else control->reason = X4_BITRATE_REASON_SEND_FAILED;
    } else if (control->pending_is_probe) {
        if (success && control->probe_active && control->probe_target_kbps == kbps) {
            control->probe_sent_kbps = kbps;
            control->reason = X4_BITRATE_REASON_PROBE;
        } else if (!success)
            control->reason = X4_BITRATE_REASON_SEND_FAILED;
    } else {
        x4_abr_note_send(&control->policy, now_us, kbps, success);
        if (!success) control->reason = X4_BITRATE_REASON_SEND_FAILED;
    }
    control->pending = false;
    control->pending_is_probe = false;
    control->pending_is_restore = false;
    control->pending_kbps = 0;
}

static inline void x4_bitrate_report(X4BitrateControl *control, uint64_t now_us,
    const X4SessionSnapshot *session, const X4LiveMediaSnapshot *live,
    bool eligible)
{
    if (!control->have_report || now_us < control->report_at_us) {
        control->report_at_us = now_us;
        control->report_bytes = live->video_received_bytes;
        control->report_frames = live->video_present_new;
        control->report_valid = live->video_packets;
        control->report_lost = live->video_lost_packets;
        control->report_local_drops = live->video_ingress_dropped;
        control->have_report = true;
        return;
    }
    uint64_t elapsed = now_us - control->report_at_us;
    if (elapsed < X4_BITRATE_REPORT_US) return;
    uint64_t bytes = live->video_received_bytes >= control->report_bytes ?
        live->video_received_bytes - control->report_bytes : 0;
    uint64_t frames = live->video_present_new >= control->report_frames ?
        live->video_present_new - control->report_frames : 0;
    uint64_t valid = live->video_packets >= control->report_valid ?
        live->video_packets - control->report_valid : 0;
    uint64_t lost = live->video_lost_packets >= control->report_lost ?
        live->video_lost_packets - control->report_lost : 0;
    uint64_t local_drops = live->video_ingress_dropped >= control->report_local_drops ?
        live->video_ingress_dropped - control->report_local_drops : 0;
    control->receive_kbps = x4_bitrate_scaled_rate(bytes, 8000u, elapsed);
    control->fps_new_x100 = x4_bitrate_scaled_rate(frames, 100000000u, elapsed);
    uint64_t total = valid <= UINT64_MAX - lost ? valid + lost : UINT64_MAX;
    control->loss_pct_x100 = total ?
        (unsigned)(lost <= UINT64_MAX / 10000u ? lost * 10000u / total :
            (long double)lost * 10000.0L / (long double)total) : 0;
    unsigned desired = control->probe_active ? control->probe_target_kbps :
        control->restore_active ? control->policy.max_kbps : control->policy.desired_kbps;
    unsigned sent = control->probe_active ? control->probe_sent_kbps :
        control->policy.sent_kbps;
    printf("[x4 bitrate] mode=%s eligible=%u desired=%u sent=%u requested=%u "
           "pending=%u receive_kbps=%u width=%u height=%u fps_new_x100=%u "
           "valid=%llu lost=%llu loss_pct_x100=%u local_drops=%llu reason=%u\n",
        control->probe_active ? "probe" : control->restore_active ? "restore" : "auto",
        eligible ? 1u : 0u,
        desired, sent, session->bitrate_requested_kbps, control->pending_kbps,
        control->receive_kbps, live->width, live->height, control->fps_new_x100,
        (unsigned long long)valid, (unsigned long long)lost,
        control->loss_pct_x100, (unsigned long long)local_drops, control->reason);
    control->report_at_us = now_us;
    control->report_bytes = live->video_received_bytes;
    control->report_frames = live->video_present_new;
    control->report_valid = live->video_packets;
    control->report_lost = live->video_lost_packets;
    control->report_local_drops = live->video_ingress_dropped;
}

/* Call once per main-loop iteration with copied snapshots. The worker's
 * feedback proves only that the local RTC send was attempted successfully. */
static inline void x4_bitrate_tick(X4BitrateControl *control, uint64_t now_us,
    X4Auth *auth, const X4SessionSnapshot *session,
    const X4LiveMediaSnapshot *live)
{
    if (!control || !session || !live) return;
    if (control->probe_active && now_us >= control->probe_until_us)
        x4_bitrate_probe(control, now_us, 0);
    bool eligible = auth && x4_bitrate_eligible(session, live);
    if (control->pending) {
        if (session->bitrate_feedback_serial != control->pending_serial_base &&
            session->bitrate_requested_kbps == control->pending_kbps) {
            x4_bitrate_finish_pending(control, now_us,
                session->bitrate_feedback_rc == 0);
        } else if (now_us >= control->pending_deadline_us || !eligible) {
            x4_bitrate_finish_pending(control, now_us, false);
            control->reason = X4_BITRATE_REASON_FEEDBACK_TIMEOUT;
        }
    }
    if (!eligible) {
        control->reason = session->bitrate_supported ?
            X4_BITRATE_REASON_INACTIVE : X4_BITRATE_REASON_UNSUPPORTED;
        x4_bitrate_report(control, now_us, session, live, false);
        return;
    }
    if (control->probe_active) {
        if (control->reason != X4_BITRATE_REASON_FEEDBACK_TIMEOUT &&
            control->reason != X4_BITRATE_REASON_SEND_FAILED)
            control->reason = X4_BITRATE_REASON_PROBE;
        if (!control->pending && control->probe_sent_kbps != control->probe_target_kbps &&
            control->probe_attempts < X4_BITRATE_PROBE_MAX_ATTEMPTS &&
            now_us >= control->next_probe_attempt_us) {
            unsigned kbps = control->probe_target_kbps;
            int rc = x4_auth_request_bitrate(auth, kbps);
            ++control->probe_attempts;
            control->next_probe_attempt_us = x4_bitrate_deadline(now_us, X4_ABR_RETRY_US);
            if (rc == 0) {
                control->pending = true;
                control->pending_is_probe = true;
                control->pending_kbps = kbps;
                control->pending_serial_base = session->bitrate_feedback_serial;
                control->pending_deadline_us = x4_bitrate_deadline(now_us,
                    X4_BITRATE_FEEDBACK_TIMEOUT_US);
            } else control->reason = X4_BITRATE_REASON_SEND_FAILED;
        }
    } else if (control->restore_active) {
        if (control->reason != X4_BITRATE_REASON_FEEDBACK_TIMEOUT &&
            control->reason != X4_BITRATE_REASON_SEND_FAILED)
            control->reason = X4_BITRATE_REASON_RESTORE;
        if (!control->pending &&
            control->restore_attempts < X4_BITRATE_PROBE_MAX_ATTEMPTS &&
            now_us >= control->next_restore_attempt_us) {
            int rc = x4_auth_request_bitrate(auth, control->policy.max_kbps);
            ++control->restore_attempts;
            control->next_restore_attempt_us = x4_bitrate_deadline(now_us,
                X4_ABR_RETRY_US);
            if (rc == 0) {
                control->pending = true;
                control->pending_is_restore = true;
                control->pending_kbps = control->policy.max_kbps;
                control->pending_serial_base = session->bitrate_feedback_serial;
                control->pending_deadline_us = x4_bitrate_deadline(now_us,
                    X4_BITRATE_FEEDBACK_TIMEOUT_US);
            } else control->reason = X4_BITRATE_REASON_SEND_FAILED;
        }
        if (!control->pending &&
            control->restore_attempts >= X4_BITRATE_PROBE_MAX_ATTEMPTS)
            control->restore_active = false;
    } else {
        X4AbrSnapshot counters = {
            .valid_video_packets = live->video_packets,
            .lost_packets = live->video_lost_packets,
            .local_dropped_packets = live->video_ingress_dropped,
            .damaged_frames = live->video_dropped_frames,
            .pli_requests = live->keyframe_requests,
            .received_video_bytes = live->video_received_bytes,
            .ingress_oldest_us = live->video_queue_oldest_age_us
        };
        X4AbrDecision decision = x4_abr_update(&control->policy, now_us, &counters);
        if (decision.reason != X4_ABR_HOLD) control->reason = decision.reason;
        if (decision.send_due && !control->pending) {
            int rc = x4_auth_request_bitrate(auth, decision.send_kbps);
            if (rc == 0) {
                control->pending = true;
                control->pending_is_probe = false;
                control->pending_kbps = decision.send_kbps;
                control->pending_serial_base = session->bitrate_feedback_serial;
                control->pending_deadline_us = x4_bitrate_deadline(now_us,
                    X4_BITRATE_FEEDBACK_TIMEOUT_US);
            } else {
                x4_abr_note_send(&control->policy, now_us, decision.send_kbps, false);
                control->reason = X4_BITRATE_REASON_SEND_FAILED;
            }
        }
    }
    x4_bitrate_report(control, now_us, session, live, true);
}

#endif
