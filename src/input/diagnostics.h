/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* User-controlled numeric diagnostics, default off; no credentials or text. */
bool x4_input_diagnostics_enabled(void);
void x4_input_diagnostics_set(bool enabled);
#ifdef __cplusplus
}
#endif
