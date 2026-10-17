/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "queue_estimate.h"

/* Public view of one Xbox cloud session preparation. This milestone asks
 * Xbox to provision a cloud session, waits until the server reports it ready
 * to negotiate, then authorizes the connection: the Microsoft access is
 * renewed, a console-transfer token is obtained from Microsoft and sent to
 * the session's /connect. A real RTC peer then exchanges SDP and ICE;
 * authenticated media packets go to the registered receiver. Every prepared session is deleted before the worker
 * releases its private credentials.
 *
 *   IDLE -> STARTING -> WAITING -> READY -> AUTHORIZING -> AUTHORIZED
 *        -> NEGOTIATING -> CONNECTING -> STREAMING
 *        -> STOPPING -> CLOSED
 *   any active state -> STOPPING -> CANCELLED (user cancel, cleanup confirmed)
 *   any state -> ERROR (service/transport failure, deadline or failed cleanup)
 *
 * READY is set only for the server states ReadyToConnect or Provisioned and
 * means "ready to negotiate", not streaming. ready_seen stays 1 afterwards.
 * AUTHORIZING is published before the Microsoft exchanges start. AUTHORIZED
 * only means Xbox accepted the /connect request (2xx without errorDetails);
 * it does not mean a media connection exists. connection_authorized stays 1
 * afterwards, also once the session was closed. cleanup_failed is 1 whenever
 * the remote session could not be confirmed closed; it is never cleared by
 * a cancellation. */
enum X4SessionState {
    X4_SESSION_IDLE,
    X4_SESSION_STARTING,
    X4_SESSION_WAITING,
    X4_SESSION_READY,
    X4_SESSION_STOPPING,
    X4_SESSION_CLOSED,
    X4_SESSION_CANCELLED,
    X4_SESSION_ERROR,
    X4_SESSION_AUTHORIZING,
    X4_SESSION_AUTHORIZED,
    X4_SESSION_NEGOTIATING,
    X4_SESSION_CONNECTING,
    X4_SESSION_STREAMING,
};

/* Last validated /state response; UNKNOWN includes unrecognized server
 * strings. This is observed state, independent of the optional wait estimate. */
enum X4RemoteQueueState {
    X4_REMOTE_UNKNOWN, X4_REMOTE_WAITING_FOR_RESOURCES, X4_REMOTE_PROVISIONING,
    X4_REMOTE_READY_TO_CONNECT, X4_REMOTE_PROVISIONED, X4_REMOTE_FAILED
};

/* Holds no token, session path, session ID or server body. stage is always
 * copied from a constant literal. passport_http_status is the status of the
 * last Microsoft exchange of the authorization step (access renewal or
 * console-transfer token); connect_http_status that of /connect; 0 when the
 * exchange did not return a status. */
typedef struct {
    enum X4SessionState state;
    int error, http_status;
    int cleanup_error, cleanup_http_status;
    int ready_seen, cleanup_failed;
    int connection_authorized, passport_http_status, connect_http_status;
    int rtc_connected, sdp_http_status, ice_http_status, keepalive_http_status;
    uint64_t video_packets, audio_packets;
    /* Local RTC send feedback, not an acknowledgement from the server. */
    unsigned bitrate_requested_kbps, bitrate_feedback_serial;
    int bitrate_feedback_rc;
    bool bitrate_supported;
    int input_ready, input_error;
    uint64_t input_packets, input_dropped;
    unsigned elapsed_seconds, seconds_left;
    enum X4RemoteQueueState remote_state;
    unsigned state_poll_count, last_state_poll_elapsed_seconds;
    X4QueueEstimate queue_estimate;
    char stage[80], region[80], offering[24], title_name[128];
} X4SessionSnapshot;

/* Authenticated, decrypted RTP. kind 0 is H264 video, kind 1 Opus audio.
 * Borrowed pointer, valid only for the call on a transport thread. Copy
 * immediately to a bounded queue; decoding/rendering must happen elsewhere.
 * Caller keeps user alive until cancellation and worker join have completed. */
typedef void (*X4SessionMediaCallback)(void *user, int kind, const uint8_t *rtp, size_t size);
