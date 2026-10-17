/* SPDX-License-Identifier: GPL-3.0-only */
#include "diagnostics.h"
#include <stdatomic.h>
static atomic_bool enabled;
bool x4_input_diagnostics_enabled(void) { return atomic_load_explicit(&enabled, memory_order_relaxed); }
void x4_input_diagnostics_set(bool value) { atomic_store_explicit(&enabled, value, memory_order_relaxed); }
