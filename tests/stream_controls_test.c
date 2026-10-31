/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/input/stream_controls.h"
#include "../src/input/gamepad.h"
#include <assert.h>
#include <stdio.h>

/* Portable test values, deliberately not SDK ABI constants. */
enum { OPTIONS = 1u, TOUCHPAD = 2u, L3 = 4u, R3 = 8u, CROSS = 16u };
static const X4StreamControlsConfig config = { TOUCHPAD, L3 | R3 };
static X4StreamControls state;
static X4StreamControlsSample in;

static X4StreamControlsResult step(void)
{ return x4_stream_controls_step(&state, &in, &config); }
static void reset(void)
{
    x4_stream_controls_reset(&state);
    in = (X4StreamControlsSample){ .connected = true, .streaming = true,
        .analog_neutral = true, .touch = { .valid = true }, .now_ms = 1000 };
    assert(!step().capture_remote);
}
/* Model the actual controller mapping after the routing decision. */
static uint16_t routed(X4StreamControlsResult r)
{
    uint32_t buttons = in.buttons & ~r.consume_buttons;
    if (r.capture_remote) return 0;
    return ((buttons & OPTIONS) ? X4_GAMEPAD_MENU : 0) |
        ((buttons & TOUCHPAD) ? X4_GAMEPAD_VIEW : 0);
}
static void contact(unsigned x, unsigned y)
{
    in.touch = (X4StreamTouch){ .valid = true, .count = 1, .id = 7,
        .x = (uint16_t)x, .y = (uint16_t)y };
}
static X4StreamControlsResult release(void)
{ in.touch.count = 0; return step(); }
static void swipe(unsigned start_y, unsigned end_y)
{
    contact(32000, start_y); assert(step().action == X4_STREAM_CONTROL_NONE);
    in.now_ms += 100; contact(33000, end_y);
    assert(step().action == X4_STREAM_CONTROL_NONE);
    in.now_ms += 40;
}

static void remote_buttons(void)
{
    reset();
    in.buttons = OPTIONS;
    for (unsigned n = 0; n < 200; ++n) {
        X4StreamControlsResult r = step();
        assert(r.action == X4_STREAM_CONTROL_NONE && routed(r) == X4_GAMEPAD_MENU);
        ++in.now_ms;
    }
    in.buttons = 0; assert(routed(step()) == 0);
    in.buttons = OPTIONS; assert(routed(step()) == X4_GAMEPAD_MENU);
    in.buttons = 0; assert(routed(step()) == 0); /* Brief tap is not deferred. */
    contact(30000, 35000); assert(step().action == X4_STREAM_CONTROL_NONE);
    in.buttons = TOUCHPAD;
    assert(routed(step()) == X4_GAMEPAD_VIEW);
    in.now_ms += 100; contact(30000, 5000);
    assert(routed(step()) == X4_GAMEPAD_VIEW);
    in.buttons = 0;
    assert(release().action == X4_STREAM_CONTROL_NONE); /* Click is never a swipe. */
}

static void gestures(void)
{
    reset(); swipe(50000, 12000);
    X4StreamControlsResult r = release();
    assert(r.action == X4_STREAM_CONTROL_GUIDE && r.capture_remote);
    assert(step().action == X4_STREAM_CONTROL_NONE); /* Neutral release guard. */
    assert(!step().capture_remote);
    swipe(10000, 50000); r = release();
    assert(r.action == X4_STREAM_CONTROL_MENU && r.capture_remote);
    assert(step().action == X4_STREAM_CONTROL_NONE);

    reset(); swipe(30000, 31000); assert(release().action == X4_STREAM_CONTROL_NONE);
    reset(); swipe(50000, 10000); in.now_ms += 800;
    assert(release().action == X4_STREAM_CONTROL_NONE); /* Slow/held contact. */
    reset(); contact(32000, 50000); step(); in.now_ms += 5;
    contact(32000, 10000); step(); in.now_ms += 5;
    assert(release().action == X4_STREAM_CONTROL_NONE); /* Sensor spike/tap. */
    reset(); swipe(50000, 10000); in.touch.x = 50000; step();
    in.touch.x = 32000; step();
    assert(release().action == X4_STREAM_CONTROL_NONE); /* Returning horizontally cannot rearm. */

    reset(); swipe(50000, 10000); in.touch.count = 2; step();
    in.touch.count = 1; step(); assert(release().action == X4_STREAM_CONTROL_NONE);
    swipe(50000, 10000); assert(release().action == X4_STREAM_CONTROL_GUIDE);
    reset(); swipe(50000, 10000); ++in.touch.id; step();
    assert(release().action == X4_STREAM_CONTROL_NONE);
    reset(); swipe(50000, 10000); in.touch.valid = false; step();
    in.touch.valid = true; assert(release().action == X4_STREAM_CONTROL_NONE);
    reset(); in.now_ms = UINT32_MAX - 80;
    swipe(50000, 10000); assert(release().action == X4_STREAM_CONTROL_GUIDE);
    reset(); swipe(50000, 10000); in.now_ms -= 200;
    assert(release().action == X4_STREAM_CONTROL_NONE); /* Clock reversal cancels. */
}

