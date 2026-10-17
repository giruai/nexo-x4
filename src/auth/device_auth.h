/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "xbox_catalog.h"
#include "xbox_session.h"
#include "../input/gamepad.h"
#include "../input/rumble.h"
#include "../streaming/stream_profile.h"
#include "../streaming/text_input.h"

/* Microsoft account sign-in (OAuth 2.0 device authorization grant) on one
 * background worker. AUTHORIZED only means a Microsoft token was acquired;
 * it implies no Xbox Live session or game access. Tokens stay private.
 * The same worker reads the Xbox cloud catalog with that token; Xbox
 * credentials exist only during that run and never reach a snapshot. A
 * session run also renews the Microsoft access with its refresh token (a
 * rotated refresh token is kept) to obtain the console-transfer token for
 * the session's /connect; none of these tokens reaches a snapshot. */

/* Local errors; native Net/Ssl/Http/pthread errors are reported unchanged. */
enum {
    X4_AUTH_E_ARGUMENT = -2001,
    X4_AUTH_E_BUSY = -2002,
    X4_AUTH_E_ALLOCATION = -2003,
    X4_AUTH_E_AUTHORIZED = -2004, /* connection check refused: token held */
    X4_AUTH_E_FORM = -2005,
    X4_AUTH_E_RESPONSE = -2006,   /* malformed or out-of-bounds response */
    X4_AUTH_E_STATUS = -2007,     /* unexpected HTTP status */
    X4_AUTH_E_REJECTED = -2008,   /* OAuth error returned by Microsoft */
    X4_AUTH_E_SIGNED_OUT = -2009, /* catalog refused: no valid Microsoft token */
    X4_AUTH_E_XBOX = -2010,       /* Xbox Live or cloud gaming refused the account */
    X4_AUTH_E_REGION = -2011,     /* no usable default streaming region */
    X4_AUTH_E_SESSION = -2012,    /* Xbox reported the cloud session failed */
    X4_AUTH_E_DEADLINE = -2013,   /* session not ready within its deadline */
    X4_AUTH_E_UNCONFIRMED = -2014, /* a remote session may exist; not confirmed closed */
    X4_AUTH_E_STORE = -2015, /* local refresh-token removal failed */
};

typedef struct X4Auth X4Auth;

/* Main-owned cursor paired with one catalog output buffer. Zero-initialize
 * it before first use and reset it when replacing the auth context or output
 * storage. It contains no credential and must not be shared between callers. */
typedef struct {
    const X4Auth *source;
    uint64_t revision;
    int initialized, cancel_visible;
} X4AuthCatalogCursor;

enum X4AuthState {
    X4_AUTH_IDLE,
    X4_AUTH_CONNECTING,
    X4_AUTH_WAITING,
    X4_AUTH_CONNECTED,
    X4_AUTH_AUTHORIZED,
    X4_AUTH_CANCELLED,
    X4_AUTH_EXPIRED,
    X4_AUTH_DENIED,
    X4_AUTH_ERROR,
};

/* X4_AUTH_XBOX_SESSION is dispatched only through x4_auth_start_session;
 * x4_auth_start refuses it. */
enum X4AuthAction { X4_AUTH_SIGN_IN, X4_AUTH_CHECK_CONNECTION, X4_AUTH_XBOX_CATALOG, X4_AUTH_XBOX_SESSION,
    X4_AUTH_RESTORE /* internal worker action; x4_auth_start refuses it */ };

typedef struct {
    enum X4AuthState state;
    int error, http_status;
    unsigned seconds_left;
    char stage[80], user_code[32], verification_uri[256];
} X4AuthSnapshot;

/* Public operations belong to main except the synchronized input source and
 * cancel/keyframe-request flags. */
X4Auth *x4_auth_create(void);
/* Main thread only, before starting a session. Returns BUSY while any auth
 * worker runs; the selected profile is copied into each session worker. */
int x4_auth_set_stream_profile(X4Auth *auth, int profile);
int x4_auth_stream_profile(const X4Auth *auth);
/* Main thread only, while no worker is busy. Registration persists across
 * sign-ins. A completed worker is joined before replacing the registration;
 * a join error preserves the old registration. The receiver context must
 * outlive every session worker. */
