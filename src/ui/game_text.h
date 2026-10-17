/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>
#include "../streaming/text_input.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Local pad-driven fallback for game text transactions. The stream owner
 * consumes pad presses while this widget is active and suppresses them from
 * the remote game until all buttons are released after close. */
typedef struct {
    X4TextRequest request;
    char text[X4_TEXT_MAX_BYTES + 1];
    unsigned chars, cursor;
    int uppercase, active;
} X4GameText;
typedef enum {
    X4_GAME_TEXT_UP, X4_GAME_TEXT_DOWN, X4_GAME_TEXT_LEFT, X4_GAME_TEXT_RIGHT,
    X4_GAME_TEXT_INSERT, X4_GAME_TEXT_BACKSPACE, X4_GAME_TEXT_SPACE,
    X4_GAME_TEXT_CASE, X4_GAME_TEXT_SUBMIT, X4_GAME_TEXT_CANCEL
} X4GameTextKey;
typedef enum { X4_GAME_TEXT_EDITING, X4_GAME_TEXT_DONE, X4_GAME_TEXT_CANCELLED } X4GameTextResult;

/* A successful open makes the widget active. Scope 31 (password) is never
 * presented in our custom keyboard; leave it to provider UI or cancel. */
int x4_game_text_open(X4GameText *ui, const X4TextRequest *request);
const char *x4_game_text_keys(void);
unsigned x4_game_text_key_count(void);
X4GameTextResult x4_game_text_key(X4GameText *ui, X4GameTextKey key);
void x4_game_text_close(X4GameText *ui);
void x4_game_text_draw(const X4GameText *ui, uint32_t *pixels);

#ifdef __cplusplus
}
#endif
