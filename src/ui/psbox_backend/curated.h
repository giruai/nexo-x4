/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "metadata.h"

/* Parse one official public SIGL against the verified Xbox catalog. No list
 * entry grants an entitlement or supplies a playable titleId. */
int x4_psbox_curated_parse(const char *json, size_t length,
    const X4CatalogSnapshot *catalog, X4PsboxCuratedRow *out);
