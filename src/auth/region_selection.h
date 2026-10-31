/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "json.h"
#include <stddef.h>

#define X4_REGION_MAX 32u
#define X4_REGION_ID_SIZE 80u
#define X4_REGION_ORIGIN_SIZE 256u
#define X4_REGION_SCAN_MAX 256u
/* Public, credential-free service selection. Empty preferred_id means Auto.
 * IDs are validated service names, or the service host prefix when unnamed.
 * Neither the public list nor saved preferences contains an endpoint. */
typedef struct {
    char id[X4_REGION_ID_SIZE], label[X4_REGION_ID_SIZE];
    int is_default;
} X4Region;
typedef struct {
    X4Region items[X4_REGION_MAX];
    unsigned count;
    int truncated, fallback;
    char preferred_id[X4_REGION_ID_SIZE], active_id[X4_REGION_ID_SIZE];
} X4RegionSnapshot;
#ifdef __cplusplus
extern "C" {
#endif
/* NULL and empty are Auto; bounded ASCII identifiers only. */
int x4_region_id_valid(const char *id);
/* Strict HTTPS GSSV origin only: no credentials, port, path, query, fragment,
 * empty labels, or invalid DNS labels. A final '/' is accepted. */
int x4_region_origin(const char *base, char *origin, size_t capacity);
/* Pure selection from this login's offeringSettings.regions. No network or
 * workspace mutation. All entries (at most SCAN_MAX) are examined; public
 * list truncation never hides the active entry. A missing preferred ID falls
 * back to the first valid isDefault and sets fallback. Auto requires default;
 * an available explicit ID does not. 0 succeeds; -1 clears outputs. */
int x4_region_select(X4JsonSpan root, const char *preferred_id,
    X4RegionSnapshot *out, char *origin, size_t capacity);
#ifdef __cplusplus
}
#endif
