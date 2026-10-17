#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -Eeuo pipefail
cd "$(dirname "$0")/.."
: "${OO_PS4_TOOLCHAIN:?Set OO_PS4_TOOLCHAIN; see docs/BUILDING.md}"
TITLE='Nexo X4'
TITLE_ID='XCLD00001'
CONTENT_ID='IV0000-XCLD00001_00-XCLOUD4APP000000'
VERSION=$(cat APP_VERSION)
PACKAGE_VERSION=$(cat VERSION)
PACKAGE_SUFFIX=${X4_PACKAGE_SUFFIX:-}
case "$PACKAGE_SUFFIX" in ''|'-diagnostic') ;; *) echo 'Invalid package suffix' >&2; exit 1 ;; esac
TOOLS="$OO_PS4_TOOLCHAIN/bin/linux"
# The PS4 startup loader requires the auxiliary OpenOrbis modules before main().
# Keep compiled module binaries in an external local directory, never in Git.
: "${X4_RUNTIME_MODULES:?Define X4_RUNTIME_MODULES con los módulos locales de PS4}"
RUNTIME_FILES=(libSceFios2.prx libc.prx)
for module in "${RUNTIME_FILES[@]}"; do
    [[ -s "$X4_RUNTIME_MODULES/$module" ]] || { echo "Falta $module; no se genera un paquete incompleto." >&2; exit 1; }
done
python3 - "$X4_RUNTIME_MODULES" "${RUNTIME_FILES[@]}" <<'PY'
import pathlib,sys
for name in sys.argv[2:]:
    module=pathlib.Path(sys.argv[1])/name
    if module.read_bytes()[:4] != bytes.fromhex('4f153d1d'):
        raise SystemExit(f'{name} debe ser un módulo SELF. Un ELF obtenido por FTP requiere conversión previa.')
PY
# LibOrbisPkg is distributed as a self-contained .NET executable.
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
HOSTLIBS="${X4_HOSTLIBS:-$PWD/.local/hostlibs}"
if [[ -d "$HOSTLIBS/usr/lib/x86_64-linux-gnu" ]]; then
    export LD_LIBRARY_PATH="$HOSTLIBS/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
[[ -s assets/sample.h264 ]] || { echo 'Falta assets/sample.h264; ejecuta scripts/generar-muestra.sh.' >&2; exit 1; }
mkdir -p build/package/sce_sys build/package/sce_module build/package/assets dist
cp assets/sample.h264 build/package/assets/sample.h264
mkdir -p build/package/assets/psbox/fonts
cp assets/psbox/fonts/Inter-Regular.ttf assets/psbox/fonts/Inter-SemiBold.ttf assets/psbox/fonts/Inter-Bold.ttf assets/psbox/fonts/OFL.txt build/package/assets/psbox/fonts/
cp build/eboot.bin build/package/eboot.bin
for module in "${RUNTIME_FILES[@]}"; do
    cp "$X4_RUNTIME_MODULES/$module" "build/package/sce_module/$module"
done
cp assets/icon0.png build/package/sce_sys/icon0.png
cp LICENSE build/package/LICENSE
cp LICENSE build/package/LICENSE.txt
cp THIRD_PARTY_NOTICES.md build/package/THIRD_PARTY_NOTICES.md
cp CREDITS.md build/package/CREDITS.md
mkdir -p build/package/docs
cp docs/LEGAL.md docs/PRIVACY.md build/package/docs/
mkdir -p build/package/docs/licenses
LICENSE_FILES=(docs/licenses/*)
cp "${LICENSE_FILES[@]}" build/package/docs/licenses/
PACKAGE_FILES='eboot.bin assets/sample.h264 sce_module/libSceFios2.prx sce_module/libc.prx sce_sys/param.sfo sce_sys/icon0.png LICENSE LICENSE.txt THIRD_PARTY_NOTICES.md CREDITS.md'
PACKAGE_FILES+=' docs/LEGAL.md docs/PRIVACY.md'
PACKAGE_FILES+=' assets/psbox/fonts/Inter-Regular.ttf assets/psbox/fonts/Inter-SemiBold.ttf assets/psbox/fonts/Inter-Bold.ttf assets/psbox/fonts/OFL.txt'
for license in "${LICENSE_FILES[@]}"; do
    PACKAGE_FILES+=" docs/licenses/${license##*/}"
done
# Ship the small public runtime source supplement with its notices too.
mapfile -t RUNTIME_SOURCE_FILES < <(python3 - <<'RUNTIME'
import json
from pathlib import Path
base=Path('vendor/openorbis-runtime')
info=json.loads((base/'SOURCE.json').read_text())
for name in sorted(list(info['files_sha256'])+['README.md','SOURCE.json','SDK-ARCHIVES.json']):
    print(base/name)
RUNTIME
)
for source_file in "${RUNTIME_SOURCE_FILES[@]}"; do
    cp --parents "$source_file" build/package/
    PACKAGE_FILES+=" $source_file"
done
export X4_PACKAGE_FILES="$PACKAGE_FILES"
SFO='build/package/sce_sys/param.sfo'
"$TOOLS/PkgTool.Core" sfo_new "$SFO"
entry() { "$TOOLS/PkgTool.Core" sfo_setentry "$SFO" "$1" --type "$2" --maxsize "$3" --value "$4"; }
entry APP_TYPE Integer 4 1
entry APP_VER Utf8 8 "$VERSION"
entry ATTRIBUTE Integer 4 0
entry CATEGORY Utf8 4 gd
entry CONTENT_ID Utf8 48 "$CONTENT_ID"
entry DOWNLOAD_DATA_SIZE Integer 4 0
entry SYSTEM_VER Integer 4 0
entry TITLE Utf8 128 "$TITLE"
entry TITLE_ID Utf8 12 "$TITLE_ID"
entry VERSION Utf8 8 "$VERSION"
DIST="$PWD/dist"
(
    cd build/package
    "$TOOLS/create-gp4" -out nexo-x4.gp4 --content-id="$CONTENT_ID" \
        --files "$PACKAGE_FILES"
    # Declare exactly the directories needed by the selected package files.
    python3 - <<'PYGP4'
import os
from pathlib import PurePosixPath
import xml.etree.ElementTree as ET
project='nexo-x4.gp4'
tree=ET.parse(project); root=tree.find('.//rootdir')
if root is None:
    raise SystemExit('Expected a GP4 rootdir element.')
for child in list(root): root.remove(child)
for filename in sorted(os.environ['X4_PACKAGE_FILES'].split()):
    parent=root
    for component in PurePosixPath(filename).parts[:-1]:
        found=next((x for x in parent if x.get('targ_name')==component),None)
        if found is None: found=ET.SubElement(parent,'dir',{'targ_name':component})
        parent=found
tree.write(project,encoding='utf-8',xml_declaration=True)
PYGP4
    "$TOOLS/PkgTool.Core" pkg_build nexo-x4.gp4 "$DIST"
)
mv "dist/$CONTENT_ID.pkg" "dist/Nexo-X4-v$PACKAGE_VERSION-ps4$PACKAGE_SUFFIX.pkg"
sha256sum "dist/Nexo-X4-v$PACKAGE_VERSION-ps4$PACKAGE_SUFFIX.pkg" > "dist/Nexo-X4-v$PACKAGE_VERSION-ps4$PACKAGE_SUFFIX.pkg.sha256"
printf '\nPaquete: %s/dist/Nexo-X4-v%s-ps4%s.pkg\n' "$PWD" "$PACKAGE_VERSION" "$PACKAGE_SUFFIX"
