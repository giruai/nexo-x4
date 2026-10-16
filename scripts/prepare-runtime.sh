#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Only public OpenOrbis auxiliary modules; no console dump is used.
source "$(dirname "$0")/common.sh"
mkdir -p "$X4_RUNTIME_MODULES"
for module in libc libSceFios2; do
    [[ -d "$OO_PS4_TOOLCHAIN/src/modules/$module" ]] || { echo 'Public module sources missing from toolchain archive.' >&2; exit 1; }
    make -B -C "$OO_PS4_TOOLCHAIN/src/modules/$module" -j"${JOBS:-4}"
    cp "$OO_PS4_TOOLCHAIN/src/modules/$module/$module.prx" "$X4_RUNTIME_MODULES/"
done
