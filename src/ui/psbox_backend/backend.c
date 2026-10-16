/* SPDX-License-Identifier: GPL-3.0-only */
#include "metadata.h"
#include "curated.h"
#include <orbis/libkernel.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct X4PsboxBackend {
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    _Atomic int cancel;
    int stopping;
    uint64_t signature, generation;
    X4CatalogSnapshot *pending, *active; /* active belongs to worker */
    size_t next;
    char detail[32];
    X4PsboxMetadata batches[2][X4_PSB_METADATA_LIGHT_BATCH];
    size_t batch_counts[2], batch_head, batch_count;
    X4PsboxCuratedRow rows[4];
    size_t row_count, sigl_next;
    int rows_ready;
};

static uint64_t catalog_signature(const X4CatalogSnapshot *catalog)
{
    uint64_t hash = 1469598103934665603ull;
    for (unsigned i = 0; i < catalog->count; ++i) {
        const X4CatalogTitle *title = &catalog->titles[i];
        for (size_t k = 0; k < sizeof(title->product_id) && title->product_id[k]; ++k)
            hash = (hash ^ (unsigned char)title->product_id[k]) * 1099511628211ull;
        hash = (hash ^ 0xffu) * 1099511628211ull;
        for (size_t k = 0; k < sizeof(title->id) && title->id[k]; ++k)
            hash = (hash ^ (unsigned char)title->id[k]) * 1099511628211ull;
        hash = (hash ^ 0xfeu) * 1099511628211ull;
    }
    hash = (hash ^ catalog->count) * 1099511628211ull;
    return (hash ^ catalog->total) * 1099511628211ull;
}

static size_t product_index(const X4CatalogSnapshot *catalog, const char *id)
{
    if (!catalog || !id) return 0;
    for (size_t i = 0; i < catalog->count; ++i)
        if (!strcmp(catalog->titles[i].product_id, id)) return i;
    return catalog->count;
}

/* Reorder only the worker's private copy. The UI's verified snapshot retains
 * service indexes/titleIds; this brings visible curated art into early batches. */
static void prioritize_curated(X4CatalogSnapshot *catalog,
    const X4PsboxCuratedRow *rows, size_t row_count)
{
    X4CatalogTitle *ordered = malloc(catalog->count * sizeof(*ordered));
    if (!ordered) return;
    bool used[X4_CATALOG_MAX] = {0};
    size_t count = 0;
    for (size_t r = 0; r < row_count; ++r)
        for (size_t j = 0; j < rows[r].count; ++j) {
            size_t i = product_index(catalog, rows[r].product_ids[j]);
            if (i < catalog->count && !used[i]) {
                ordered[count++] = catalog->titles[i];
                used[i] = true;
            }
        }
    for (size_t i = 0; i < catalog->count; ++i)
        if (!used[i]) ordered[count++] = catalog->titles[i];
    if (count == catalog->count)
        memcpy(catalog->titles, ordered, count * sizeof(*ordered));
    free(ordered);
}

