#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
source "$(dirname "$0")/common.sh"
archive="$NEXO_ROOT/.local/downloads/toolchain-llvm-18.tar.gz"
mkdir -p "$(dirname "$archive")" "$NEXO_ROOT/.local/toolchain"
expected=3c7cd5bb593ca74fa1c13fd59f3938dc0fc07985167f7275063019e63abe4526
if [[ ! -f "$archive" ]]; then
    curl --fail --location --proto '=https' --tlsv1.2 -o "$archive.part" \
        https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/releases/download/v0.5.4/toolchain-llvm-18.tar.gz
    printf '%s  %s\n' "$expected" "$archive.part" | sha256sum -c -
    mv "$archive.part" "$archive"
fi
printf '%s  %s\n' "$expected" "$archive" | sha256sum -c -
[[ -f "$OO_PS4_TOOLCHAIN/link.x" ]] || tar -xf "$archive" -C "$NEXO_ROOT/.local/toolchain"
printf 'OpenOrbis v0.5.4 ready. Run scripts/build-deps.sh next.\n'
