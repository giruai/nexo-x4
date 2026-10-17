# OpenOrbis auxiliary runtime source supplement

These unmodified public sources are from OpenOrbis PS4 Toolchain v0.5.4, whose pinned toolchain archive is identified in `SOURCE.json`. They supplement the sources of the `libc.prx` and `libSceFios2.prx` wrappers included in Nexo X4 packages. They are not Sony SDK files or console dumps. Original GPL-3.0 licensing is retained in `LICENSE`.

The module sources and Makefiles are byte-identical to those in the pinned SDK used for the release build. The public `crtlib` sources, CRT build recipe and linker script are retained as well; module Makefiles use the CRT object, headers and import stubs supplied by that SDK. This is a source supplement, not a replacement SDK.

Run `scripts/bootstrap-toolchain.sh` and `scripts/prepare-runtime.sh` from the Nexo root. The latter builds these module sources in an app-local runtime build directory using the pinned external OpenOrbis toolchain. No proprietary module is downloaded from a console.

The SDK also supplies compiler runtime archives (libc++/libc++abi/unwind); their retained notices and the limit on identifying their exact prebuilt revision are described in `THIRD_PARTY_NOTICES.md` at the application root. This supplement does not misidentify those archives as being rebuilt from the CRT sources here.

`SDK-ARCHIVES.json` records the hashes of the SDK runtime archives and CRT objects used for this build. It identifies the binary inputs without pretending to establish their upstream source revisions. Musl and compiler-rt notices are retained in the application license inventory.
