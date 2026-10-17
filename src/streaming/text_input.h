/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The service's ShowVirtualKeyboard transaction. All strings are UTF-8.
 * Limits are local safety bounds, not a claim about the service's limits. */
#define X4_TEXT_MAX_CHARS 1000u
#define X4_TEXT_MAX_BYTES 4000u
#define X4_TEXT_WIRE_MAX 16384u
typedef struct X4TextInput X4TextInput;
typedef struct {
    char id[81];
    char title[193];
    char description[385];
    char default_text[X4_TEXT_MAX_BYTES + 1];
    unsigned input_scope;
    unsigned max_chars; /* 1..1000; service MaxLength=0 becomes 1000. */
} X4TextRequest;
typedef enum { X4_TEXT_EVENT_NONE, X4_TEXT_EVENT_REQUEST, X4_TEXT_EVENT_WITHDRAWN } X4TextEvent;

X4TextInput *x4_text_input_create(void);
/* A caller holding a live reference may retain another one for an RTC object.
 * Returns NULL only if the reference counter cannot grow. A quarantined RTC
 * retains its reference until that RTC is actually freed. */
X4TextInput *x4_text_input_retain(X4TextInput *input);
/* Releases one reference; the last release wipes requests and frees storage. */
void x4_text_input_destroy(X4TextInput *input);
/* Called on RTC message callback; unrelated messages return 0, accepted
 * messages 1, malformed/unsupported requests -1. No user text is logged. */
int x4_text_input_receive(X4TextInput *input, const char *data, size_t length);
/* Main thread: copies the next request or withdrawal. A request is delivered
 * once; the matching id must be completed or cancelled before the next one. */
X4TextEvent x4_text_input_poll(X4TextInput *input, X4TextRequest *out);
/* Exactly one terminal reply per active id. Rejects stale/withdrawn ids and
 * invalid UTF-8 or text longer than the negotiated local bound. */
int x4_text_input_complete(X4TextInput *input, const char *id, const char *text);
int x4_text_input_cancel(X4TextInput *input, const char *id);
/* Queue ReceiverCancel for every remaining transaction on close/error. */
void x4_text_input_cancel_all(X4TextInput *input);
/* Session boundary after callbacks are stopped: discard old wire frames and
 * queued text; deliver a withdrawal for an already shown widget. */
void x4_text_input_reset(X4TextInput *input);
/* RTC worker sends returned frame on its message channel. Returns 1 for a
 * frame, 0 when empty, -1 if the supplied buffer is too small. */
int x4_text_input_take_outbound(X4TextInput *input, char *out, size_t capacity);

#ifdef __cplusplus
}
#endif
