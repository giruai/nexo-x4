/* SPDX-License-Identifier: GPL-3.0-only */
#include "queue_estimate.h"
#include "json.h"
#include <stdio.h>
#include <string.h>

#define X4_QUEUE_URL_MAX 512u
static const char scheme[] = "https://";
static const char suffix[] = ".gssv-play-prod.xboxlive.com";
static const char route[] = "/v1/waittime/";

static bool alnum(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9');
}

bool x4_queue_waittime_url_valid(const char *url)
{
    if (!url) return false;
    size_t n = 0;
    while (n <= X4_QUEUE_URL_MAX && url[n]) ++n;
    if (n > X4_QUEUE_URL_MAX || strncmp(url, scheme, sizeof(scheme) - 1)) return false;
    const char *host = url + sizeof(scheme) - 1;
    const char *slash = strchr(host, '/');
    if (!slash) return false;
    size_t h = (size_t)(slash - host), s = sizeof(suffix) - 1;
    if (h <= s || h > 253 || memcmp(host + h - s, suffix, s)) return false;
    size_t labels = 0, label = 0, end = h - s;
    for (size_t i = 0; i <= end; ++i) {
        unsigned char c = i < end ? (unsigned char)host[i] : '.';
        if (c == '.') {
            if (!label || label > 63 || host[i - 1] == '-' || host[i - label] == '-' ||
                ++labels > 4) return false;
            label = 0;
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') ++label;
        else return false;
    }
    if (strncmp(slash, route, sizeof(route) - 1)) return false;
    const char *id = slash + sizeof(route) - 1;
    size_t length = 0;
    for (; id[length]; ++length) {
        unsigned char c = (unsigned char)id[length];
        if (!alnum(c) && c != '-' && c != '_' && c != '.') return false;
        if (length >= 95) return false;
    }
    return length != 0 && !(length == 1 && id[0] == '.') &&
        !(length == 2 && id[0] == '.' && id[1] == '.');
}

bool x4_queue_waittime_url(const char *origin, const char *title_id,
    char *url, size_t capacity)
{
    if (!url || !capacity) return false;
    url[0] = 0;
    if (!origin || !title_id) return false;
    int n = snprintf(url, capacity, "%s%s%s", origin, route, title_id);
    if (n < 0 || (size_t)n >= capacity || !x4_queue_waittime_url_valid(url)) {
        url[0] = 0;
        return false;
    }
    return true;
}

void x4_queue_estimate_parse(const char *body, size_t length, int status,
    X4QueueEstimate *out)
{
    if (!out) return;
    /* A failed refresh changes availability but retains the last valid
     * values and their original timestamp so the UI can label them stale. */
    out->result = X4_QUEUE_ERROR;
    out->http_status = status;
    if (status == 404 || status == 429) { out->result = X4_QUEUE_UNAVAILABLE; return; }
    if (status != 200 || !body || !length || length > 8192) return;
    X4JsonSpan root;
    if (x4_json_parse(body, length, &root) != 0 || x4_json_type(root) != X4_JSON_T_OBJECT) return;
    static const char *const names[] = {
        "estimatedAllocationTimeInSeconds", "estimatedProvisioningTimeInSeconds",
        "estimatedTotalWaitTimeInSeconds"
    };
    uint32_t values[3] = {0};
    unsigned mask = 0;
    for (unsigned i = 0; i < 3; ++i) {
        X4JsonSpan value;
        int found = x4_json_member(root, names[i], &value);
        if (found < 0) return;
        if (!found || x4_json_type(value) == X4_JSON_T_NULL) continue;
        if (x4_json_type(value) != X4_JSON_T_NUMBER ||
            x4_json_uint32(value, &values[i]) != 0 || values[i] > X4_QUEUE_SECONDS_MAX) return;
        mask |= 1u << i;
    }
    out->present_mask = mask;
    out->allocation_seconds = values[0];
    out->provisioning_seconds = values[1];
    out->total_seconds = values[2];
    out->result = mask ? X4_QUEUE_AVAILABLE : X4_QUEUE_UNAVAILABLE;
}

bool x4_queue_refresh_due(bool waiting_for_resources, uint64_t now_us,
    uint64_t next_query_us)
{
    return waiting_for_resources && now_us >= next_query_us;
}
