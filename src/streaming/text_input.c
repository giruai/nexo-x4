/* SPDX-License-Identifier: GPL-3.0-only */
#include "text_input.h"
#include "../auth/json.h"
#include <stdatomic.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define PENDING_MAX 2u
#define OUTBOUND_MAX 8u
typedef struct { char bytes[X4_TEXT_WIRE_MAX]; } Frame;
struct X4TextInput {
    atomic_uint references;
    atomic_flag gate;
    X4TextRequest active, pending[PENDING_MAX];
    unsigned pending_count;
    int active_present, delivered, withdrawn;
    Frame outbound[OUTBOUND_MAX];
    unsigned out_head, out_count;
};
typedef struct { char *data; size_t capacity, used; int bad; } Writer;

static void lock(X4TextInput *s)
{ while (atomic_flag_test_and_set_explicit(&s->gate, memory_order_acquire)) {} }
static void unlock(X4TextInput *s)
{ atomic_flag_clear_explicit(&s->gate, memory_order_release); }
static void wipe(void *p, size_t n)
{ volatile unsigned char *v = p; while (n--) *v++ = 0; }

static int utf8_char(const unsigned char *p, size_t left, uint32_t *code, unsigned *size)
{
    if (!left) return -1;
    unsigned c = p[0], n = c < 0x80 ? 1u : c >= 0xc2 && c <= 0xdf ? 2u :
        c >= 0xe0 && c <= 0xef ? 3u : c >= 0xf0 && c <= 0xf4 ? 4u : 0u;
    if (!n || n > left) return -1;
    uint32_t v = c & (n == 1 ? 0x7f : n == 2 ? 0x1f : n == 3 ? 0x0f : 0x07);
    for (unsigned i = 1; i < n; ++i) {
        if ((p[i] & 0xc0) != 0x80) return -1;
        v = (v << 6) | (p[i] & 0x3f);
    }
    if ((n == 2 && v < 0x80) || (n == 3 && v < 0x800) ||
        (n == 4 && v < 0x10000) || v > 0x10ffff || (v >= 0xd800 && v <= 0xdfff)) return -1;
    *code = v; *size = n; return 0;
}
static size_t bounded_length(const char *s, size_t cap)
{ size_t n = 0; while (n < cap && s[n]) ++n; return n; }
static int text_length(const char *s, size_t cap, unsigned *chars)
{
    size_t len = bounded_length(s, cap);
    if (len == cap) return -1;
    unsigned count = 0;
    for (size_t i = 0; i < len;) {
        uint32_t code; unsigned n;
        if (utf8_char((const unsigned char *)s + i, len - i, &code, &n) ||
            (code < 0x20 && code != '\n' && code != '\t') || code == 0x7f) return -1;
        i += n; ++count;
    }
    *chars = count;
    return 0;
}
static int hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static int unicode_escape(const char **at, const char *end, uint32_t *code)
{
    if (end - *at < 4) return -1;
    uint32_t v = 0;
    for (int i = 0; i < 4; ++i) {
        int h = hex((*at)[i]); if (h < 0) return -1; v = (v << 4) | (unsigned)h;
    }
    *at += 4;
    if (v >= 0xd800 && v <= 0xdbff) {
        if (end - *at < 6 || (*at)[0] != '\\' || (*at)[1] != 'u') return -1;
        *at += 2;
        uint32_t lo = 0;
        for (int i = 0; i < 4; ++i) {
            int h = hex((*at)[i]); if (h < 0) return -1; lo = (lo << 4) | (unsigned)h;
        }
        *at += 4;
        if (lo < 0xdc00 || lo > 0xdfff) return -1;
        v = 0x10000 + ((v - 0xd800) << 10) + (lo - 0xdc00);
    } else if (v >= 0xdc00 && v <= 0xdfff) return -1;
    *code = v; return 0;
}
static int append_codepoint(char *out, size_t capacity, size_t *used, uint32_t v)
{
    unsigned n = v < 0x80 ? 1u : v < 0x800 ? 2u : v < 0x10000 ? 3u : 4u;
    if (*used + n >= capacity) return -1;
    if (n == 1) out[(*used)++] = (char)v;
    else {
        for (unsigned i = n; i > 1; --i) out[*used + i - 1] = (char)(0x80 | (v & 0x3f)), v >>= 6;
        out[*used] = (char)((n == 2 ? 0xc0 : n == 3 ? 0xe0 : 0xf0) | v);
        *used += n;
    }
    out[*used] = 0; return 0;
}
/* x4_json_parse has already checked syntax. Decode without transliterating or
 * silently truncating text; that would alter what the game receives. */
