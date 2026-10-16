/* SPDX-License-Identifier: GPL-3.0-only */
/* Product fields and hydration follow PSBox catalog.cpp, Copyright (C) 2026 RafaelNGP. */
#include "metadata.h"
#include "../../auth/json.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define X4_PSB_PRODUCTS_BODY (2u * 1024u * 1024u + 1u)
static const char image_prefix[] = "https://store-images.s-microsoft.com/image/";

static bool product_id(const char *id)
{
    if (!id) return false;
    size_t n = 0;
    for (; n < 31 && id[n]; ++n) {
        unsigned char c = (unsigned char)id[n];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9'))) return false;
    }
    return n > 0 && !id[n];
}

static void text_member(X4JsonSpan object, const char *key, char *dst, size_t cap)
{
    X4JsonSpan value;
    dst[0] = 0;
    if (x4_json_member(object, key, &value) == 1)
        (void)x4_json_text(value, dst, cap);
}

static void image_member(X4JsonSpan object, const char *key, char *dst, size_t cap)
{
    X4JsonSpan image, value;
    char raw[384];
    dst[0] = 0;
    if (x4_json_member(object, key, &image) != 1 ||
        x4_json_member(image, "URL", &value) != 1 ||
        x4_json_token(value, raw, sizeof(raw)) < 0) return;
    const char *url = raw;
    char absolute[384];
    if (raw[0] == '/' && raw[1] == '/') {
        int n = snprintf(absolute, sizeof(absolute), "https:%s", raw);
        if (n < 0 || (size_t)n >= sizeof(absolute)) return;
        url = absolute;
    }
    size_t prefix = sizeof(image_prefix) - 1;
    if (strncmp(url, image_prefix, prefix)) return;
    const char *end = strchr(url, '?');
    size_t length = end ? (size_t)(end - url) : strlen(url);
    if (length <= prefix || length >= cap) return;
    for (size_t i = prefix; i < length; ++i) {
        unsigned char c = (unsigned char)url[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_' || c == '/')) return;
    }
    memcpy(dst, url, length);
    dst[length] = 0;
}

int x4_psbox_metadata_batch(X4Http *http, const X4CatalogSnapshot *catalog,
    size_t start_index, int detailed, X4PsboxMetadata *out, size_t out_capacity,
    size_t *next_index, size_t *out_count, int *http_status, const _Atomic int *cancel)
{
    if (!http || !catalog || catalog->state != X4_CATALOG_READY || !out || !out_capacity ||
        !next_index || !out_count || !http_status || (detailed != 0 && detailed != 1) ||
        start_index > catalog->count || catalog->count > X4_CATALOG_MAX) return X4_HTTP_ARGUMENT;
    *next_index = start_index;
    *out_count = 0;
    *http_status = 0;
    if (cancel && atomic_load(cancel)) return X4_HTTP_CANCELLED;
    if (start_index == catalog->count) return 0;
    size_t limit = detailed ? X4_PSB_METADATA_DETAIL_BATCH : X4_PSB_METADATA_LIGHT_BATCH;
    if (limit > out_capacity) limit = out_capacity;
    char ids[X4_PSB_METADATA_LIGHT_BATCH][32] = {{0}};
    size_t selected = 0;
    size_t cursor = start_index;
    for (; cursor < catalog->count && selected < limit; ++cursor) {
        const char *id = catalog->titles[cursor].product_id;
        if (!product_id(id)) continue;
        memcpy(ids[selected++], id, strlen(id) + 1);
    }
    *next_index = cursor;
    if (!selected) return 0;
    char request[2200];
    size_t used = 0;
    int n = snprintf(request, sizeof(request), "{\"Products\":[");
    if (n < 0 || (size_t)n >= sizeof(request)) return X4_HTTP_BOUND;
    used = (size_t)n;
    for (size_t i = 0; i < selected; ++i) {
        n = snprintf(request + used, sizeof(request) - used, "%s\"%s\"", i ? "," : "", ids[i]);
        if (n < 0 || (size_t)n >= sizeof(request) - used) return X4_HTTP_BOUND;
        used += (size_t)n;
    }
    n = snprintf(request + used, sizeof(request) - used, "]}");
    if (n < 0 || (size_t)n >= sizeof(request) - used) return X4_HTTP_BOUND;
    char *response = malloc(X4_PSB_PRODUCTS_BODY);
    if (!response) return X4_HTTP_ALLOCATION;
    size_t length = 0;
    int status = 0;
    int rc = x4_http_gamepass_products(http, detailed, request, response, X4_PSB_PRODUCTS_BODY,
        &length, &status, cancel);
    *http_status = status;
    if (rc == 0 && status == 200) {
        X4JsonSpan root, products;
        if (x4_json_parse(response, length, &root) ||
            x4_json_member(root, "Products", &products) != 1 ||
            x4_json_type(products) != X4_JSON_T_OBJECT) rc = X4_HTTP_PROTOCOL;
        else {
            for (size_t i = 0; i < selected; ++i) {
                X4JsonSpan entry;
                int found = x4_json_member(products, ids[i], &entry);
                if (found == 0) {
                    /* The public catalog commonly keys products in uppercase
                     * even if a regional response used lowercase IDs. */
                    char upper[sizeof(ids[i])];
                    memcpy(upper, ids[i], sizeof(upper));
                    for (size_t k = 0; upper[k]; ++k)
                        if (upper[k] >= 'a' && upper[k] <= 'z') upper[k] = (char)(upper[k] - 'a' + 'A');
                    found = x4_json_member(products, upper, &entry);
                }
                if (found != 1 ||
                    x4_json_type(entry) != X4_JSON_T_OBJECT) continue;
                X4PsboxMetadata *item = &out[*out_count];
                memset(item, 0, sizeof(*item));
                memcpy(item->product_id, ids[i], strlen(ids[i]) + 1);
                text_member(entry, "ProductTitle", item->name, sizeof(item->name));
                text_member(entry, "PublisherName", item->publisher, sizeof(item->publisher));
                text_member(entry, "ProductDescriptionShort", item->description, sizeof(item->description));
                if (!item->description[0])
                    text_member(entry, "ProductDescription", item->description, sizeof(item->description));
                image_member(entry, "Image_Tile", item->tile_url, sizeof(item->tile_url));
                if (!item->tile_url[0])
                    image_member(entry, "Image_Poster", item->tile_url, sizeof(item->tile_url));
                image_member(entry, "Image_Hero", item->hero_url, sizeof(item->hero_url));
                if (!item->hero_url[0])
                    image_member(entry, "Image_TitledHero", item->hero_url, sizeof(item->hero_url));
                ++*out_count;
            }
        }
    } else if (rc == 0) rc = X4_HTTP_PROTOCOL;
    x4_secure_clear(response, X4_PSB_PRODUCTS_BODY);
    free(response);
    return rc;
}
