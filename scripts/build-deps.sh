#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Build the complete, already-patched source bundle. Never reapply baseline patches.
source "$(dirname "$0")/common.sh"
: "${SOURCE_ARCHIVE:?Set SOURCE_ARCHIVE to the matching Nexo X4 dependency source archive.}"
[[ -f "$OO_PS4_TOOLCHAIN/link.x" ]] || { echo 'OpenOrbis toolchain missing.' >&2; exit 1; }
digest=$(sha256sum "$SOURCE_ARCHIVE" | cut -d' ' -f1)
expected=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["sha256"])' "$NEXO_ROOT/dependencies.lock.json")
[[ "$digest" == "$expected" ]] || { echo 'Source bundle differs from dependencies.lock.json' >&2; exit 1; }
if [[ -f "$X4_WEBRTC_ROOT/.nexo-source-sha256" ]]; then
    [[ $(cat "$X4_WEBRTC_ROOT/.nexo-source-sha256") == "$digest" ]] || { echo 'Dependency root contains a different source bundle; use a fresh directory.' >&2; exit 1; }
else
    python3 "$NEXO_ROOT/scripts/unpack-deps.py" "$SOURCE_ARCHIVE" "$X4_WEBRTC_ROOT"
    printf '%s\n' "$digest" > "$X4_WEBRTC_ROOT/.nexo-source-sha256"
fi
mkdir -p "$X4_WEBRTC_PREFIX/include" "$X4_WEBRTC_PREFIX/lib"
cp "$NEXO_ROOT/scripts/webrtc/openorbis.cmake" "$NEXO_ROOT/scripts/webrtc/mbedtls-user-config.h" "$X4_WEBRTC_ROOT/"
maps="-ffile-prefix-map=$X4_WEBRTC_ROOT=/nexo-x4/deps -ffile-prefix-map=$OO_PS4_TOOLCHAIN=/openorbis"
cflags="-fPIC -D_BSD_SOURCE -DX4_OPENORBIS -I$X4_WEBRTC_ROOT/overlay -isystem $OO_PS4_TOOLCHAIN/include $maps"
cxxflags="$cflags -nostdinc++ -isystem $OO_PS4_TOOLCHAIN/include/c++/v1"
cmake -S "$X4_WEBRTC_ROOT/mbedtls" -B "$X4_WEBRTC_ROOT/build-mbedtls" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$X4_WEBRTC_ROOT/openorbis.cmake" -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF -DUSE_SHARED_MBEDTLS_LIBRARY=OFF \
    -DCMAKE_INSTALL_PREFIX="$X4_WEBRTC_PREFIX" -DMBEDTLS_USER_CONFIG_FILE="$X4_WEBRTC_ROOT/mbedtls-user-config.h" \
    -DCMAKE_C_FLAGS="$cflags"
cmake --build "$X4_WEBRTC_ROOT/build-mbedtls" -j"${JOBS:-4}"
cmake --install "$X4_WEBRTC_ROOT/build-mbedtls"
cp "$X4_WEBRTC_ROOT/mbedtls-user-config.h" "$X4_WEBRTC_PREFIX/include/x4-mbedtls-user-config.h"
if ! grep -Fq '#include "../x4-mbedtls-user-config.h"' "$X4_WEBRTC_PREFIX/include/mbedtls/mbedtls_config.h"; then
    printf '\n#include "../x4-mbedtls-user-config.h"\n' >> "$X4_WEBRTC_PREFIX/include/mbedtls/mbedtls_config.h"
fi
dnsflags='-Dgetaddrinfo=x4_native_getaddrinfo -Dfreeaddrinfo=x4_native_freeaddrinfo -Dgetnameinfo=x4_native_getnameinfo -Dgetifaddrs=x4_native_getifaddrs -Dfreeifaddrs=x4_native_freeifaddrs'
cmake -S "$X4_WEBRTC_ROOT/libdatachannel" -B "$X4_WEBRTC_ROOT/build-datachannel" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$X4_WEBRTC_ROOT/openorbis.cmake" -DCMAKE_BUILD_TYPE=Release \
    -DUSE_MBEDTLS=ON -DNO_TESTS=ON -DNO_EXAMPLES=ON -DNO_WEBSOCKET=ON -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_SHARED_DEPS_LIBS=OFF -DLIBSRTP_TEST_APPS=OFF -Dsctp_inet=OFF -Dsctp_inet6=OFF \
    -Dsctp_build_programs=OFF -Dsctp_debug=OFF -Dsctp_werror=OFF \
    -DCMAKE_INSTALL_PREFIX="$X4_WEBRTC_PREFIX" -DCMAKE_FIND_ROOT_PATH="$OO_PS4_TOOLCHAIN;$X4_WEBRTC_PREFIX" \
    -DMbedTLS_INCLUDE_DIR="$X4_WEBRTC_PREFIX/include" -DMbedCrypto_LIBRARY="$X4_WEBRTC_PREFIX/lib/libmbedcrypto.a" \
    -DMbedTLS_LIBRARY="$X4_WEBRTC_PREFIX/lib/libmbedtls.a" -DMbedX509_LIBRARY="$X4_WEBRTC_PREFIX/lib/libmbedx509.a" \
    -DCMAKE_C_FLAGS="$cflags $dnsflags" -DCMAKE_CXX_FLAGS="$cxxflags $dnsflags"
cmake --build "$X4_WEBRTC_ROOT/build-datachannel" --target datachannel-static -j"${JOBS:-4}"
cp -R "$X4_WEBRTC_ROOT/libdatachannel/include/rtc" "$X4_WEBRTC_PREFIX/include/"
cmake -S "$X4_WEBRTC_ROOT/opus-1.5.2" -B "$X4_WEBRTC_ROOT/build-opus" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$X4_WEBRTC_ROOT/openorbis.cmake" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$X4_WEBRTC_PREFIX" -DCMAKE_C_FLAGS="$cflags" \
    -DOPUS_BUILD_PROGRAMS=OFF -DOPUS_BUILD_TESTING=OFF -DOPUS_INSTALL_PKG_CONFIG_MODULE=OFF -DOPUS_INSTALL_CMAKE_CONFIG_MODULE=OFF
cmake --build "$X4_WEBRTC_ROOT/build-opus" -j"${JOBS:-4}"
cmake --install "$X4_WEBRTC_ROOT/build-opus"
cmake -S "$X4_WEBRTC_ROOT/zlib" -B "$X4_WEBRTC_ROOT/build-zlib" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$X4_WEBRTC_ROOT/openorbis.cmake" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="$cflags" -DZLIB_BUILD_EXAMPLES=OFF
cmake --build "$X4_WEBRTC_ROOT/build-zlib" --target zlibstatic -j"${JOBS:-4}"
cp "$X4_WEBRTC_ROOT/build-zlib/libz.a" "$X4_WEBRTC_PREFIX/lib/"
cp "$X4_WEBRTC_ROOT/zlib/zlib.h" "$X4_WEBRTC_ROOT/build-zlib/zconf.h" "$X4_WEBRTC_PREFIX/include/"
printf 'Patched static dependencies built successfully.\n'