static void *metadata_worker(void *opaque)
{
    X4PsboxBackend *b = opaque;
    X4Http *http = NULL;
    X4PsboxMetadata *result = calloc(X4_PSB_METADATA_LIGHT_BATCH, sizeof(*result));
    char *sigl_body = malloc(X4_HTTP_RESPONSE_MAX + 1u);
    X4PsboxCuratedRow *sigl_row = malloc(sizeof(*sigl_row));
    if (!result || !sigl_body || !sigl_row) {
        free(result); free(sigl_body); free(sigl_row); return NULL;
    }
    unsigned failures = 0;
    size_t light_limit = X4_PSB_METADATA_LIGHT_BATCH;
    for (;;) {
        pthread_mutex_lock(&b->mutex);
        while (!b->stopping && !b->pending &&
               (!b->active || (b->sigl_next >= X4_SIGL_COUNT &&
                (b->batch_count == 2 ||
                 (!b->detail[0] && b->next >= b->active->count)))))
            pthread_cond_wait(&b->changed, &b->mutex);
        if (b->stopping) { pthread_mutex_unlock(&b->mutex); break; }
        if (b->pending) {
            free(b->active);
            b->active = b->pending;
            b->pending = NULL;
            b->next = 0;
            b->sigl_next = 0;
            b->row_count = 0;
            b->rows_ready = 0;
            failures = 0;
            light_limit = X4_PSB_METADATA_LIGHT_BATCH;
            atomic_store(&b->cancel, 0);
        }
        X4CatalogSnapshot *catalog = b->active;
        uint64_t generation = b->generation;
        int sigl = b->sigl_next < X4_SIGL_COUNT ? (int)b->sigl_next : -1;
        size_t start = b->next;
        int detailed = sigl < 0 && b->detail[0] != 0;
        char selected_detail[32] = {0};
        if (detailed) {
            memcpy(selected_detail, b->detail, sizeof(selected_detail));
            start = product_index(catalog, b->detail);
            b->detail[0] = 0;
        }
        pthread_mutex_unlock(&b->mutex);
        if (!catalog || (sigl < 0 && start >= catalog->count)) continue;
        if (!http) {
            int error = 0;
            http = x4_http_open(&error);
            if (!http) {
                printf("Nexo X4: PSBox metadata HTTPS inicio error=0x%08x\n", (unsigned)error);
                if (detailed) {
                    pthread_mutex_lock(&b->mutex);
                    if (b->generation == generation && !b->detail[0])
                        memcpy(b->detail, selected_detail, sizeof(b->detail));
                    pthread_mutex_unlock(&b->mutex);
                }
                sceKernelUsleep(1000000);
                continue;
            }
        }
        size_t next = start, count = 0;
        int status = 0;
        int rc;
        if (sigl >= 0) {
            size_t length = 0;
            rc = x4_http_gamepass_sigl(http, (enum X4GamepassSigl)sigl, sigl_body,
                X4_HTTP_RESPONSE_MAX + 1u, &length, &status, &b->cancel);
            if (!rc && status == 200 &&
                x4_psbox_curated_parse(sigl_body, length, catalog, sigl_row))
                rc = X4_HTTP_PROTOCOL;
        } else {
            rc = x4_psbox_metadata_batch(http, catalog, start, detailed, result,
                detailed ? 1u : light_limit, &next, &count, &status, &b->cancel);
        }
        if (!x4_http_can_retry(http)) { x4_http_close(http); http = NULL; }
        bool pause_retry = false;
        pthread_mutex_lock(&b->mutex);
        if (b->generation == generation && !b->stopping && !atomic_load(&b->cancel)) {
            if (sigl >= 0) {
                if (!rc && status == 200 && b->row_count < 4 && sigl_row->count) {
                    memcpy(&b->rows[b->row_count++], sigl_row, sizeof(*sigl_row));
                    b->rows_ready = 1;
                }
                else if (rc != X4_HTTP_CANCELLED)
                    printf("Nexo X4: PSBox SIGL lista=%d error=0x%08x http=%d\n",
                        sigl, (unsigned)rc, status);
                b->sigl_next = (size_t)sigl + 1;
                if (b->sigl_next == X4_SIGL_COUNT && b->row_count)
                    prioritize_curated(b->active, b->rows, b->row_count);
                pthread_mutex_unlock(&b->mutex);
                continue;
            }
            bool shrink = rc == X4_HTTP_BOUND && !detailed && light_limit > 1;
            if (shrink) { light_limit = (light_limit + 1) / 2; failures = 0; }
            bool transient = rc && (status == 0 || status >= 500);
            if (shrink) transient = false;
            if (transient && ++failures <= 3) pause_retry = true;
            else if (!shrink) {
                failures = 0;
                if (!detailed) b->next = next;
            }
            if (pause_retry && detailed && !b->detail[0])
                memcpy(b->detail, selected_detail, sizeof(b->detail));
            if (rc == 0 && count && b->batch_count < 2) {
                size_t slot = (b->batch_head + b->batch_count) % 2;
                memcpy(b->batches[slot], result, count * sizeof(*result));
                b->batch_counts[slot] = count;
                ++b->batch_count;
            } else if (rc && rc != X4_HTTP_CANCELLED) {
                printf("Nexo X4: PSBox metadata error=0x%08x http=%d detail=%d\n",
                    (unsigned)rc, status, detailed);
            }
        }
        pthread_mutex_unlock(&b->mutex);
        if (pause_retry) sceKernelUsleep(2000000);
    }
    x4_http_close(http);
    free(result);
    free(sigl_body);
    free(sigl_row);
    return NULL;
}

