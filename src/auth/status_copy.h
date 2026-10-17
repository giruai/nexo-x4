/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PSBox language order: English, pt-BR, Spanish, French, German, Italian.
 * Translate only first-party status keys at the presentation boundary. The
 * backend retains its original keys for comparisons and state transitions. */
enum { X4_STATUS_EN, X4_STATUS_PT_BR, X4_STATUS_ES, X4_STATUS_FR,
       X4_STATUS_DE, X4_STATUS_IT, X4_STATUS_LANGUAGES };
void x4_status_format(char *dst, size_t capacity, const char *key, int language);
size_t x4_status_key_count(void);
const char *x4_status_key(size_t index);

#ifdef __cplusplus
}
#endif
