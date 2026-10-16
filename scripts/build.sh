#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
source "$(dirname "$0")/common.sh"
cd "$NEXO_ROOT"
[[ $(uname -m) == x86_64 ]] || { echo 'Use Linux x86_64 (an amd64 VM is supported).' >&2; exit 1; }
[[ -f "$OO_PS4_TOOLCHAIN/link.x" ]] || { echo 'OpenOrbis toolchain missing; see docs/BUILDING.md.' >&2; exit 1; }
python3 - <<'PY'
import pathlib,re
r=pathlib.Path('.')
v=(r/'VERSION').read_text().strip()
assert re.fullmatch(r'\d+\.\d+\.\d+',v), 'Invalid VERSION'
assert f'#define X4_PRODUCT_VERSION "{v}"' in (r/'src/core/build_identity.h').read_text(), 'Version header mismatch'
PY
make -j"${JOBS:-4}" package X4_RECEIVER_NACK=1 X4_NACK_GRACE_MS=0 X4_RUMBLE=0 X4_DEV_CONTROL=0 X4_PSBOX_UI=1
