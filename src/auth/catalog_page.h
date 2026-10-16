/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "json.h"
#include "http_client.h"
#include "xbox_catalog.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define X4_CATALOG_PAGES_MAX 50u
#define X4_CATALOG_TOKEN_MAX 256u
#define X4_CATALOG_GET_ATTEMPTS 2u
#define X4_CATALOG_GET_CANCELLED 1

enum { X4_PAGE_DONE = 0, X4_PAGE_MORE = 1, X4_PAGE_INVALID = -1,
    X4_PAGE_FULL = -2, X4_PAGE_CANCELLED = -3 };

typedef int (*X4CatalogGet)(void *context, const char *url, size_t *length, int *status);
typedef int (*X4CatalogReady)(void *context);

/* Only an idempotent regional-title GET with no HTTP status can be retried.
 * The same URL/token is passed again, and the page is parsed only on success.
 * Certificate, protocol, HTTP response and cancellation failures are final. */
static inline int x4_catalog_page_get(void *context, const char *url, X4CatalogGet get,
    X4CatalogReady ready, const _Atomic int *cancel, size_t *length, int *status)
{
    int rc = X4_HTTP_ARGUMENT;
    for (unsigned attempt = 0; attempt < X4_CATALOG_GET_ATTEMPTS; ++attempt) {
        if (cancel && atomic_load(cancel)) return X4_CATALOG_GET_CANCELLED;
        *length = 0;
        *status = 0;
        rc = get(context, url, length, status);
        if (cancel && atomic_load(cancel)) return X4_CATALOG_GET_CANCELLED;
        if (rc == 0) return 0;
        if (attempt + 1 == X4_CATALOG_GET_ATTEMPTS ||
            (rc != X4_HTTP_NATIVE_TIMEOUT && rc != X4_HTTP_DEADLINE) ||
            *status != 0 || !ready(context)) return rc;
    }
    return rc;
}

/* A page result is complete only when the service omits continuationToken or
 * provides null/empty. Every other value must be followed or rejected. */

static inline bool x4_catalog_query_valid(const char *q)
{
    static const char base[] = "/v2/titles?mr=200";
    static const char with_token[] = "/v2/titles?mr=200&ct=";
    if (!strcmp(q, base)) return true;
    if (strncmp(q, with_token, sizeof(with_token) - 1)) return false;
    const char *ct = q + sizeof(with_token) - 1;
    if (!*ct) return false;
    for (; *ct; ++ct) {
        unsigned char c = (unsigned char)*ct;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') continue;
        if (c == '%' && ct[1] && ct[2] &&
            ((ct[1] >= '0' && ct[1] <= '9') || (ct[1] >= 'A' && ct[1] <= 'F')) &&
            ((ct[2] >= '0' && ct[2] <= '9') || (ct[2] >= 'A' && ct[2] <= 'F'))) {
            unsigned hi = ct[1] <= '9' ? (unsigned)(ct[1] - '0') : (unsigned)(ct[1] - 'A' + 10);
            unsigned lo = ct[2] <= '9' ? (unsigned)(ct[2] - '0') : (unsigned)(ct[2] - 'A' + 10);
            if ((hi << 4 | lo) < 0x20 || (hi << 4 | lo) > 0x7e) return false;
            ct += 2;
            continue;
        }
        return false;
    }
    return true;
}

static inline int x4_catalog_page_url(const char *origin, const char *token, char *url, size_t cap)
{
    int n = snprintf(url, cap, "%s/v2/titles?mr=200", origin);
    if (n < 0 || (size_t)n >= cap) return -1;
    size_t used = (size_t)n;
    if (!token || !*token) return 0;
    if (used + 4 >= cap) return -1;
    memcpy(url + used, "&ct=", 4); used += 4;
    static const char hex[] = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *)token; *p; ++p) {
        unsigned char c = *p;
        bool simple = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        size_t need = simple ? 1 : 3;
        if (used + need >= cap) { url[0] = 0; return -1; }
        if (simple) url[used++] = (char)c;
        else { url[used++] = '%'; url[used++] = hex[c >> 4]; url[used++] = hex[c & 15]; }
    }
    url[used] = 0;
    return 0;
}

static inline int x4_catalog_page_advance(char *current, size_t cap, const char *next, unsigned pages)
{
    if (!next[0]) return X4_PAGE_DONE;
    if (pages >= X4_CATALOG_PAGES_MAX || !strcmp(current, next) || strlen(next) >= cap)
        return X4_PAGE_INVALID;
    memcpy(current, next, strlen(next) + 1);
    return X4_PAGE_MORE;
}

typedef bool (*X4CatalogEntryParser)(X4JsonSpan, X4CatalogTitle *);

static inline int x4_catalog_page_append(const char *json, size_t length, X4CatalogSnapshot *catalog,
    char *next, size_t next_cap, X4CatalogEntryParser parse, const _Atomic int *cancel)
{
    X4JsonSpan root, results, item, continuation;
    if (!json || !catalog || !next || !next_cap || !parse) return X4_PAGE_INVALID;
    next[0] = 0;
    if (x4_json_parse(json, length, &root) ||
        x4_json_member(root, "results", &results) != 1 || x4_json_type(results) != X4_JSON_T_ARRAY)
        return X4_PAGE_INVALID;
    int member = x4_json_member(root, "continuationToken", &continuation);
    if (member < 0) return X4_PAGE_INVALID;
    if (member == 1 && x4_json_type(continuation) != X4_JSON_T_NULL &&
        (x4_json_type(continuation) != X4_JSON_T_STRING ||
         x4_json_token(continuation, next, next_cap) < 0)) return X4_PAGE_INVALID;
    size_t cursor = 0;
    int more;
    unsigned received = 0;
    X4CatalogTitle title;
    while ((more = x4_json_item(results, &cursor, &item)) == 1) {
        if (cancel && atomic_load(cancel)) return X4_PAGE_CANCELLED;
        if (!parse(item, &title)) continue;
        if (catalog->total == UINT32_MAX) return X4_PAGE_FULL;
        ++catalog->total;
        ++received;
        X4CatalogTitle *same = NULL;
        for (unsigned i = 0; i < catalog->count && !same; ++i)
            if (!strcmp(catalog->titles[i].id, title.id)) same = &catalog->titles[i];
        if (same) {
            if (title.entitled) same->entitled = 1;
            if (!same->product_id[0])
                memcpy(same->product_id, title.product_id, sizeof(same->product_id));
        } else {
            if (catalog->count == X4_CATALOG_MAX) return X4_PAGE_FULL;
            catalog->titles[catalog->count++] = title;
        }
    }
    if (more < 0 || (next[0] && received == 0)) return X4_PAGE_INVALID;
    return next[0] ? X4_PAGE_MORE : X4_PAGE_DONE;
}