static int decoded(X4JsonSpan value, char *out, size_t capacity)
{
    if (!out || !capacity || x4_json_type(value) != X4_JSON_T_STRING) return -1;
    out[0] = 0;
    const char *p = value.data + 1, *end = value.data + value.length - 1;
    size_t used = 0;
    while (p < end) {
        uint32_t code; unsigned n = 1;
        if (*p == '\\') {
            ++p; if (p == end) return -1;
            switch (*p++) {
            case '"': code = '"'; break; case '\\': code = '\\'; break;
            case '/': code = '/'; break; case 'b': code = '\b'; break;
            case 'f': code = '\f'; break; case 'n': code = '\n'; break;
            case 'r': code = '\r'; break; case 't': code = '\t'; break;
            case 'u': if (unicode_escape(&p, end, &code)) return -1; break;
            default: return -1;
            }
        } else {
            if (utf8_char((const unsigned char *)p, (size_t)(end - p), &code, &n)) return -1;
            p += n;
        }
        if (!code || append_codepoint(out, capacity, &used, code)) return -1;
    }
    return 0;
}
static void put(Writer *w, const char *s)
{
    if (w->bad) return;
    size_t n = strlen(s);
    if (n >= w->capacity - w->used) { w->bad = 1; return; }
    memcpy(w->data + w->used, s, n + 1); w->used += n;
}
static void quote(Writer *w, const char *s)
{
    put(w, "\"");
    for (const unsigned char *p = (const unsigned char *)s; *p && !w->bad; ++p) {
        char one[2] = {(char)*p, 0};
        switch (*p) {
        case '"': put(w, "\\\""); break; case '\\': put(w, "\\\\"); break;
        case '\n': put(w, "\\n"); break; case '\r': put(w, "\\r"); break;
        case '\t': put(w, "\\t"); break;
        default:
            if (*p < 0x20) { w->bad = 1; break; }
            put(w, one);
        }
    }
    put(w, "\"");
}
static int frame(char *out, size_t capacity, const char *id, const char *text)
{
    Writer w = {out, capacity, 0, 0};
    if (!capacity) return -1;
    out[0] = 0;
    if (text) {
        char inner[X4_TEXT_WIRE_MAX / 2];
        Writer i = {inner, sizeof(inner), 0, 0};
        inner[0] = 0;
        put(&i, "{\"Text\":"); quote(&i, text); put(&i, "}");
        if (i.bad) { wipe(inner, sizeof(inner)); return -1; }
        put(&w, "{\"type\":\"TransactionComplete\",\"content\":"); quote(&w, inner);
        wipe(inner, sizeof(inner));
    } else put(&w, "{\"type\":\"ReceiverCancel\"");
    put(&w, ",\"id\":"); quote(&w, id); put(&w, ",\"cv\":\"\"}");
    return w.bad ? -1 : 0;
}
static int queue_frame(X4TextInput *s, const char *id, const char *text)
{
    if (s->out_count == OUTBOUND_MAX) return -1;
    unsigned tail = (s->out_head + s->out_count) % OUTBOUND_MAX;
    if (frame(s->outbound[tail].bytes, sizeof(s->outbound[tail].bytes), id, text)) return -1;
    ++s->out_count; return 0;
}
static int reject_request(X4TextInput *s, const char *id)
{
    lock(s);
    int rc = queue_frame(s, id, NULL);
    unlock(s);
    return rc ? -1 : 1;
}
static int valid_id(const char *id)
{
    size_t n = bounded_length(id, 81);
    if (!n || n >= 81) return 0;
    unsigned chars;
    if (text_length(id, 81, &chars)) return 0;
    for (size_t i = 0; i < n; ++i)
        if ((unsigned char)id[i] < 0x20 || (unsigned char)id[i] == 0x7f) return 0;
    return 1;
}
static int member_text(X4JsonSpan object, const char *name, char *out, size_t cap)
{
    X4JsonSpan v;
    int f = x4_json_member(object, name, &v);
    if (f < 0) return -1;
    if (!f || x4_json_type(v) == X4_JSON_T_NULL) { out[0] = 0; return 0; }
    return decoded(v, out, cap);
}
static int member_uint(X4JsonSpan object, const char *name, unsigned *out)
{
    X4JsonSpan v;
    int f = x4_json_member(object, name, &v);
    if (f < 0) return -1;
    if (!f || x4_json_type(v) == X4_JSON_T_NULL) { *out = 0; return 0; }
    uint32_t n;
    if (x4_json_uint32(v, &n)) return -1;
    *out = n; return 0;
}
static void advance(X4TextInput *s)
{
    wipe(&s->active, sizeof(s->active)); s->active_present = s->delivered = s->withdrawn = 0;
    if (s->pending_count) {
        s->active = s->pending[0]; s->active_present = 1;
        for (unsigned i = 1; i < s->pending_count; ++i) s->pending[i - 1] = s->pending[i];
        wipe(&s->pending[--s->pending_count], sizeof(s->pending[0]));
    }
}
X4TextInput *x4_text_input_create(void)
{
    X4TextInput *s = calloc(1, sizeof(*s));
    if (s) { atomic_init(&s->references, 1u); atomic_flag_clear(&s->gate); }
    return s;
}
X4TextInput *x4_text_input_retain(X4TextInput *s)
{
    if (!s) return NULL;
    unsigned current = atomic_load_explicit(&s->references, memory_order_relaxed);
    while (current && current != UINT_MAX) {
        if (atomic_compare_exchange_weak_explicit(&s->references, &current, current + 1,
                memory_order_relaxed, memory_order_relaxed)) return s;
    }
    return NULL;
}
void x4_text_input_destroy(X4TextInput *s)
{
    if (!s) return;
    /* Last owner must have stopped all its callbacks before this release. */
    if (atomic_fetch_sub_explicit(&s->references, 1u, memory_order_acq_rel) != 1u) return;
    wipe(s, sizeof(*s)); free(s);
}
static int receive_request(X4TextInput *s, X4JsonSpan root, const char *id)
{
    X4TextRequest req = {0};
    char content[X4_TEXT_WIRE_MAX];
    int result;
    content[0] = 0;
    memcpy(req.id, id, strlen(id) + 1);
    X4JsonSpan v, body;
    if (x4_json_member(root, "content", &v) != 1 ||
        decoded(v, content, sizeof(content)) ||
        x4_json_parse(content, strlen(content), &body) ||
        x4_json_type(body) != X4_JSON_T_OBJECT ||
        member_uint(body, "InputScope", &req.input_scope)) goto reject;
    /* Do not copy a password's DefaultText into the UI/request queue. */
    if (req.input_scope == 31 ||
        member_text(body, "TitleText", req.title, sizeof(req.title)) ||
        member_text(body, "DescriptionText", req.description, sizeof(req.description)) ||
        member_text(body, "DefaultText", req.default_text, sizeof(req.default_text)) ||
        member_uint(body, "MaxLength", &req.max_chars)) goto reject;
    if (!req.max_chars || req.max_chars > X4_TEXT_MAX_CHARS) req.max_chars = X4_TEXT_MAX_CHARS;
    unsigned chars;
    if (text_length(req.default_text, sizeof(req.default_text), &chars)) goto reject;

    lock(s);
    if (s->active_present && !strcmp(s->active.id, req.id)) { result = 1; goto unlock; }
    if (chars > req.max_chars) {
        result = queue_frame(s, req.id, NULL) ? -1 : 1;
        goto unlock;
    }
    for (unsigned i = 0; i < s->pending_count; ++i)
        if (!strcmp(s->pending[i].id, req.id)) { result = 1; goto unlock; }
    if (!s->active_present) { s->active = req; s->active_present = 1; }
    else if (s->pending_count < PENDING_MAX) s->pending[s->pending_count++] = req;
    else { result = queue_frame(s, req.id, NULL) ? -1 : 1; goto unlock; }
    result = 1;
unlock:
    unlock(s);
    goto done;
reject:
    result = reject_request(s, id);
done:
    wipe(content, sizeof(content));
    wipe(&req, sizeof(req));
    return result;
}
int x4_text_input_receive(X4TextInput *s, const char *data, size_t length)
{
    if (!s || !data || !length || length > X4_TEXT_WIRE_MAX) return -1;
    X4JsonSpan root;
    if (x4_json_parse(data, length, &root) || x4_json_type(root) != X4_JSON_T_OBJECT) return -1;
    char type[33], id[81];
    if (member_text(root, "type", type, sizeof(type))) return -1;
    if (strcmp(type, "Message") && strcmp(type, "TransactionStart") && strcmp(type, "SenderCancel")) return 0;
    if (strcmp(type, "SenderCancel")) {
        char target[97];
        if (member_text(root, "target", target, sizeof(target))) return -1;
        if (strcmp(target, "/streaming/systemUi/messages/ShowVirtualKeyboard")) return 0;
    }
    if (member_text(root, "id", id, sizeof(id)) || !valid_id(id)) return -1;
    if (!strcmp(type, "SenderCancel")) {
        lock(s);
        int found = 0;
        if (s->active_present && !strcmp(s->active.id, id)) {
            found = 1;
            if (s->delivered) s->withdrawn = 1;
            else advance(s);
        } else for (unsigned i = 0; i < s->pending_count; ++i) {
            if (strcmp(s->pending[i].id, id)) continue;
            found = 1;
            wipe(&s->pending[i], sizeof(s->pending[i]));
            for (unsigned j = i + 1; j < s->pending_count; ++j) s->pending[j - 1] = s->pending[j];
            wipe(&s->pending[--s->pending_count], sizeof(s->pending[0]));
            break;
        }
        unlock(s); return found;
    }
    return receive_request(s, root, id);
}
X4TextEvent x4_text_input_poll(X4TextInput *s, X4TextRequest *out)
{
    if (!s || !out) return X4_TEXT_EVENT_NONE;
    lock(s);
    X4TextEvent event = X4_TEXT_EVENT_NONE;
    if (s->active_present && s->withdrawn) {
        *out = s->active; event = X4_TEXT_EVENT_WITHDRAWN; advance(s);
    } else if (s->active_present && !s->delivered) {
        *out = s->active; s->delivered = 1; event = X4_TEXT_EVENT_REQUEST;
    }
    unlock(s); return event;
}
int x4_text_input_complete(X4TextInput *s, const char *id, const char *text)
{
    if (!s || !id || !text) return -1;
    unsigned chars;
    if (text_length(text, X4_TEXT_MAX_BYTES + 1, &chars)) return -1;
    lock(s);
    int rc = -1;
    if (s->active_present && s->delivered && !s->withdrawn && !strcmp(s->active.id, id) &&
        chars <= s->active.max_chars && !queue_frame(s, id, text)) { advance(s); rc = 0; }
    unlock(s); return rc;
}
int x4_text_input_cancel(X4TextInput *s, const char *id)
{
    if (!s || !id) return -1;
    lock(s);
    int rc = -1;
    if (s->active_present && s->delivered && !s->withdrawn && !strcmp(s->active.id, id) &&
        !queue_frame(s, id, NULL)) { advance(s); rc = 0; }
    unlock(s); return rc;
}
void x4_text_input_cancel_all(X4TextInput *s)
{
    if (!s) return;
    lock(s);
    if (s->active_present && !s->withdrawn) queue_frame(s, s->active.id, NULL);
    for (unsigned i = 0; i < s->pending_count; ++i) queue_frame(s, s->pending[i].id, NULL);
    wipe(s->pending, sizeof(s->pending)); s->pending_count = 0;
    if (s->active_present && s->delivered) {
        char id[sizeof(s->active.id)]; memcpy(id, s->active.id, sizeof(id));
        wipe(&s->active, sizeof(s->active)); memcpy(s->active.id, id, sizeof(id));
        wipe(id, sizeof(id)); s->withdrawn = 1;
    } else {
        wipe(&s->active, sizeof(s->active));
        s->active_present = s->delivered = s->withdrawn = 0;
    }
    unlock(s);
}
void x4_text_input_reset(X4TextInput *s)
{
    if (!s) return;
    lock(s);
    char id[sizeof(s->active.id)] = {0};
    int notify = s->active_present && s->delivered;
    if (notify) memcpy(id, s->active.id, sizeof(id));
    wipe(&s->active, sizeof(s->active)); wipe(s->pending, sizeof(s->pending));
    wipe(s->outbound, sizeof(s->outbound));
    s->pending_count = s->out_head = s->out_count = 0;
    s->active_present = s->delivered = s->withdrawn = 0;
    if (notify) {
        memcpy(s->active.id, id, sizeof(id));
        s->active_present = s->delivered = s->withdrawn = 1;
    }
    wipe(id, sizeof(id));
    unlock(s);
}
int x4_text_input_take_outbound(X4TextInput *s, char *out, size_t capacity)
{
    if (!s || !out || !capacity) return -1;
    lock(s);
    if (!s->out_count) { unlock(s); return 0; }
    char *frame_bytes = s->outbound[s->out_head].bytes;
    size_t n = strlen(frame_bytes);
    if (n >= capacity) { unlock(s); return -1; }
    memcpy(out, frame_bytes, n + 1);
    wipe(frame_bytes, n + 1);
    s->out_head = (s->out_head + 1) % OUTBOUND_MAX; --s->out_count;
    unlock(s); return 1;
}
