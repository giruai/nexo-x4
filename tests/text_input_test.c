/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/streaming/text_input.h"
#include "../src/auth/json.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int send_request(X4TextInput *input, const char *id, unsigned scope)
{
    char wire[1024];
    int n = snprintf(wire, sizeof(wire),
        "{\"type\":\"TransactionStart\",\"id\":\"%s\","
        "\"target\":\"/streaming/systemUi/messages/ShowVirtualKeyboard\","
        "\"content\":\"{\\\"TitleText\\\":\\\"Search\\\",\\\"DescriptionText\\\":\\\"Enter text\\\","
        "\\\"DefaultText\\\":\\\"hi\\\",\\\"InputScope\\\":%u,\\\"MaxLength\\\":8}\"}", id, scope);
    assert(n > 0 && (size_t)n < sizeof(wire));
    return x4_text_input_receive(input, wire, (size_t)n);
}
static void request(X4TextInput *input, const char *id, unsigned scope)
{ assert(send_request(input, id, scope) == 1); }
static void expect_reply(const char *wire, const char *type, const char *id, const char *text)
{
    X4JsonSpan root, v, inner;
    assert(x4_json_parse(wire, strlen(wire), &root) == 0);
    char value[128];
    assert(x4_json_member(root, "type", &v) == 1 && x4_json_token(v, value, sizeof(value)) == 0);
    assert(!strcmp(value, type));
    assert(x4_json_member(root, "id", &v) == 1 && x4_json_token(v, value, sizeof(value)) == 0);
    assert(!strcmp(value, id));
    if (!text) { assert(x4_json_member(root, "content", &v) == 0); return; }
    char content[512];
    assert(x4_json_member(root, "content", &v) == 1 && x4_json_text(v, content, sizeof(content)) == 0);
    assert(x4_json_parse(content, strlen(content), &inner) == 0);
    assert(x4_json_member(inner, "Text", &v) == 1 && x4_json_text(v, value, sizeof(value)) == 0);
    assert(!strcmp(value, text));
}
int main(void)
{
    X4TextInput *input = x4_text_input_create();
    assert(input);
    X4TextRequest r;
    char wire[X4_TEXT_WIRE_MAX];
    assert(x4_text_input_receive(input, "{\"type\":\"HandshakeAck\"}", 23) == 0);
    request(input, "first-1", 50);
    assert(x4_text_input_poll(input, &r) == X4_TEXT_EVENT_REQUEST);
    assert(!strcmp(r.id, "first-1") && !strcmp(r.default_text, "hi") && r.max_chars == 8);
    assert(x4_text_input_poll(input, &r) == X4_TEXT_EVENT_NONE);
    assert(x4_text_input_complete(input, "stale", "hello") == -1);
    assert(x4_text_input_complete(input, "first-1", "hi \"X\"") == 0);
    assert(x4_text_input_take_outbound(input, wire, sizeof(wire)) == 1);
    expect_reply(wire, "TransactionComplete", "first-1", "hi \"X\"");
    assert(x4_text_input_take_outbound(input, wire, sizeof(wire)) == 0);

    request(input, "second-2", 0);
    request(input, "third-3", 0);
    request(input, "fourth-4", 0);
    request(input, "overflow-5", 0); /* bounded queue: explicit cancel */
    assert(x4_text_input_take_outbound(input, wire, sizeof(wire)) == 1);
    expect_reply(wire, "ReceiverCancel", "overflow-5", NULL);
    assert(x4_text_input_poll(input, &r) == X4_TEXT_EVENT_REQUEST && !strcmp(r.id, "second-2"));
    const char withdraw[] = "{\"type\":\"SenderCancel\",\"id\":\"second-2\"}";
    assert(x4_text_input_receive(input, withdraw, strlen(withdraw)) == 1);
    assert(x4_text_input_complete(input, "second-2", "wrong") == -1);
    assert(x4_text_input_poll(input, &r) == X4_TEXT_EVENT_WITHDRAWN && !strcmp(r.id, "second-2"));
    assert(x4_text_input_poll(input, &r) == X4_TEXT_EVENT_REQUEST && !strcmp(r.id, "third-3"));
    assert(x4_text_input_cancel(input, "third-3") == 0);
    assert(x4_text_input_take_outbound(input, wire, sizeof(wire)) == 1);
    expect_reply(wire, "ReceiverCancel", "third-3", NULL);
    assert(x4_text_input_poll(input, &r) == X4_TEXT_EVENT_REQUEST && !strcmp(r.id, "fourth-4"));
    x4_text_input_reset(input);
    assert(x4_text_input_poll(input, &r) == X4_TEXT_EVENT_WITHDRAWN && !strcmp(r.id, "fourth-4"));
    assert(x4_text_input_poll(input, &r) == X4_TEXT_EVENT_NONE);
    assert(x4_text_input_take_outbound(input, wire, sizeof(wire)) == 0);

    request(input, "password-6", 31);
    assert(x4_text_input_poll(input, &r) == X4_TEXT_EVENT_NONE);
    assert(x4_text_input_take_outbound(input, wire, sizeof(wire)) == 1);
    expect_reply(wire, "ReceiverCancel", "password-6", NULL);
    const char malformed[] = "{\"type\":\"TransactionStart\",\"id\":\"bad-8\","
        "\"target\":\"/streaming/systemUi/messages/ShowVirtualKeyboard\",\"content\":\"{}\"}";
    assert(x4_text_input_receive(input, malformed, strlen(malformed)) == 1);
    /* Missing optional fields are a valid empty request; it can be cancelled. */
    assert(x4_text_input_poll(input, &r) == X4_TEXT_EVENT_REQUEST && !strcmp(r.id, "bad-8"));
    assert(x4_text_input_cancel(input, "bad-8") == 0);
    assert(x4_text_input_take_outbound(input, wire, sizeof(wire)) == 1);
    expect_reply(wire, "ReceiverCancel", "bad-8", NULL);
    const char invalid_content[] = "{\"type\":\"TransactionStart\",\"id\":\"bad-9\","
        "\"target\":\"/streaming/systemUi/messages/ShowVirtualKeyboard\",\"content\":\"[1]\"}";
    assert(x4_text_input_receive(input, invalid_content, strlen(invalid_content)) == 1);
    assert(x4_text_input_poll(input, &r) == X4_TEXT_EVENT_NONE);
    assert(x4_text_input_take_outbound(input, wire, sizeof(wire)) == 1);
    expect_reply(wire, "ReceiverCancel", "bad-9", NULL);
    request(input, "last-7", 0);
    assert(x4_text_input_poll(input, &r) == X4_TEXT_EVENT_REQUEST);
    assert(x4_text_input_complete(input, "last-7", "123456789") == -1);
    assert(x4_text_input_complete(input, "last-7", "bad\xff") == -1);
    x4_text_input_cancel_all(input);
    assert(x4_text_input_take_outbound(input, wire, sizeof(wire)) == 1);
    expect_reply(wire, "ReceiverCancel", "last-7", NULL);
    assert(x4_text_input_poll(input, &r) == X4_TEXT_EVENT_WITHDRAWN && !strcmp(r.id, "last-7"));
    x4_text_input_destroy(input);

    /* Auth releases its session owner while an RTC retained in quarantine
     * still has callbacks and must safely receive/cancel its old requests. */
    X4TextInput *old_owner = x4_text_input_create();
    assert(old_owner);
    X4TextInput *old_rtc = x4_text_input_retain(old_owner);
    assert(old_rtc == old_owner);
    x4_text_input_destroy(old_owner);
    X4TextInput *new_owner = x4_text_input_create();
    assert(new_owner && new_owner != old_rtc);
    request(old_rtc, "old-session", 50);
    assert(x4_text_input_poll(old_rtc, &r) == X4_TEXT_EVENT_REQUEST);
    assert(x4_text_input_cancel(old_rtc, r.id) == 0);
    assert(x4_text_input_take_outbound(old_rtc, wire, sizeof(wire)) == 1);
    expect_reply(wire, "ReceiverCancel", "old-session", NULL);
    assert(x4_text_input_poll(new_owner, &r) == X4_TEXT_EVENT_NONE);
    x4_text_input_destroy(old_rtc);

    /* Backpressure reports failure and reset removes all old replies/ids. */
    for (unsigned i = 0; i < 8; ++i) {
        char id[24]; snprintf(id, sizeof(id), "rejected-%u", i);
        request(new_owner, id, 31);
    }
    assert(send_request(new_owner, "queue-full", 31) == -1);
    x4_text_input_reset(new_owner);
    assert(x4_text_input_take_outbound(new_owner, wire, sizeof(wire)) == 0);
    assert(x4_text_input_poll(new_owner, &r) == X4_TEXT_EVENT_NONE);
    request(new_owner, "fresh", 50);
    assert(x4_text_input_poll(new_owner, &r) == X4_TEXT_EVENT_REQUEST);
    assert(!strcmp(r.id, "fresh"));
    x4_text_input_destroy(new_owner);
    X4TextInput *opaque = x4_text_input_create();
    request(opaque, "{opaque.id}:1", 0);
    assert(x4_text_input_poll(opaque, &r) == X4_TEXT_EVENT_REQUEST);
    assert(!strcmp(r.id, "{opaque.id}:1"));
    assert(x4_text_input_complete(opaque, r.id, "test") == 0);
    assert(x4_text_input_take_outbound(opaque, wire, sizeof(wire)) == 1);
    expect_reply(wire, "TransactionComplete", "{opaque.id}:1", "test");
    x4_text_input_destroy(opaque);
    puts("text input protocol ok");
    return 0;
}
