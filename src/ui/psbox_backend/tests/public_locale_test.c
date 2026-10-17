/* SPDX-License-Identifier: GPL-3.0-only */
#include "../../../auth/http_client.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char *const locales[6] = {
        "en-US", "pt-BR", "es-MX", "fr-FR", "de-DE", "it-IT"
    };
    static const char *const ids[4] = {
        "06323672-b8c8-43cc-b0de-32d5a9834749",
        "6a589fa0-d493-472b-8e20-3813699d7056",
        "d8f4afcd-882a-49e3-86b3-f61fa0172b75",
        "31ff2361-2772-4622-849b-f4f1abb4ad1b"
    };
    char actual[160], expected[160];
    for (int language = 0; language < 6; ++language) {
        assert(!strcmp(x4_http_public_locale(language), locales[language]));
        for (int detailed = 0; detailed < 2; ++detailed) {
            assert(x4_http_public_products_url(language, detailed, actual, sizeof(actual)) == 0);
            snprintf(expected, sizeof(expected),
                "https://catalog.gamepass.com/v3/products?market=MX&language=%s&hydration=%s",
                locales[language], detailed ? "RemoteHighSapphire0" : "RemoteLowJade0");
            assert(!strcmp(actual, expected));
        }
        for (int list = 0; list < X4_SIGL_COUNT; ++list) {
            assert(x4_http_public_sigl_url(language, (enum X4GamepassSigl)list,
                actual, sizeof(actual)) == 0);
            snprintf(expected, sizeof(expected),
                "https://catalog.gamepass.com/sigls/v2?id=%s&market=MX&language=%s",
                ids[list], locales[language]);
            assert(!strcmp(actual, expected));
        }
    }
    assert(x4_http_public_locale(-1) == NULL);
    assert(x4_http_public_locale(6) == NULL);
    memset(actual, 'Q', sizeof(actual));
    assert(x4_http_public_products_url(6, 0, actual, sizeof(actual)) < 0);
    assert(actual[0] == 'Q');
    assert(x4_http_public_products_url(2, 2, actual, sizeof(actual)) < 0);
    assert(x4_http_public_sigl_url(2, X4_SIGL_COUNT, actual, sizeof(actual)) < 0);
    assert(x4_http_public_products_url(2, 0, actual, 8) < 0);
    assert(x4_http_public_sigl_url(2, X4_SIGL_RECENT, actual, 8) < 0);
    puts("Public metadata locale URLs: PASS");
    return 0;
}
