/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "text_input.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../input/gamepad.h"
#include "stream_profile.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct X4Rtc X4Rtc;
typedef struct X4Trace X4Trace;
typedef struct X4Rumble X4Rumble;
typedef struct X4NackGraceInbox X4NackGraceInbox;
enum { X4_RTC_VIDEO = 0, X4_RTC_AUDIO = 1 };
typedef enum {
    X4_RTC_NEW = 0, X4_RTC_CONNECTING, X4_RTC_CONNECTED,
    X4_RTC_DISCONNECTED, X4_RTC_FAILED, X4_RTC_CLOSED
} X4RtcState;
typedef struct {
    X4RtcState state;
    int error;
    bool gathering_done;
    uint64_t video_packets, audio_packets;
    bool input_ready;
    int input_error;
    /* Native send calls that succeeded, and superseded/error drops. These
     * do not acknowledge Xbox consumption of controller reports. */
    uint64_t input_packets, input_dropped;
    bool bitrate_supported;
} X4RtcSnapshot;
/* Called on a transport thread, with an authenticated/decrypted RTP packet.
 * The pointer is borrowed for this call only. Copy into a bounded queue;
 * never decode, block or render on this callback. */
typedef void (*X4RtcMediaCallback)(void *user, int kind, const uint8_t *rtp, size_t size);
X4Rtc *x4_rtc_open(int *error);
/* Immutable profile copied when the RTC instance is created. */
X4Rtc *x4_rtc_open_profile(int profile, int *error);
void x4_rtc_set_media_callback(X4Rtc *, X4RtcMediaCallback, void *user);
/* Session owner only, before dispatch starts. Trace outlives joined RTC. */
void x4_rtc_set_diagnostic_trace(X4Rtc *, X4Trace *);
/* Optional borrowed storage, bound by owner before remote SDP/callbacks.
 * Both objects outlive all producers and a successful RTC close. */
void x4_rtc_set_rumble_mailbox(X4Rtc *, X4Rumble *);
void x4_rtc_set_nack_grace_inbox(X4Rtc *, X4NackGraceInbox *);
/* Owner thread only, once after open. Starts a bounded independent sender;
 * a native thread-creation error is returned without failing media/RTC.
 * Source/context must outlive a successful close. */
int x4_rtc_set_gamepad_source(X4Rtc *, X4GamepadSource, void *user);
/* Returns 1 when copied, 0 while pending, negative on failure. */
int x4_rtc_local_description(X4Rtc *, char *sdp, size_t capacity);
int x4_rtc_next_local_candidate(X4Rtc *, char *candidate, size_t capacity,
                                char *mid, size_t mid_capacity);
int x4_rtc_set_remote_description(X4Rtc *, const char *sdp);
int x4_rtc_add_remote_candidate(X4Rtc *, const char *candidate, const char *mid);
void x4_rtc_snapshot(X4Rtc *, X4RtcSnapshot *);
int x4_rtc_request_keyframe(X4Rtc *);
/* Session owner only. kbps are converted to RTCP REMB bits/second; a zero
 * return is local API acceptance, never acknowledgment from Xbox. */
int x4_rtc_request_bitrate(X4Rtc *, unsigned kbps);
/* Joins callbacks before releasing the context. Call only from the owner
 * worker, never from a callback. */
/* A join error retains the complete RTC/provider context. The owner must
 * keep it alive and retry; no callback/channel/context is freed on error. */
int x4_rtc_close(X4Rtc *);
#ifdef __cplusplus
}
#endif

/* Configure only before SDP/channels open; worker pumps replies after startup. */
void x4_rtc_set_text_input(X4Rtc *rtc, X4TextInput *input);
void x4_rtc_pump_text(X4Rtc *rtc);
