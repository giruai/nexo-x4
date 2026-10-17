/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/auth/status_copy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int utf8_valid(const char *s)
{
    for (const unsigned char *p = (const unsigned char *)s; *p;) {
        unsigned n = *p < 0x80 ? 1u : *p >= 0xc2 && *p <= 0xdf ? 2u :
            *p >= 0xe0 && *p <= 0xef ? 3u : *p >= 0xf0 && *p <= 0xf4 ? 4u : 0u;
        if (!n) return 0;
        for (unsigned i = 1; i < n; ++i) if (!p[i] || (p[i] & 0xc0) != 0x80) return 0;
        p += n;
    }
    return 1;
}
int main(void)
{
    char out[512];
    assert(x4_status_key_count() == 165);
    assert(x4_status_key(x4_status_key_count()) == NULL);
    for (size_t i = 0; i < x4_status_key_count(); ++i) {
        const char *key = x4_status_key(i);
        assert(key && *key);
        for (size_t j = i + 1; j < x4_status_key_count(); ++j)
            assert(strcmp(key, x4_status_key(j)) != 0);
        for (int language = 0; language < X4_STATUS_LANGUAGES; ++language) {
            x4_status_format(out, sizeof(out), key, language);
            assert(out[0] && utf8_valid(out));
            assert(!(out[0] >= 'a' && out[0] <= 'z'));
        }
    }
    x4_status_format(out, sizeof(out), "sin sesion", X4_STATUS_EN);
    assert(!strcmp(out, "No session"));
    x4_status_format(out, sizeof(out), "sin sesion", X4_STATUS_PT_BR);
    assert(!strcmp(out, "Sem sessão"));
    x4_status_format(out, sizeof(out), "sin sesion", X4_STATUS_ES);
    assert(!strcmp(out, "Sin sesion"));
    x4_status_format(out, sizeof(out), "sin sesion", X4_STATUS_FR);
    assert(!strcmp(out, "Aucune session"));
    x4_status_format(out, sizeof(out), "sin sesion", X4_STATUS_DE);
    assert(!strcmp(out, "Keine Sitzung"));
    x4_status_format(out, sizeof(out), "sin sesion", X4_STATUS_IT);
    assert(!strcmp(out, "Nessuna sessione"));
    x4_status_format(out, sizeof(out), "árbol", X4_STATUS_ES);
    assert(!strcmp(out, "Árbol"));
    x4_status_format(out, sizeof(out), "ó", X4_STATUS_EN);
    assert(!strcmp(out, "Ó"));
    for (size_t cap = 1; cap <= 5; ++cap) {
        char tiny[7] = {'#','#','#','#','#','#','#'};
        x4_status_format(tiny, cap, "Échec de la connexion", X4_STATUS_FR);
        assert(tiny[cap] == '#');
        assert(utf8_valid(tiny));
    }
    x4_status_format(out, sizeof(out), "HTTPS: network/TLS", X4_STATUS_DE);
    assert(!strcmp(out, "HTTPS: network/TLS"));
    x4_status_format(out, sizeof(out), "HTTPS: lectura de respuesta", X4_STATUS_EN);
    assert(!strcmp(out, "HTTPS: Reading response"));
    x4_status_format(out, sizeof(out), "HTTPS: lectura de respuesta", X4_STATUS_PT_BR);
    assert(!strcmp(out, "HTTPS: Leitura da resposta"));
    puts("status localization ok");
    return 0;
}
