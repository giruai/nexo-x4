/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/auth/http_client.h"
#include <assert.h>
#include <string.h>
int main(void) {
    char url[256];
    const char *codes[] = {"en-US", "pt-BR", "es-MX", "fr-FR", "de-DE", "it-IT"};
    for (int i = 0; i < 6; ++i) {
        assert(!strcmp(x4_http_public_locale(i), codes[i]));
        for (int detail = 0; detail < 2; ++detail) {
            assert(!x4_http_public_products_url(i, detail, url, sizeof(url)));
            assert(strstr(url, codes[i]) && strstr(url, "market=MX"));
            assert(strstr(url, detail ? "RemoteHighSapphire0" : "RemoteLowJade0"));
        }
        for (int list = 0; list < X4_SIGL_COUNT; ++list) {
            assert(!x4_http_public_sigl_url(i, (enum X4GamepassSigl)list, url, sizeof(url)));
            assert(strstr(url, codes[i]) && strstr(url, "market=MX"));
        }
    }
    assert(!x4_http_public_locale(-1) && !x4_http_public_locale(6));
    assert(x4_http_public_products_url(0, 0, url, 2) == -1);
    assert(x4_http_public_products_url(0, 2, url, sizeof(url)) == -1);
    assert(x4_http_public_sigl_url(0, (enum X4GamepassSigl)-1, url, sizeof(url)) == -1);
    assert(x4_http_public_sigl_url(0, X4_SIGL_COUNT, url, sizeof(url)) == -1);
    puts("Public metadata locale URLs: six languages and bounds passed");
}
