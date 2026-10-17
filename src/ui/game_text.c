/* SPDX-License-Identifier: GPL-3.0-only */
#include "game_text.h"
#include "canvas.h"
#include <stdio.h>
#include <string.h>

static size_t bounded_length(const char *s, size_t cap)
{ size_t n = 0; while (n < cap && s[n]) ++n; return n; }

static const char keys[] = "abcdefghijklmnopqrstuvwxyz0123456789-_.@/:!?+,'\"";
enum { KEY_COLUMNS = 6, KEY_COUNT = sizeof(keys) - 1 };
static const uint32_t BG = 0xff171319u, PANEL = 0xff302833u;
static const uint32_t WHITE = 0xfff5f5f5u, MUTED = 0xffaeaaaa, ACCENT = 0xff69d79cu;

static unsigned count_chars(const char *text)
{
    unsigned count = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p)
        if ((*p & 0xc0) != 0x80) ++count;
    return count;
}
static void visual_text(char *out, size_t cap, const char *in, unsigned max)
{
    size_t used = 0; unsigned chars = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p && chars < max && used + 1 < cap; ++p) {
        if ((*p & 0xc0) == 0x80) continue;
        out[used++] = *p >= 0x20 && *p < 0x7f ? (char)*p : '?'; ++chars;
    }
    out[used] = 0;
}
int x4_game_text_open(X4GameText *ui, const X4TextRequest *request)
{
    if (!ui || !request || request->input_scope == 31 || !request->max_chars ||
        request->max_chars > X4_TEXT_MAX_CHARS) return -1;
    size_t len = bounded_length(request->default_text, sizeof(request->default_text));
    if (len == sizeof(request->default_text)) return -1;
    unsigned chars = count_chars(request->default_text);
    if (chars > request->max_chars) return -1;
    memset(ui, 0, sizeof(*ui));
    ui->request = *request;
    memcpy(ui->text, request->default_text, len + 1);
    ui->chars = chars;
    ui->active = 1;
    return 0;
}
const char *x4_game_text_keys(void) { return keys; }
unsigned x4_game_text_key_count(void) { return KEY_COUNT; }
X4GameTextResult x4_game_text_key(X4GameText *ui, X4GameTextKey key)
{
    if (!ui || !ui->active) return X4_GAME_TEXT_CANCELLED;
    switch (key) {
    case X4_GAME_TEXT_UP: ui->cursor = (ui->cursor + KEY_COUNT - KEY_COLUMNS) % KEY_COUNT; break;
    case X4_GAME_TEXT_DOWN: ui->cursor = (ui->cursor + KEY_COLUMNS) % KEY_COUNT; break;
    case X4_GAME_TEXT_LEFT: ui->cursor = ui->cursor % KEY_COLUMNS ? ui->cursor - 1 : ui->cursor + KEY_COLUMNS - 1; break;
    case X4_GAME_TEXT_RIGHT: ui->cursor = ui->cursor % KEY_COLUMNS == KEY_COLUMNS - 1 ? ui->cursor - KEY_COLUMNS + 1 : ui->cursor + 1; break;
    case X4_GAME_TEXT_CASE: ui->uppercase = !ui->uppercase; break;
    case X4_GAME_TEXT_BACKSPACE: {
        size_t n = strlen(ui->text);
        if (!n) break;
        do --n; while (n && ((unsigned char)ui->text[n] & 0xc0) == 0x80);
        ui->text[n] = 0; --ui->chars;
        break;
    }
    case X4_GAME_TEXT_SPACE:
    case X4_GAME_TEXT_INSERT: {
        if (ui->chars >= ui->request.max_chars) break;
        size_t n = strlen(ui->text);
        if (n + 2 > sizeof(ui->text)) break;
        char c = key == X4_GAME_TEXT_SPACE ? ' ' : keys[ui->cursor];
        if (ui->uppercase && c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        ui->text[n] = c; ui->text[n + 1] = 0; ++ui->chars;
        break;
    }
    case X4_GAME_TEXT_SUBMIT: return X4_GAME_TEXT_DONE;
    case X4_GAME_TEXT_CANCEL: return X4_GAME_TEXT_CANCELLED;
    default: break;
    }
    return X4_GAME_TEXT_EDITING;
}
void x4_game_text_close(X4GameText *ui)
{
    if (!ui) return;
    volatile unsigned char *p = (volatile unsigned char *)ui;
    for (size_t i = 0; i < sizeof(*ui); ++i) p[i] = 0;
}
/* The repository's tiny bitmap font does not include these punctuation
 * glyphs. Draw them directly for the canvas fallback; the PSBox/Inter bridge
 * uses x4_game_text_keys() for its normal, full-font renderer. */
static void punctuation(uint32_t *p, int x, int y, int scale, char c, uint32_t color)
{
    static const struct { char c; unsigned char rows[7]; } marks[] = {
        {'_', {0,0,0,0,0,0,31}}, {'@', {14,17,23,21,23,16,14}},
        {'!', {4,4,4,4,4,0,4}}, {',', {0,0,0,0,0,4,8}},
        {'\'', {4,4,8,0,0,0,0}}, {'"', {10,10,0,0,0,0,0}},
    };
    for (unsigned i = 0; i < sizeof(marks) / sizeof(marks[0]); ++i) {
        if (marks[i].c != c) continue;
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if (marks[i].rows[row] & (1u << (4 - col)))
                    x4_rect(p, x + col * scale, y + row * scale, scale, scale, color);
        return;
    }
}
void x4_game_text_draw(const X4GameText *ui, uint32_t *p)
{
    if (!ui || !ui->active || !p) return;
    x4_rect(p, 105, 106, 1710, 870, BG);
    x4_rect(p, 130, 132, 1660, 820, PANEL);
    x4_text(p, 166, 166, 4, "TEXTO DEL JUEGO", WHITE);
    char line[97];
    visual_text(line, sizeof(line), ui->request.title, 55);
    x4_text(p, 166, 226, 3, line, WHITE);
    visual_text(line, sizeof(line), ui->request.description, 55);
    x4_text(p, 166, 268, 2, line, MUTED);
    x4_rect(p, 166, 318, 1588, 84, BG);
    size_t n = strlen(ui->text), start = n > 68 ? n - 68 : 0;
    while (start < n && ((unsigned char)ui->text[start] & 0xc0) == 0x80) ++start;
    visual_text(line, sizeof(line), ui->text + start, 68);
    x4_text(p, 188, 343, 3, line, WHITE);
    for (unsigned i = 0; line[i]; ++i)
        punctuation(p, 188 + (int)i * 18, 343, 3, line[i], WHITE);
    snprintf(line, sizeof(line), "%u / %u", ui->chars, ui->request.max_chars);
    x4_text(p, 1450, 420, 2, line, MUTED);
    for (unsigned i = 0; i < KEY_COUNT; ++i) {
        int x = 170 + (int)(i % KEY_COLUMNS) * 155, y = 445 + (int)(i / KEY_COLUMNS) * 55;
        int selected = i == ui->cursor;
        x4_rect(p, x, y, 130, 46, selected ? ACCENT : BG);
        char one[2] = {keys[i], 0};
        if (ui->uppercase && one[0] >= 'a' && one[0] <= 'z') one[0] -= 'a' - 'A';
        x4_text(p, x + 53, y + 8, 4, one, selected ? BG : WHITE);
        punctuation(p, x + 53, y + 8, 4, one[0], selected ? BG : WHITE);
    }
    x4_text(p, 1180, 490, 2, "CRUCETA ELEGIR", WHITE);
    x4_text(p, 1180, 535, 2, "X ESCRIBIR", WHITE);
    x4_text(p, 1180, 580, 2, ui->uppercase ? "MAYUSCULAS" : "MINUSCULAS", ACCENT);
    x4_text(p, 1180, 605, 2, "TRIANGULO CAMBIAR", WHITE);
    x4_text(p, 1180, 625, 2, "CUADRADO BORRAR", WHITE);
    x4_text(p, 1180, 670, 2, "R1 ESPACIO", WHITE);
    x4_text(p, 1180, 715, 2, "R2 ENVIAR", ACCENT);
    x4_text(p, 1180, 760, 2, "CIRCULO CANCELAR", MUTED);
}