typedef struct X4Trace X4Trace;
int x4_auth_set_media_callback(X4Auth *auth, X4SessionMediaCallback callback, void *user, X4Trace *trace);
typedef struct X4NackGraceInbox X4NackGraceInbox;
int x4_auth_set_grace_inbox(X4Auth *auth, X4NackGraceInbox *inbox);
/* Main-only desired motors. Native controller calls remain on main. */
bool x4_auth_poll_rumble(X4Auth *auth, uint64_t now_us, X4RumbleOutput *out);
/* Thread-safe request flag. The session owner consumes it and requests PLI;
 * this never accesses the transport from the main/decoder thread. */
void x4_auth_request_keyframe(X4Auth *auth);
/* Thread-safe latest-desired-rate mailbox for an active session worker.
 * 0 means queued, not accepted by RTC or Xbox. The session snapshot reports
 * the worker's local send result separately. */
int x4_auth_request_bitrate(X4Auth *auth, unsigned kbps);
/* Main publishes a copied normalized controller state each UI iteration.
 * The RTC sender reads it under a short gate; stale states (>250 ms) and
 * cancellation produce neutral reports. No controller or token pointer is
 * shared with the sender. */
void x4_auth_set_gamepad(X4Auth *auth, const X4GamepadFrame *frame);
/* 0 when the worker was started. Refuses while busy, refuses a connection
 * check while a token is held, and refuses the catalog without a valid one.
 * Sign-in discards any old token and catalog. The catalog run publishes
 * LOADING, then READY, ERROR or CANCELLED; the account stays AUTHORIZED
 * through it while the Microsoft token remains valid. */
int x4_auth_start(X4Auth *auth, enum X4AuthAction action);
/* Main thread only. Renew with an already held refresh token on the existing
 * worker. Never starts device authorization or reads a different profile. */
int x4_auth_restore_saved_if_idle(X4Auth *auth);
/* Sets the cancel flag only; poll x4_auth_busy until it reports false. */
void x4_auth_cancel(X4Auth *auth);
int x4_auth_busy(X4Auth *auth);
void x4_auth_snapshot(X4Auth *auth, X4AuthSnapshot *out);
/* Copy of the last catalog view; it holds no token, user hash or body. */
void x4_auth_catalog_snapshot(X4Auth *auth, X4CatalogSnapshot *out);
/* Main only: returns 1 after copying a changed view, otherwise 0. The paired
 * output remains valid when 0 is returned. Token expiry is still checked and
 * immediate cancellation visibility is part of the cache key. A NULL cursor
 * requests an unconditional copy, as the legacy snapshot above does. */
int x4_auth_catalog_snapshot_cached(X4Auth *auth, X4CatalogSnapshot *out,
    X4AuthCatalogCursor *cursor);
/* Prepares an Xbox cloud session for catalog title catalog_index with the
 * catalog's offering and authorizes its connection (see xbox_session.h). A
 * registered receiver obtains actual RTP once WebRTC connects. Requires a READY catalog, a valid index, a
 * valid Microsoft token and no running worker. Publishes STARTING before the
 * worker starts. The account stays AUTHORIZED and the catalog view is kept,
 * except when Microsoft refuses the renewal with invalid_grant: then the
 * local token and catalog are dropped and the account turns EXPIRED (no
 * global sign-out). x4_auth_cancel stops it; busy stays true through
 * negotiation, streaming and remote cleanup until every credential is wiped and
 * HTTPS is closed. */
int x4_auth_start_session(X4Auth *auth, unsigned catalog_index);
/* Copy of the last session view; no token, session path or ID. */
void x4_auth_session_snapshot(X4Auth *auth, X4SessionSnapshot *out);
/* Removes this PS4 user's saved refresh token and wipes the local session;
 * no global Microsoft sign-out. An I/O error leaves the session intact. */
int x4_auth_forget(X4Auth *auth);
/* Call once busy is false. On a join error the context is kept. */
int x4_auth_close(X4Auth *auth);

/* Main-thread local game-text adapter. Text never enters public snapshots or logs. */
X4TextEvent x4_auth_poll_game_text(X4Auth *auth, X4TextRequest *request);
int x4_auth_complete_game_text(X4Auth *auth, const char *id, const char *text);
int x4_auth_cancel_game_text(X4Auth *auth, const char *id);
