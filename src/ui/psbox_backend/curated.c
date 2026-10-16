/* SPDX-License-Identifier: GPL-3.0-only */
#include "curated.h"
#include "../../auth/json.h"
#include <string.h>

/* PSBox upstream library.cpp uses ListSpec{SIGL, 40} for these four
 * curated sections. A 40-card window is intentional, not truncation of the
 * verified All Games catalog. Parse the entire response before publishing. */
#define X4_PSB_CURATED_WINDOW 40u

static int same_id(const char *a, const char *b)
{
    for (; *a && *b; ++a, ++b) {
        unsigned char x = (unsigned char)*a, y = (unsigned char)*b;
        if (x >= 'a' && x <= 'z') x -= 'a' - 'A';
        if (y >= 'a' && y <= 'z') y -= 'a' - 'A';
        if (x != y) return 0;
    }
    return !*a && !*b;
}

static int valid_id(const char *id)
{
    size_t n = 0;
    for (; id[n]; ++n) {
        unsigned char c = (unsigned char)id[n];
        if (n == 31 || !((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return 0;
    }
    return n != 0;
}

int x4_psbox_curated_parse(const char *json, size_t length,
    const X4CatalogSnapshot *catalog, X4PsboxCuratedRow *out)
{
    if (!json || !catalog || !out || length > X4_JSON_DOCUMENT_MAX ||
        catalog->state != X4_CATALOG_READY || catalog->truncated ||
        catalog->count > X4_CATALOG_MAX) return -1;
    memset(out, 0, sizeof(*out));
    X4JsonSpan root, item, value;
    if (x4_json_parse(json, length, &root) || x4_json_type(root) != X4_JSON_T_ARRAY) return -1;
    size_t cursor = 0;
    int found_metadata = 0;
    int step;
    while ((step = x4_json_item(root, &cursor, &item)) == 1) {
        if (x4_json_type(item) != X4_JSON_T_OBJECT) return -1;
        int title = x4_json_member(item, "title", &value);
        if (title < 0) return -1;
        if (title == 1) {
            if (found_metadata || x4_json_member(item, "siglId", &value) != 1) return -1;
            /* Re-read title after checking the metadata marker. */
            if (x4_json_member(item, "title", &value) != 1 ||
                x4_json_text(value, out->name, sizeof(out->name)) < 0 ||
                !out->name[0]) return -1;
            found_metadata = 1;
            continue;
        }
        if (x4_json_member(item, "id", &value) != 1) return -1;
        char id[32];
        if (x4_json_token(value, id, sizeof(id)) < 0 || !valid_id(id)) return -1;
        ++out->source_count;
        if (out->count == X4_PSB_CURATED_WINDOW) continue;
        const char *verified = NULL;
        for (unsigned i = 0; i < catalog->count; ++i)
            if (same_id(catalog->titles[i].product_id, id)) {
                verified = catalog->titles[i].product_id;
                break;
            }
        if (!verified) continue;
        int duplicate = 0;
        for (size_t i = 0; i < out->count; ++i)
            if (same_id(out->product_ids[i], verified)) { duplicate = 1; break; }
        if (duplicate) continue;
        memcpy(out->product_ids[out->count++], verified, strlen(verified) + 1);
    }
    return step == 0 && found_metadata ? 0 : -1;
}
