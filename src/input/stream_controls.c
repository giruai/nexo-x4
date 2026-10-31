/* SPDX-License-Identifier: GPL-3.0-only */
#include "stream_controls.h"
#include <string.h>

/* At least 1/4 pad height, at most 1/8 pad width of horizontal excursion.
 * Bound the whole contact, not just its last segment, to reject holds/drags.
 * Unsigned subtraction makes an ordinary uint32 millisecond wrap harmless. */
enum { MIN_SWIPE = 16384, MAX_HORIZONTAL = 8192,
       MIN_SWIPE_MS = 50, MAX_SWIPE_MS = 750 };

void x4_stream_controls_reset(X4StreamControls *s)
{
    if (s) memset(s, 0, sizeof(*s));
}

static void cancel_touch(X4StreamControls *s, const X4StreamTouch *t)
{
    s->touching = false;
    s->touch_blocked = t->count != 0;
}

static enum X4StreamControlAction touch_step(X4StreamControls *s,
    const X4StreamControlsSample *in, uint32_t click)
{
    const X4StreamTouch *t = &in->touch;
    if (!t->valid || t->count > 1 || (in->buttons & click)) {
        cancel_touch(s, t);
        return X4_STREAM_CONTROL_NONE;
    }
    if (s->touch_blocked) {
        if (!t->count) s->touch_blocked = false;
        return X4_STREAM_CONTROL_NONE;
    }
    if (!t->count) {
        if (!s->touching) return X4_STREAM_CONTROL_NONE;
        s->touching = false; /* Release generates at most one action. */
        uint32_t elapsed = in->now_ms - s->touch_start_ms;
        int dy = (int)s->last_y - s->start_y;
        if (elapsed < MIN_SWIPE_MS || elapsed > MAX_SWIPE_MS)
            return X4_STREAM_CONTROL_NONE;
        if (dy <= -MIN_SWIPE) return X4_STREAM_CONTROL_GUIDE;
        if (dy >= MIN_SWIPE) return X4_STREAM_CONTROL_MENU;
        return X4_STREAM_CONTROL_NONE;
    }
    if (!s->touching) {
        s->touching = true;
        s->touch_id = t->id;
        s->start_x = t->x;
        s->start_y = s->last_y = t->y;
        s->touch_start_ms = in->now_ms;
    } else {
        int dx = (int)t->x - s->start_x;
        if (t->id != s->touch_id || dx < -MAX_HORIZONTAL || dx > MAX_HORIZONTAL ||
            in->now_ms - s->touch_start_ms > MAX_SWIPE_MS) {
            cancel_touch(s, t);
            return X4_STREAM_CONTROL_NONE;
        }
        s->last_y = t->y;
    }
    return X4_STREAM_CONTROL_NONE;
}

X4StreamControlsResult x4_stream_controls_step(X4StreamControls *s,
    const X4StreamControlsSample *in, const X4StreamControlsConfig *config)
{
    X4StreamControlsResult out = {0};
    if (!s || !in || !config) return out;
    if (!in->streaming || !in->connected) {
        x4_stream_controls_reset(s);
        out.capture_remote = !in->connected;
        out.consume_buttons = out.capture_remote ? UINT32_MAX : 0;
        return out;
    }
    if (!s->active) {
        s->active = true;
        /* A contact/chord held before entering a game is not a new gesture. */
        s->touch_blocked = in->touch.count != 0;
        s->chord_armed = in->buttons == 0;
    }
    if (in->text_capture || in->menu_open || s->waiting_neutral) {
        bool ui_capture = in->text_capture || in->menu_open;
        s->waiting_neutral = true;
        cancel_touch(s, &in->touch);
        s->chord_armed = in->buttons == 0;
        if (!ui_capture && !in->buttons && in->analog_neutral && !in->touch.count)
            s->waiting_neutral = false;
        out.capture_remote = true; /* Own the complete neutral release frame. */
    } else {
        uint32_t chord = config->menu_chord;
        if (!in->buttons) s->chord_armed = true;
        if (chord && s->chord_armed && in->buttons == chord) {
            out.action = X4_STREAM_CONTROL_MENU;
            s->chord_armed = false;
            cancel_touch(s, &in->touch);
        } else out.action = touch_step(s, in, config->touchpad_button);
        if (out.action != X4_STREAM_CONTROL_NONE) {
            s->waiting_neutral = true;
            out.capture_remote = true;
        }
    }
    out.consume_buttons = out.capture_remote ? UINT32_MAX : 0;
    return out;
}
