/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stddef.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Local errors; native Net/Ssl/Http errors are returned unchanged. */
enum {
    X4_HTTP_CANCELLED = -1001,
    X4_HTTP_BOUND = -1002,
    X4_HTTP_URL = -1003,
    X4_HTTP_DEADLINE = -1004,
    X4_HTTP_ALLOCATION = -1005,
    X4_HTTP_ARGUMENT = -1006,
    X4_HTTP_UNUSABLE = -1007,
    X4_HTTP_PROTOCOL = -1008,
    X4_HTTP_BUSY = -1009,
};
/* Observed native sceHttpSendRequest timeout on this PS4. The same numeric
 * code is named SCE_HTTP_ERROR_TIMEOUT in Sony's Vita HTTP headers. */
#define X4_HTTP_NATIVE_TIMEOUT ((int)0x80431068u)
#define X4_HTTP_PHASE_TIMEOUT_USEC (15u * 1000 * 1000)
#define X4_HTTP_TITLES_PHASE_TIMEOUT_USEC (15u * 1000 * 1000)
static inline uint32_t x4_http_phase_timeout(int regional_titles)
{
    return regional_titles ? X4_HTTP_TITLES_PHASE_TIMEOUT_USEC : X4_HTTP_PHASE_TIMEOUT_USEC;
}

typedef struct X4Http X4Http;

/* Public Game Pass text only; market and verified catalog permissions stay MX.
 * Fixed locale choices keep caller input out of public URLs. */
static inline const char *x4_http_public_locale(int language)
{
    static const char *const locales[6] = {
        "en-US", "pt-BR", "es-MX", "fr-FR", "de-DE", "it-IT"
    };
    return language >= 0 && language < 6 ? locales[language] : NULL;
}

enum X4GamepassSigl {
    X4_SIGL_RECENT, X4_SIGL_POPULAR, X4_SIGL_FREE_TO_PLAY, X4_SIGL_LEAVING,
    X4_SIGL_COUNT
};
static inline int x4_http_public_products_url(int language, int detailed, char *out, size_t capacity)
{
    const char *locale = x4_http_public_locale(language);
    if (!locale || !out || !capacity || (detailed != 0 && detailed != 1)) return -1;
    int n = snprintf(out, capacity,
        "https://catalog.gamepass.com/v3/products?market=MX&language=%s&hydration=%s",
        locale, detailed ? "RemoteHighSapphire0" : "RemoteLowJade0");
    return n >= 0 && (size_t)n < capacity ? 0 : -1;
}
static inline int x4_http_public_sigl_url(int language, enum X4GamepassSigl list,
    char *out, size_t capacity)
{
    static const char *const ids[X4_SIGL_COUNT] = {
        "06323672-b8c8-43cc-b0de-32d5a9834749",
        "6a589fa0-d493-472b-8e20-3813699d7056",
        "d8f4afcd-882a-49e3-86b3-f61fa0172b75",
        "31ff2361-2772-4622-849b-f4f1abb4ad1b"
    };
    const char *locale = x4_http_public_locale(language);
    if (!locale || !out || !capacity || (unsigned)list >= X4_SIGL_COUNT) return -1;
    int n = snprintf(out, capacity,
        "https://catalog.gamepass.com/sigls/v2?id=%s&market=MX&language=%s",
        ids[list], locale);
    return n >= 0 && (size_t)n < capacity ? 0 : -1;
}

/* Every call, including open and close, belongs to one worker thread. Close
 * only after that worker has finished; it never runs a global NetTerm. */
X4Http *x4_http_open(int *error);
void x4_http_close(X4Http *http);
/* Worker-thread only; refused while a request is active. Default is es-MX. */
int x4_http_set_public_language(X4Http *http, int language);

/* Encoded forms are visible ASCII of at most this many bytes; a refresh token
 * of 16 KiB still fits once percent-encoded. */
#define X4_HTTP_FORM_MAX (64u * 1024)

/* HTTPS only to https://login.microsoftonline.com/consumers/<path>. GET when
 * form is NULL, otherwise POST application/x-www-form-urlencoded. Returns 0
 * once any HTTP status and its whole body were read (body NUL terminated,
 * at most capacity - 1 bytes). On error body is wiped, length and status 0.
 * cancel may be NULL; it is polled between stages and reads, so cancelling
 * waits for the current native operation under its configured timeouts. */
int x4_http_request(X4Http *http, const char *url, const char *form, char *body,
    size_t capacity, size_t *length, int *status, const _Atomic int *cancel);

#define X4_HTTP_PASSPORT_BODY_MAX (64u * 1024)

/* POST application/x-www-form-urlencoded to exactly
 * https://login.live.com/oauth20_token.srf; the destination is fixed here and
 * never chosen by the caller. form is non-empty visible ASCII of at most
 * X4_HTTP_FORM_MAX bytes; capacity is at most X4_HTTP_PASSPORT_BODY_MAX + 1.
 * Same TLS, timeout, redirect, cookie, quarantine and result rules as
 * x4_http_request. */
int x4_http_passport_request(X4Http *http, const char *form, char *body, size_t capacity,
    size_t *length, int *status, const _Atomic int *cancel);

