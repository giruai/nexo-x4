/* SPDX-License-Identifier: GPL-3.0-only */
#include "region_selection.h"
#include <stdio.h>
#include <string.h>

static int alnum_ascii(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

int x4_region_id_valid(const char *id)
{
    if (!id || !id[0]) return 1;
    if (!alnum_ascii((unsigned char)id[0])) return 0;
    size_t i = 0;
    for (; i < X4_REGION_ID_SIZE && id[i]; ++i)
        if (!alnum_ascii((unsigned char)id[i]) && id[i] != '-' && id[i] != '_' &&
            id[i] != '.' && id[i] != ' ') return 0;
    return i < X4_REGION_ID_SIZE && id[i - 1] != ' ';
}

int x4_region_origin(const char *base, char *origin, size_t capacity)
{
    static const char scheme[] = "https://", suffix[] = ".gssv-play-prod.xboxlive.com";
    char host[254];
    size_t n = 0, label = 0;
    if (!origin || !capacity) return -1;
    origin[0] = 0;
    if (!base || strncmp(base, scheme, sizeof(scheme) - 1)) return -1;
    const char *h = base + sizeof(scheme) - 1;
    while (n < sizeof(host) && h[n] && h[n] != '/') ++n;
    if (!n || n >= sizeof(host) || (h[n] == '/' && h[n + 1])) return -1;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)h[i];
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        if (c == '.') {
            if (!label || host[i - 1] == '-') return -1;
            label = 0;
        } else {
            if ((!alnum_ascii(c) && c != '-') || (!label && c == '-') || ++label > 63) return -1;
        }
        host[i] = (char)c;
    }
    if (!label || host[n - 1] == '-' || n <= sizeof(suffix) - 1 ||
        memcmp(host + n - (sizeof(suffix) - 1), suffix, sizeof(suffix) - 1)) return -1;
    host[n] = 0;
    int written = snprintf(origin, capacity, "%s%s", scheme, host);
    if (written < 0 || (size_t)written >= capacity) { origin[0] = 0; return -1; }
    return 0;
}

static int parse_region(X4JsonSpan item, X4Region *entry, char *origin, size_t capacity)
{
    X4JsonSpan v;
    char base[256], name[X4_REGION_ID_SIZE];
    memset(entry, 0, sizeof(*entry));
    if (x4_json_member(item, "baseUri", &v) != 1 || x4_json_token(v, base, sizeof(base)) ||
        x4_region_origin(base, origin, capacity)) return -1;
    int flag = x4_json_member(item, "isDefault", &v);
    if (flag < 0 || (flag == 1 && x4_json_bool(v, &entry->is_default))) return -1;
    if (x4_json_member(item, "name", &v) == 1 && !x4_json_token(v, name, sizeof(name)) &&
        name[0] && x4_region_id_valid(name)) strcpy(entry->id, name);
    else {
        const char *host = origin + strlen("https://");
        size_t n = strlen(host) - strlen(".gssv-play-prod.xboxlive.com");
        if (!n || n >= sizeof(entry->id)) return -1;
        memcpy(entry->id, host, n);
        entry->id[n] = 0;
        if (!x4_region_id_valid(entry->id)) return -1;
    }
    strcpy(entry->label, entry->id);
    return 0;
}

int x4_region_select(X4JsonSpan root, const char *preferred_id,
    X4RegionSnapshot *out, char *origin, size_t capacity)
{
    X4JsonSpan settings, regions, item;
    X4RegionSnapshot next = {0};
    X4Region def = {0}, preferred = {0};
    char default_origin[X4_REGION_ORIGIN_SIZE] = {0}, preferred_origin[X4_REGION_ORIGIN_SIZE] = {0};
    unsigned scanned = 0;
    size_t cursor = 0;
    if (!out || !origin || !capacity) return -1;
    memset(out, 0, sizeof(*out));
    origin[0] = 0;
    if (!x4_region_id_valid(preferred_id) ||
        x4_json_member(root, "offeringSettings", &settings) != 1 ||
        x4_json_member(settings, "regions", &regions) != 1 || x4_json_type(regions) != X4_JSON_T_ARRAY) return -1;
    if (preferred_id) strcpy(next.preferred_id, preferred_id);
    int rc;
    while ((rc = x4_json_item(regions, &cursor, &item)) == 1) {
        X4Region entry;
        char candidate[X4_REGION_ORIGIN_SIZE];
        if (++scanned > X4_REGION_SCAN_MAX) return -1;
        if (parse_region(item, &entry, candidate, sizeof(candidate))) continue;
        /* Ambiguous repeated IDs are excluded by keeping their first entry. */
        unsigned i = 0;
        for (; i < next.count; ++i) if (!strcmp(next.items[i].id, entry.id)) break;
        if (i < next.count || (def.id[0] && !strcmp(def.id, entry.id)) ||
            (preferred.id[0] && !strcmp(preferred.id, entry.id))) continue;
        if (entry.is_default && !def.id[0]) { def = entry; strcpy(default_origin, candidate); }
        if (next.preferred_id[0] && !strcmp(next.preferred_id, entry.id)) {
            preferred = entry; strcpy(preferred_origin, candidate);
        }
        if (next.count < X4_REGION_MAX) next.items[next.count++] = entry;
        else next.truncated = 1;
    }
    if (rc < 0) return -1;
    X4Region chosen = preferred.id[0] ? preferred : def;
    const char *chosen_origin = preferred.id[0] ? preferred_origin : default_origin;
    if (!chosen.id[0] || strlen(chosen_origin) >= capacity) return -1;
    next.fallback = next.preferred_id[0] && !preferred.id[0];
    strcpy(next.active_id, chosen.id);
    unsigned i = 0;
    for (; i < next.count; ++i) if (!strcmp(next.items[i].id, chosen.id)) break;
    if (i == next.count) {
        if (next.count < X4_REGION_MAX) next.items[next.count++] = chosen;
        else next.items[X4_REGION_MAX - 1] = chosen;
    }
    *out = next;
    strcpy(origin, chosen_origin);
    return 0;
}
