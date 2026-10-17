#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Only public OpenOrbis auxiliary modules; no console dump is used.
source "$(dirname "$0")/common.sh"
source_root="$NEXO_ROOT/vendor/openorbis-runtime"
python3 - "$source_root" <<'CHECK'
import hashlib,json,pathlib,sys
root=pathlib.Path(sys.argv[1]); info=json.loads((root/'SOURCE.json').read_text())
for name,expected in info['files_sha256'].items():
    if hashlib.sha256((root/name).read_bytes()).hexdigest()!=expected:
        raise SystemExit('OpenOrbis runtime source differs from its recorded snapshot: '+name)
CHECK
mkdir -p "$X4_RUNTIME_MODULES/.build"
for module in libc libSceFios2; do
    [[ -d "$source_root/src/modules/$module" ]] || { echo 'Public module sources missing.' >&2; exit 1; }
    # Build app-local copies; never modify a shared SDK source tree.
    mkdir -p "$X4_RUNTIME_MODULES/.build/$module"
    cp -R "$source_root/src/modules/$module/." "$X4_RUNTIME_MODULES/.build/$module/"
    make -B -C "$X4_RUNTIME_MODULES/.build/$module" -j"${JOBS:-4}"
    cp "$X4_RUNTIME_MODULES/.build/$module/$module.prx" "$X4_RUNTIME_MODULES/"
done