#define X4_HTTP_JSON_BODY_MAX (64u * 1024)
#define X4_HTTP_RESPONSE_MAX (2u * 1024 * 1024)
#define X4_HTTP_HEADERS_MAX 4u
#define X4_HTTP_HEADER_VALUE_MAX 16384u

typedef struct { const char *name, *value; } X4HttpHeader;

/* Xbox Live and Microsoft Store exchanges, under the same TLS, timeout,
 * redirect, cookie and quarantine rules as x4_http_request. Only these
 * destinations are accepted:
 *   POST JSON  https://user.auth.xboxlive.com/user/authenticate
 *   POST JSON  https://xsts.auth.xboxlive.com/xsts/authorize
 *   POST JSON  https://xgpuweb[f2p].gssv-play-prod.xboxlive.com/v2/login/user
 *   GET        https://<labels>.gssv-play-prod.xboxlive.com/v2/titles
 *              or /v2/titles?mr=200[&ct=<nonempty percent-encoded token>]
 *   GET        https://displaycatalog.mp.microsoft.com/v7.0/products?bigIds=
 *              <1..8 alphanumeric IDs>&market=MX&languages=es-MX&fieldsTemplate=Details
 * json selects POST (printable ASCII, at most X4_HTTP_JSON_BODY_MAX bytes);
 * NULL selects GET. Each destination has its own short header allowlist;
 * Authorization is accepted only as "Bearer <token68>" to a /v2/titles
 * region. capacity is at most X4_HTTP_RESPONSE_MAX + 1. */
int x4_http_json_request(X4Http *http, const char *url, const X4HttpHeader *headers,
    size_t header_count, const char *json, char *body, size_t capacity, size_t *length,
    int *status, const _Atomic int *cancel);

/* Public PSBox artwork/metadata only. No caller headers or OAuth credentials.
 * Products: fixed catalog.gamepass.com v3 route, light or detailed hydration,
 * generated printable JSON body. Artwork: store-images.s-microsoft.com/image/
 * with bounded size query. Same TLS checks, cancellation and 30 s deadline. */
int x4_http_gamepass_products(X4Http *http, int detailed, const char *json, char *body,
    size_t capacity, size_t *length, int *status, const _Atomic int *cancel);
int x4_http_store_image(X4Http *http, const char *url, char *body, size_t capacity,
    size_t *length, int *status, const _Atomic int *cancel);
/* Fixed public SIGL lists from PSBox. No caller URL, headers or credentials. */
int x4_http_gamepass_sigl(X4Http *http, enum X4GamepassSigl list, char *body,
    size_t capacity, size_t *length, int *status, const _Atomic int *cancel);

enum X4HttpSessionMethod { X4_SESSION_HTTP_GET, X4_SESSION_HTTP_POST, X4_SESSION_HTTP_DELETE };

/* Xbox cloud session exchanges on the validated default region host
 * (https://<labels>.gssv-play-prod.xboxlive.com, same label rules as
 * /v2/titles), under the same TLS, timeout, redirect, cookie and quarantine
 * rules. Exactly these method/path pairs are accepted:
 *   POST JSON  /v5/sessions/cloud/play            (non-empty JSON body)
 *   GET        /v5/sessions/cloud/<id>/state      (json NULL)
 *   POST       /v5/sessions/cloud/<id>/keepalive  (json "", empty body)
 *   POST JSON  /v5/sessions/cloud/<id>/connect    (non-empty JSON body)
 *   GET        /v5/sessions/cloud/<id>/sdp        (json NULL)
 *   POST JSON  /v5/sessions/cloud/<id>/sdp        (non-empty JSON body)
 *   GET        /v5/sessions/cloud/<id>/ice        (json NULL)
 *   POST JSON  /v5/sessions/cloud/<id>/ice        (non-empty JSON body)
 *   DELETE     /v5/sessions/cloud/<id>            (json NULL)
 * <id> is 1..128 of [A-Za-z0-9_-] (GUIDs included) and never "play" or
 * "active". No query, fragment, escape, dot segment, port or userinfo.
 * Exactly three headers are required, once each: Authorization
 * ("Bearer <token68>"), x-gssv-client and X-MS-Device-Info. Header values
 * and bodies stay private. capacity is at most X4_HTTP_RESPONSE_MAX + 1. */
int x4_http_session_request(X4Http *http, enum X4HttpSessionMethod method, const char *url,
    const X4HttpHeader *headers, size_t header_count, const char *json, char *body, size_t capacity,
    size_t *length, int *status, const _Atomic int *cancel);

/* Optional regional GET /v1/waittime/<validated title ID>. Exactly the
 * session's three bearer/client/device headers; separate X4Http context is
 * recommended so a quarantined estimate cannot disable session polling.
 * Total request deadline is 3 s; each native I/O phase is capped to the
 * remaining budget when configured. Response capacity is at most 8193. */
int x4_http_waittime_request(X4Http *http, const char *url,
    const X4HttpHeader *headers, size_t header_count, char *body,
    size_t capacity, size_t *length, int *status, const _Atomic int *cancel);

/* Constant literal naming the last stage reached; safe to show in the UI. */
const char *x4_http_stage(const X4Http *http);
/* Read on the owning worker after a request returns. False when cleanup left
 * native objects quarantined, so retrying on this context would be unsafe. */
int x4_http_can_retry(const X4Http *http);

void x4_secure_clear(void *data, size_t size);
#ifdef __cplusplus
}
#endif
