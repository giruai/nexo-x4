/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "metadata.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Main-thread preference update. 1 means changed, 0 unchanged, -1 invalid or
 * allocation failure. A change cancels old public metadata/SIGL work, empties
 * completed batches, and restarts from the same verified catalog snapshot.
 * It never changes account, entitlement, game session, or Store-name requests.
 * Clear the UI's old metadata/curated rows when this returns 1. */
int x4_psbox_backend_set_language(X4PsboxBackend *backend, int language);
uint64_t x4_psbox_backend_language_revision(X4PsboxBackend *backend);

#ifdef __cplusplus
}
#endif
