/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum X4QueueEstimateResult {
    X4_QUEUE_NONE, X4_QUEUE_AVAILABLE, X4_QUEUE_UNAVAILABLE, X4_QUEUE_ERROR
};
enum {
    X4_QUEUE_ALLOCATION = 1u << 0,
    X4_QUEUE_PROVISIONING = 1u << 1,
    X4_QUEUE_TOTAL = 1u << 2,
    X4_QUEUE_SECONDS_MAX = 86400u
};

/* These are title/region estimates, not an individual queue position or a
 * guaranteed remaining wait. A clear bit means absent, never zero. */
typedef struct {
    enum X4QueueEstimateResult result;
    unsigned present_mask;
    uint32_t allocation_seconds, provisioning_seconds, total_seconds;
    int http_status;
    /* Time of last valid nonempty estimate relative to session start. This
     * does not advance on 429, transport error, or malformed response. */
    unsigned updated_elapsed_seconds, query_count;
} X4QueueEstimate;

/* Exact HTTPS regional route; no query, fragment, userinfo or credentials. */
bool x4_queue_waittime_url_valid(const char *url);
bool x4_queue_waittime_url(const char *origin, const char *title_id,
    char *url, size_t capacity);
/* HTTP status and body are untrusted. On invalid or absent fields, mask stays
 * clear. Result is independent of the actual cloud session state. */
void x4_queue_estimate_parse(const char *body, size_t length, int http_status,
    X4QueueEstimate *out);
bool x4_queue_refresh_due(bool waiting_for_resources, uint64_t now_us,
    uint64_t next_query_us);