static void chord_and_capture(void)
{
    reset();
    in.buttons = L3; assert(!step().capture_remote); /* Immediate standalone L3. */
    in.buttons = L3 | R3;
    X4StreamControlsResult r = step();
    assert(r.action == X4_STREAM_CONTROL_MENU && r.capture_remote && !routed(r));
    for (unsigned n = 0; n < 20; ++n) assert(step().action == X4_STREAM_CONTROL_NONE);
    in.buttons = R3; assert(step().capture_remote);
    in.buttons = OPTIONS; assert(step().capture_remote && !routed(step()));
    in.buttons = 0; in.analog_neutral = false; assert(step().capture_remote);
    in.analog_neutral = true; assert(step().capture_remote);
    in.buttons = OPTIONS; assert(routed(step()) == X4_GAMEPAD_MENU);
    in.buttons = 0; step();
    in.buttons = R3; assert(!step().capture_remote);
    in.buttons |= L3; assert(step().action == X4_STREAM_CONTROL_MENU); /* Opposite prefix order. */

    reset(); in.menu_open = true; in.buttons = CROSS;
    assert(step().capture_remote && step().action == X4_STREAM_CONTROL_NONE);
    in.menu_open = false; assert(step().capture_remote);
    in.buttons = 0; assert(step().capture_remote); assert(!step().capture_remote);
    reset(); swipe(50000, 10000); in.text_capture = true;
    assert(step().capture_remote && step().action == X4_STREAM_CONTROL_NONE);
    in.buttons = L3 | R3; assert(step().action == X4_STREAM_CONTROL_NONE);
    in.text_capture = false; assert(release().capture_remote);
    in.buttons = 0; assert(step().capture_remote); assert(!step().capture_remote);
    swipe(50000, 10000); assert(release().action == X4_STREAM_CONTROL_GUIDE);
}

static void transitions(void)
{
    reset(); swipe(50000, 10000); in.connected = false;
    assert(step().capture_remote && step().action == X4_STREAM_CONTROL_NONE);
    in.connected = true; assert(step().action == X4_STREAM_CONTROL_NONE);
    assert(release().action == X4_STREAM_CONTROL_NONE);
    swipe(50000, 10000); assert(release().action == X4_STREAM_CONTROL_GUIDE);
    reset(); in.streaming = false;
    swipe(50000, 10000); assert(release().action == X4_STREAM_CONTROL_NONE);
    contact(32000, 50000); in.streaming = true; step();
    in.now_ms += 100; contact(32000, 10000); step();
    assert(release().action == X4_STREAM_CONTROL_NONE); /* Catalog contact cannot carry into game. */
    reset(); in.connected = false; step(); in.connected = true; in.buttons = L3 | R3;
    assert(step().action == X4_STREAM_CONTROL_NONE); /* Reconnect-held chord cannot fire. */
    in.buttons = 0; step(); in.buttons = L3 | R3;
    assert(step().action == X4_STREAM_CONTROL_MENU);
    reset(); in.touch.valid = false; in.buttons = L3 | R3;
    assert(step().action == X4_STREAM_CONTROL_MENU); /* Remote Play has no touch geometry. */
}

int main(void)
{
    remote_buttons(); gestures(); chord_and_capture(); transitions();
    puts("stream controls routing ok");
    return 0;
}
