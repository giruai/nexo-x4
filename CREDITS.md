# Credits

Nexo X4 builds on the work below. The distinction between incorporated source and technical references matters: a reference informed an interface or performance decision, but its implementation was not copied unless stated.

## Incorporated application and interface work

- **XCloud4**, by [asp11edgr](https://github.com/asp11edgr/XCloud4), is the original PS4 client and native application foundation. The Nexo X4 import is based on commit [`8eefb3a485867cde8af9a5c358eff6d80c8f95cc`](https://github.com/asp11edgr/XCloud4/commit/8eefb3a485867cde8af9a5c358eff6d80c8f95cc). Its source notices are retained.
- **PSBox**, by [RafaelNGP](https://github.com/RafaelNGP/xbox-cloud-gaming-ps5), supplies the adapted catalog interface, canvas, font and image rendering, localization, icons, accent, and input mapping. The import is based on commit [`41a6ea9f4624c4ea9410f25f2c9761de35e96501`](https://github.com/RafaelNGP/xbox-cloud-gaming-ps5/commit/41a6ea9f4624c4ea9410f25f2c9761de35e96501). Copied and adapted files retain their GPL-3.0-or-later headers in `src/ui/psbox/vendor`; the PS4 bridge and metadata backend integrate that interface with the native client.
- **OpenOrbis** provides the PS4 homebrew toolchain, examples, and auxiliary runtime modules. Public examples informed package, display, input, and audio setup. **Backporter's** [Videodec2 declarations](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/pull/213) informed the local ABI header. Their GPL-3.0 notices are retained. The [OpenOrbis runtime supplement](vendor/openorbis-runtime/README.md) retains the public sources for the packaged auxiliary modules.
- **LLVM libc++ 11** provides the unmodified `condition_variable.cpp` compiled into the application to match the native runtime's timeout value. Its Apache-2.0 WITH LLVM-exception notice is retained in the source and license directory.

## Bundled libraries and assets

Thanks to the authors and maintainers of libdatachannel, libjuice, usrsctp, libsrtp, Mbed TLS and its bundled Everest/p256-m code, Opus, zlib, nlohmann/json, plog, Project Nayuki's QR Code generator, Sean Barrett's stb libraries, and the Inter typeface. The exact licenses and source locations are in [Third-party notices](THIRD_PARTY_NOTICES.md); the original texts are in [docs/licenses](docs/licenses/README.md). The SDK-provided C runtime also incorporates musl and LLVM compiler-rt; their notices are retained. Some dependencies are linked from a separately supplied, patched source bundle rather than committed as vendor trees in this repository.

## Technical references

[OpenGNM's public examples](https://github.com/PS4-OpenGNM/freegnm-examples) informed GPU packet fields used by the original bounded readback implementation; its example notice is retained. [GreenVita](https://github.com/Day-OS/green-vita), [Better xCloud](https://github.com/redphx/better-xcloud), and [Moonlight PS4](https://github.com/JaimeJimenezG/Moonlight-ps4) were protocol or performance references. PSBox also informed the resolution-preference and game-text transaction formats implemented by the native Nexo adapter. Their implementations are not incorporated merely by being cited. PSBox code that *is* incorporated is identified above and in source headers.

Nexo X4's geometric icon and in-app mark were created for this project. They do not reproduce official platform logos or imply platform endorsement.
