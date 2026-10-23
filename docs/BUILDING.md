# Building Nexo X4

This guide describes v0.1.9 (`APP_VER` `01.41`). The output is `dist/Nexo-X4-v0.1.9-ps4.pkg`. Build scripts are run from the repository root.

## Host and inputs

- A Linux x86-64 environment with Clang 18 or newer, CMake, Ninja, Python 3, `make`, `tar`, and the tools required by OpenOrbis packaging. Portable host checks compile x86-64 SSE2 code (`-msse2`). The target code uses C23 and C++17.
- The pinned OpenOrbis PS4 Toolchain, v0.5.4, exposed through `OO_PS4_TOOLCHAIN`. `scripts/bootstrap-toolchain.sh` is an optional way to obtain and verify that pinned public toolchain; an existing installation can be used instead. The SDK is a local build input and is not committed to Git.
- The **complete patched dependency source archive** matching this release, passed as `SOURCE_ARCHIVE` to `scripts/build-deps.sh`. It must include the pinned library trees, including zlib, applied PS4 port modifications, submodules, build configuration, overlays, source notices, and licenses. Do not substitute a bare upstream checkout for this archive.
- OpenOrbis-built `libSceFios2.prx` and `libc.prx` in one directory exposed through `X4_RUNTIME_MODULES`. They are generated from the public OpenOrbis source supplement in `vendor/openorbis-runtime` and are required at package time.

On Apple Silicon, use a Linux x86-64 guest, for example an OrbStack amd64 VM under Rosetta, and run the commands inside that guest. Keep the repository and build inputs in a guest-local directory without spaces for compatibility with upstream OpenOrbis Makefiles. The ignored `.local/` directory can hold dependency source, builds, the toolchain, and runtime modules. It is not part of the distributed application source.

## Obtain isolated build inputs

Install Linux host packages providing Clang/LLD 18+, CMake, Ninja, Python 3.12+, Make, Git, curl, and dpkg-deb. GitHub CLI can download this public repository's release assets. Python 3.12+ is required for the safe dependency-archive extractor.

With GitHub CLI:

```sh
mkdir -p .local/downloads
gh release download v0.1.9 --repo giruai/nexo-x4 \
  --pattern 'Nexo-X4-v0.1.0-dependency-sources.tar.gz' --dir .local/downloads
./scripts/bootstrap-toolchain.sh
./scripts/bootstrap-packager.sh
./scripts/prepare-runtime.sh
SOURCE_ARCHIVE="$PWD/.local/downloads/Nexo-X4-v0.1.0-dependency-sources.tar.gz" \
  ./scripts/build-deps.sh
./scripts/test-host.sh
./scripts/build.sh
```

The archive is verified against `dependencies.lock.json` before extraction. The toolchain bootstrap verifies the pinned official archive. The optional packager bootstrap extracts its verified legacy OpenSSL dependency into `.local/hostlibs`; it does not install it system-wide. `prepare-runtime.sh` verifies the public source supplement and builds app-local copies with the pinned SDK, without reading a console or writing into shared SDK sources.

## Build with existing inputs

```sh
export OO_PS4_TOOLCHAIN="$PWD/.local/toolchain/OpenOrbis/PS4Toolchain"
export X4_WEBRTC_ROOT="$PWD/.local/deps"
export X4_WEBRTC_PREFIX="$X4_WEBRTC_ROOT/prefix"
export X4_RUNTIME_MODULES="$PWD/.local/runtime"

SOURCE_ARCHIVE="$PWD/.local/downloads/Nexo-X4-v0.1.0-dependency-sources.tar.gz" \
  ./scripts/build-deps.sh
./scripts/test-host.sh
./scripts/build.sh
```

Adjust the `.local` paths if your inputs are elsewhere. `build-deps.sh` prepares the patched, pinned libraries in `X4_WEBRTC_ROOT` and installs their static outputs under `X4_WEBRTC_PREFIX`. `test-host.sh` runs portable host checks. `build.sh` compiles and packages the release with the configured SDK and runtime modules. Treat any missing required input or mismatched source revision as a build failure; do not use an older prebuilt dependency library with this release.

The release configuration is diagnostic mailbox **off**, receiver NACK **on**, NACK grace **0 ms**, and rumble **off**. The package uses title ID `XCLD00001` for migration compatibility while its displayed product name is Nexo X4. Verify the generated package name and `APP_VER` before sharing it.

## Distribution

If you distribute the PKG, provide the exact corresponding Nexo X4 application source and the complete patched dependency source bundle used to build it, together with all retained notices and licenses. The export contains modified MPL-2.0 components, original upstream headers, port scripts, and overlays; the archive must match the binaries actually distributed. See [Third-party notices](../THIRD_PARTY_NOTICES.md) and the [license inventory](licenses/README.md).

The optional public toolchain download is separate from the source bundle. Do not add local SDK installations, generated packages, account state, or diagnostic logs to the release source archive.

The dependency source bundle retains pinned upstream test certificates, sample signing keys, and test vectors. These are already-public fixtures, are not account or console credentials, and are not packaged as client secrets.

## Profiles and diagnostics

`make` builds numeric input instrumentation with `X4_INPUT_TRACE=1` by default; its runtime switch in Options defaults off. `X4_INPUT_TRACE=0` can compile out the trigger probes. The switch does not enable diagnostic mailbox commands or capture text/account data. Build/version names come from `VERSION`, `APP_VERSION` and `src/core/build_identity.h`; keep them consistent. The 0.1.9 release passed portable tests, native compilation and PS4 hardware checks for 720p recovery and actual 1080p playback. See [Validation](VALIDATION.md) for the measured cadence and its limits. The distributed PKG is the hardware-tested binary. Post-test release documentation changes do not change its executable; [the build record](releases/0.1.9-build.json) records the original input hashes and tested PKG hash.
