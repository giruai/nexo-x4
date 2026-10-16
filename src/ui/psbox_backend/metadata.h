/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "../../auth/http_client.h"
#include "../psbox/bridge.h"

#ifdef __cplusplus
extern "C" {
#endif

#define X4_PSB_METADATA_LIGHT_BATCH 60u
#define X4_PSB_METADATA_DETAIL_BATCH 20u

/* Worker-thread call. Only real ProductIds in the supplied READY catalog are
 * posted to the fixed public Game Pass endpoint. `next_index` advances across
 * skipped records too; publish `out` through the UI bridge on the main thread.
 * A detailed call may pass a snapshot with one selected ProductId. */
int x4_psbox_metadata_batch(X4Http *http, const X4CatalogSnapshot *catalog,
    size_t start_index, int detailed, X4PsboxMetadata *out, size_t out_capacity,
    size_t *next_index, size_t *out_count, int *http_status, const _Atomic int *cancel);

typedef struct X4PsboxBackend X4PsboxBackend;
/* The backend owns one network worker. Calls below are safe on the UI thread;
 * take_metadata never waits for HTTPS. Repeated identical READY snapshots do
 * not restart a fetch. New snapshots cancel and discard stale batches. */
X4PsboxBackend *x4_psbox_backend_create(void);
void x4_psbox_backend_destroy(X4PsboxBackend *backend);
int x4_psbox_backend_set_catalog(X4PsboxBackend *backend, const X4CatalogSnapshot *catalog);
int x4_psbox_backend_request_detail(X4PsboxBackend *backend, const char *product_id);
/* 1 means a batch was copied, 0 means no completed batch, negative is invalid
 * input. Caller supplies at least X4_PSB_METADATA_LIGHT_BATCH entries. */
int x4_psbox_backend_take_metadata(X4PsboxBackend *backend, X4PsboxMetadata *out,
    size_t capacity, size_t *count);
/* One completed set of official public rows, copied without waiting for HTTPS.
 * `out` has capacity for at least X4_PSB_CURATED_MAX_ROWS entries. */
int x4_psbox_backend_take_rows(X4PsboxBackend *backend, X4PsboxCuratedRow *out,
    size_t capacity, size_t *count);

#ifdef __cplusplus
}
#endif
