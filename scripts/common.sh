#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
NEXO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
export X4_WEBRTC_ROOT="${X4_WEBRTC_ROOT:-$NEXO_ROOT/.local/deps}"
export X4_WEBRTC_PREFIX="${X4_WEBRTC_PREFIX:-$X4_WEBRTC_ROOT/prefix}"
export X4_RUNTIME_MODULES="${X4_RUNTIME_MODULES:-$NEXO_ROOT/.local/runtime}"
export X4_HOSTLIBS="${X4_HOSTLIBS:-$NEXO_ROOT/.local/hostlibs}"
export OO_PS4_TOOLCHAIN="${OO_PS4_TOOLCHAIN:-$NEXO_ROOT/.local/toolchain/OpenOrbis/PS4Toolchain}"
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
mkdir -p "$NEXO_ROOT/.local/bin"
for tool in clang clang++ ld.lld; do
    if command -v "$tool-18" >/dev/null; then
        ln -sfn "$(command -v "$tool-18")" "$NEXO_ROOT/.local/bin/$tool"
    fi
done
export PATH="$NEXO_ROOT/.local/bin:$OO_PS4_TOOLCHAIN/bin/linux:$PATH"
if [[ -d "$X4_HOSTLIBS/usr/lib/x86_64-linux-gnu" ]]; then
    export LD_LIBRARY_PATH="$X4_HOSTLIBS/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
