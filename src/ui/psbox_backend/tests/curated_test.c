/* SPDX-License-Identifier: GPL-3.0-only */
#include "../curated.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    X4CatalogSnapshot *catalog = calloc(1, sizeof(*catalog));
    X4PsboxCuratedRow *row = calloc(1, sizeof(*row));
    assert(catalog && row);
    catalog->state = X4_CATALOG_READY;
    catalog->count = 2;
    strcpy(catalog->titles[0].product_id, "9TEST0001");
    strcpy(catalog->titles[1].product_id, "9TEST0002");
    const char fixture[] = "[{\"siglId\":\"fixed\",\"title\":\"Agregados recientemente\"},"
        "{\"id\":\"9test0002\"},{\"id\":\"9ABSENT\"},"
        "{\"id\":\"9TEST0001\"},{\"id\":\"9test0002\"}]";
    assert(x4_psbox_curated_parse(fixture, strlen(fixture), catalog, row) == 0);
    assert(!strcmp(row->name, "Agregados recientemente"));
    assert(row->source_count == 4 && row->count == 2 && !row->truncated);
    assert(!strcmp(row->product_ids[0], "9TEST0002"));
    assert(!strcmp(row->product_ids[1], "9TEST0001"));
    catalog->count = 600;
    char *large = calloc(1, 32768);
    assert(large);
    size_t used = (size_t)snprintf(large, 32768,
        "[{\"siglId\":\"fixed\",\"title\":\"Mas populares\"}");
    for (unsigned i = 0; i < 600; ++i) {
        snprintf(catalog->titles[i].product_id, 32, "9TEST%04u", i);
        used += (size_t)snprintf(large + used, 32768 - used,
            ",{\"id\":\"9TEST%04u\"}", i);
    }
    used += (size_t)snprintf(large + used, 32768 - used, "]");
    assert(used < 32768);
    assert(x4_psbox_curated_parse(large, used, catalog, row) == 0);
    assert(row->source_count == 600 && row->count == 40 && !row->truncated);
    assert(!strcmp(row->product_ids[0], "9TEST0000"));
    assert(!strcmp(row->product_ids[39], "9TEST0039"));
    large[used - 2] = '!'; /* malformed tail still rejects the whole row */
    assert(x4_psbox_curated_parse(large, used, catalog, row) < 0);
    free(large);
    const char rogue[] = "[{\"siglId\":\"fixed\",\"title\":\"Public\"},{\"id\":\"bad/id\"}]";
    assert(x4_psbox_curated_parse(rogue, strlen(rogue), catalog, row) < 0);
    catalog->truncated = 1;
    assert(x4_psbox_curated_parse(fixture, strlen(fixture), catalog, row) < 0);
    free(row); free(catalog);
    puts("PSBox curated: PASS");
    return 0;
}
