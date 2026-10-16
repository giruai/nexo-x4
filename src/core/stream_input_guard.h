/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

/* Keep menu input out of Xbox until one complete neutral sample after close.
 * The 20-count stick band covers normal centre drift (about 15% of travel),
 * and the trigger band covers small resting noise without requiring exact 0. */
typedef struct { int waiting_neutral; } X4StreamInputGuard;

static inline int x4_stream_input_guard_step(X4StreamInputGuard *guard, int menu_capture,
    uint32_t held, uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry,
    uint8_t l2, uint8_t r2)
{
    if (menu_capture) guard->waiting_neutral = 1;
    if (!guard->waiting_neutral) return 0;
    int neutral = !held && l2 <= 8 && r2 <= 8 &&
        lx >= 108 && lx <= 148 && ly >= 108 && ly <= 148 &&
        rx >= 108 && rx <= 148 && ry >= 108 && ry <= 148;
    if (!menu_capture && neutral) guard->waiting_neutral = 0;
    return 1; /* The neutral release frame is still owned by the menu. */
}

static inline int x4_stream_options_owned(int psbox_active, int page, int game_input)
{
    return game_input || (psbox_active && page == 6);
}

static inline int x4_stream_exit_chord(int psbox_active, int page, int game_input,
    int chord, int options_pressed)
{
    return x4_stream_options_owned(psbox_active, page, game_input) &&
        chord && options_pressed;
}
