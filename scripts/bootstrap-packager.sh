#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# Extract the legacy packager's OpenSSL dependency locally, without apt installation.
source "$(dirname "$0")/common.sh"
package="$NEXO_ROOT/.local/downloads/libssl1.1_amd64.deb"
expected=aadf8b4b197335645b230c2839b4517aa444fd2e8f434e5438c48a18857988f7
mkdir -p "$(dirname "$package")" "$X4_HOSTLIBS"
if [[ ! -f "$package" ]]; then
    curl --fail --location --proto '=https' --tlsv1.2 -o "$package.part" \
        https://deb.debian.org/debian/pool/main/o/openssl/libssl1.1_1.1.1w-0+deb11u1_amd64.deb
    printf '%s  %s\n' "$expected" "$package.part" | sha256sum -c -
    mv "$package.part" "$package"
fi
printf '%s  %s\n' "$expected" "$package" | sha256sum -c -
dpkg-deb -x "$package" "$X4_HOSTLIBS"
printf 'Legacy packager dependency is available in the local build directory.\n'
