/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/ui/game_text.h"
#include "../src/video/display.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static X4TextRequest request(unsigned limit, const char *initial)
{
    X4TextRequest r = {0};
    strcpy(r.id, "text-1");
    strcpy(r.title, "Search");
    strcpy(r.description, "Enter text");
    strcpy(r.default_text, initial);
    r.max_chars = limit;
    return r;
}
static void move_to(X4GameText *ui, unsigned key)
{
    assert(key < x4_game_text_key_count());
    while (ui->cursor / 6 < key / 6) assert(x4_game_text_key(ui, X4_GAME_TEXT_DOWN) == X4_GAME_TEXT_EDITING);
    while (ui->cursor / 6 > key / 6) assert(x4_game_text_key(ui, X4_GAME_TEXT_UP) == X4_GAME_TEXT_EDITING);
    while (ui->cursor % 6 < key % 6) assert(x4_game_text_key(ui, X4_GAME_TEXT_RIGHT) == X4_GAME_TEXT_EDITING);
    while (ui->cursor % 6 > key % 6) assert(x4_game_text_key(ui, X4_GAME_TEXT_LEFT) == X4_GAME_TEXT_EDITING);
}
int main(void)
{
    assert(x4_game_text_key_count() == 48);
    const char *keys = x4_game_text_keys();
    assert(strlen(keys) == 48);
    assert(strchr(keys, '-') && strchr(keys, '_') && strchr(keys, '.') && strchr(keys, '@') &&
           strchr(keys, '/') && strchr(keys, ':') && strchr(keys, '!') && strchr(keys, '?') &&
           strchr(keys, '+') && strchr(keys, ',') && strchr(keys, '\'') && strchr(keys, '"'));

    X4GameText ui;
    X4TextRequest r = request(4, "");
    assert(x4_game_text_open(&ui, &r) == 0);
    assert(x4_game_text_key(&ui, X4_GAME_TEXT_INSERT) == X4_GAME_TEXT_EDITING);
    assert(!strcmp(ui.text, "a")); /* X inserts the selected letter. */
    x4_game_text_key(&ui, X4_GAME_TEXT_CASE);
    x4_game_text_key(&ui, X4_GAME_TEXT_INSERT);
    assert(!strcmp(ui.text, "aA"));
    move_to(&ui, 36); /* '-' in the punctuation row. */
    x4_game_text_key(&ui, X4_GAME_TEXT_INSERT);
    assert(!strcmp(ui.text, "aA-"));
    move_to(&ui, 47); /* final '"' key and wrap at 48. */
    x4_game_text_key(&ui, X4_GAME_TEXT_INSERT);
    assert(!strcmp(ui.text, "aA-\""));
    x4_game_text_key(&ui, X4_GAME_TEXT_INSERT);
    assert(!strcmp(ui.text, "aA-\"")); /* four-character maximum */
    assert(x4_game_text_key(&ui, X4_GAME_TEXT_SUBMIT) == X4_GAME_TEXT_DONE);

    size_t pixels = (size_t)X4_WIDTH * X4_HEIGHT;
    uint32_t *guarded = malloc((pixels + 2) * sizeof(*guarded));
    assert(guarded);
    guarded[0] = 0x11223344u;
    guarded[pixels + 1] = 0x55667788u;
    memset(guarded + 1, 0, pixels * sizeof(*guarded));
    x4_game_text_draw(&ui, guarded + 1);
    assert(guarded[0] == 0x11223344u && guarded[pixels + 1] == 0x55667788u);
    assert(guarded[1 + 132 * X4_WIDTH + 130] != 0);
    free(guarded);
    x4_game_text_close(&ui);
    assert(!ui.active && !ui.text[0]);

    /* Backspace removes one UTF-8 code point, not one byte. */
    r = request(3, "ni\xc3\xb1");
    assert(x4_game_text_open(&ui, &r) == 0 && ui.chars == 3);
    x4_game_text_key(&ui, X4_GAME_TEXT_BACKSPACE);
    assert(!strcmp(ui.text, "ni") && ui.chars == 2);
    x4_game_text_key(&ui, X4_GAME_TEXT_SPACE);
    assert(!strcmp(ui.text, "ni "));
    assert(x4_game_text_key(&ui, X4_GAME_TEXT_CANCEL) == X4_GAME_TEXT_CANCELLED);
    x4_game_text_close(&ui);

    r = request(2, ""); r.input_scope = 31;
    assert(x4_game_text_open(&ui, &r) == -1);
    puts("game text widget ok");
    return 0;
}