X4PsboxBackend *x4_psbox_backend_create(void)
{
    X4PsboxBackend *b = calloc(1, sizeof(*b));
    if (!b) return NULL;
    if (pthread_mutex_init(&b->mutex, NULL) != 0) { free(b); return NULL; }
    if (pthread_cond_init(&b->changed, NULL) != 0) {
        pthread_mutex_destroy(&b->mutex); free(b); return NULL;
    }
    atomic_init(&b->cancel, 0);
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        pthread_cond_destroy(&b->changed); pthread_mutex_destroy(&b->mutex); free(b); return NULL;
    }
    int rc = pthread_attr_setstacksize(&attr, 2u << 20);
    if (!rc) rc = pthread_create(&b->thread, &attr, metadata_worker, b);
    pthread_attr_destroy(&attr);
    if (rc) {
        pthread_cond_destroy(&b->changed); pthread_mutex_destroy(&b->mutex); free(b); return NULL;
    }
    return b;
}

void x4_psbox_backend_destroy(X4PsboxBackend *b)
{
    if (!b) return;
    pthread_mutex_lock(&b->mutex);
    b->stopping = 1;
    atomic_store(&b->cancel, 1);
    pthread_cond_broadcast(&b->changed);
    pthread_mutex_unlock(&b->mutex);
    pthread_join(b->thread, NULL);
    free(b->pending);
    free(b->active);
    pthread_cond_destroy(&b->changed);
    pthread_mutex_destroy(&b->mutex);
    free(b);
}

int x4_psbox_backend_set_catalog(X4PsboxBackend *b, const X4CatalogSnapshot *catalog)
{
    if (!b || !catalog || catalog->state != X4_CATALOG_READY || catalog->truncated ||
        catalog->count > X4_CATALOG_MAX) return -1;
    uint64_t signature = catalog_signature(catalog);
    pthread_mutex_lock(&b->mutex);
    if (b->signature == signature && (b->active || b->pending)) {
        pthread_mutex_unlock(&b->mutex);
        return 0;
    }
    pthread_mutex_unlock(&b->mutex);
    X4CatalogSnapshot *copy = malloc(sizeof(*copy));
    if (!copy) return -1;
    memcpy(copy, catalog, sizeof(*copy));
    pthread_mutex_lock(&b->mutex);
    if (b->stopping) { pthread_mutex_unlock(&b->mutex); free(copy); return -1; }
    if (b->signature == signature && (b->active || b->pending)) {
        pthread_mutex_unlock(&b->mutex); free(copy); return 0;
    }
    free(b->pending);
    b->pending = copy;
    b->signature = signature;
    ++b->generation;
    b->batch_head = b->batch_count = 0;
    b->row_count = b->rows_ready = 0;
    b->sigl_next = 0;
    b->detail[0] = 0;
    atomic_store(&b->cancel, 1);
    pthread_cond_signal(&b->changed);
    pthread_mutex_unlock(&b->mutex);
    return 1;
}

int x4_psbox_backend_request_detail(X4PsboxBackend *b, const char *product_id)
{
    if (!b || !product_id || !memchr(product_id, 0, 32)) return -1;
    pthread_mutex_lock(&b->mutex);
    const X4CatalogSnapshot *catalog = b->pending ? b->pending : b->active;
    int valid = !b->stopping && catalog && product_index(catalog, product_id) < catalog->count;
    if (valid) {
        memcpy(b->detail, product_id, strlen(product_id) + 1);
        pthread_cond_signal(&b->changed);
    }
    pthread_mutex_unlock(&b->mutex);
    return valid ? 0 : -1;
}

int x4_psbox_backend_take_metadata(X4PsboxBackend *b, X4PsboxMetadata *out,
    size_t capacity, size_t *count)
{
    if (!b || !out || capacity < X4_PSB_METADATA_LIGHT_BATCH || !count) return -1;
    *count = 0;
    pthread_mutex_lock(&b->mutex);
    if (!b->batch_count) { pthread_mutex_unlock(&b->mutex); return 0; }
    size_t slot = b->batch_head;
    *count = b->batch_counts[slot];
    memcpy(out, b->batches[slot], *count * sizeof(*out));
    b->batch_head = (b->batch_head + 1) % 2;
    --b->batch_count;
    pthread_cond_signal(&b->changed);
    pthread_mutex_unlock(&b->mutex);
    return 1;
}

int x4_psbox_backend_take_rows(X4PsboxBackend *b, X4PsboxCuratedRow *out,
    size_t capacity, size_t *count)
{
    if (!b || !out || capacity < X4_PSB_CURATED_MAX_ROWS || !count) return -1;
    *count = 0;
    pthread_mutex_lock(&b->mutex);
    if (!b->rows_ready) { pthread_mutex_unlock(&b->mutex); return 0; }
    *count = b->row_count;
    memcpy(out, b->rows, *count * sizeof(*out));
    b->rows_ready = 0;
    pthread_mutex_unlock(&b->mutex);
    return 1;
}
