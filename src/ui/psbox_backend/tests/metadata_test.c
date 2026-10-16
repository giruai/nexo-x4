/* SPDX-License-Identifier: GPL-3.0-only */
#include "../metadata.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char response[] =
    "{\"Products\":{\"9TEST0001\":{\"ProductTitle\":\"Juego real\","
    "\"PublisherName\":\"Estudio\",\"ProductDescriptionShort\":\"Descripcion\","
    "\"Image_Tile\":{\"URL\":\"//store-images.s-microsoft.com/image/apps.123\"},"
    "\"Image_Hero\":{\"URL\":\"https://evil.invalid/image/forbidden\"}}}}";

int x4_http_gamepass_products(X4Http *http, int detailed, const char *json, char *body,
    size_t capacity, size_t *length, int *status, const _Atomic int *cancel)
{
    (void)http; (void)cancel;
    assert(detailed == 0);
    assert(!strcmp(json, "{\"Products\":[\"9test0001\"]}"));
    assert(capacity > sizeof(response));
    memcpy(body, response, sizeof(response));
    *length = sizeof(response) - 1;
    *status = 200;
    return 0;
}
void x4_secure_clear(void *data, size_t size) { memset(data, 0, size); }

int main(void)
{
    X4CatalogSnapshot *catalog = calloc(1, sizeof(*catalog));
    assert(catalog);
    catalog->state = X4_CATALOG_READY;
    catalog->count = 2;
    strcpy(catalog->titles[0].product_id, "9test0001");
    strcpy(catalog->titles[1].product_id, "bad-id");
    X4PsboxMetadata out[2] = {0};
    size_t next = 0, count = 0;
    int status = 0;
    int rc = x4_psbox_metadata_batch((X4Http *)catalog, catalog, 0, 0, out, 2,
        &next, &count, &status, NULL);
    assert(rc == 0 && next == 2 && count == 1 && status == 200);
    assert(!strcmp(out[0].product_id, "9test0001"));
    assert(!strcmp(out[0].name, "Juego real"));
    assert(!strcmp(out[0].publisher, "Estudio"));
    assert(!strcmp(out[0].tile_url, "https://store-images.s-microsoft.com/image/apps.123"));
    assert(!out[0].hero_url[0]);
    free(catalog);
    puts("PSBox metadata: PASS");
    return 0;
}
