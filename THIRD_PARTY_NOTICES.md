# Third-party notices

Nexo X4 application sources are [GPL-3.0-only](LICENSE). This file identifies incorporated third-party code and assets, the separate licenses they retain, and technical references that were consulted without copying their implementations. Original copyright and SPDX headers in source files remain authoritative. Full retained license texts and their inventory are in [docs/licenses](docs/licenses/README.md).

## Application foundation and PS4 SDK

| Work | Source and use | Retained terms |
| --- | --- | --- |
| [XCloud4](https://github.com/asp11edgr/XCloud4) | Application foundation, based on commit [`8eefb3a485867cde8af9a5c358eff6d80c8f95cc`](https://github.com/asp11edgr/XCloud4/commit/8eefb3a485867cde8af9a5c358eff6d80c8f95cc). | GPL-3.0-only; original file headers retained. |
| [PSBox](https://github.com/RafaelNGP/xbox-cloud-gaming-ps5) by RafaelNGP | Adapted UI, canvas, font rendering, localization, icons, accents, and key mapping from commit [`41a6ea9f4624c4ea9410f25f2c9761de35e96501`](https://github.com/RafaelNGP/xbox-cloud-gaming-ps5/commit/41a6ea9f4624c4ea9410f25f2c9761de35e96501), primarily under `src/ui/psbox/vendor`. The PS4 bridge and metadata backend adapt those interfaces. | GPL-3.0-or-later source headers and [PSBox license](docs/licenses/PSBox-GPL-3.0.txt). |
| [OpenOrbis PS4 Toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain) v0.5.4 | Public build/package examples, display/input/audio setup, and auxiliary `libSceFios2.prx` and `libc.prx` built from its source. The exact public module sources and Makefiles, CRT source/build recipe, linker script and original license are retained in [the runtime source supplement](vendor/openorbis-runtime/README.md). | [OpenOrbis GPL-3.0](docs/licenses/OpenOrbis-GPL-3.0.txt); individual SDK components keep their own terms. |
| [Backporter Videodec2 proposal](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/pull/213) | Public declarations adapted in `src/video/videodec2_abi.h`. | OpenOrbis GPL-3.0 notice as above. |
| [LLVM libc++ 11](https://github.com/llvm/llvm-project/blob/176249bd6732a8044d457092ed932768724a6f06/libcxx/src/condition_variable.cpp) | Unmodified `condition_variable.cpp` is compiled as `src/streaming/rtc_condition_variable.cpp` for the native timeout ABI. OpenOrbis also links libc++/libc++abi runtime archives. | Apache-2.0 WITH LLVM-exception; [libc++](docs/licenses/LLVM-libcxx-11-LICENSE.TXT) and [libc++abi](docs/licenses/LLVM-libcxxabi-11-LICENSE.TXT) notices. The observed SDK header version does not prove the exact build revision of its prebuilt archives. |

## SDK-provided C and compiler runtime

The pinned public OpenOrbis v0.5.4 SDK supplies the static C runtime linked with `-lc`. Its release workflow builds the OpenOrbis musl fork and merges compiler-rt builtins into that archive. Retained notices are [musl COPYRIGHT](docs/licenses/musl-COPYRIGHT.txt) and [LLVM compiler-rt](docs/licenses/LLVM-compiler-rt-LICENSE.TXT). Musl's main license is MIT; its COPYRIGHT also identifies included third-party terms. Compiler-rt carries Apache-2.0 WITH LLVM-exception and the legacy notices preserved in its license text.

The SDK archive is pinned by SHA-256 in `dependencies.lock.json`; the actual linked C/C++ runtime archives and CRT inputs are identified by hashes in [SDK-ARCHIVES.json](vendor/openorbis-runtime/SDK-ARCHIVES.json). Its workflow does not pin the musl checkout revision, and the exact upstream revision of every prebuilt compiler/runtime archive has not been established. These notices identify the components and retain their terms; they do not claim the SDK archives were rebuilt from the application source bundle. This differs from the two auxiliary module wrappers, whose exact public source snapshot is included in the application tree. See the [runtime supplement](vendor/openorbis-runtime/README.md) and [OpenOrbis v0.5.4 release workflow](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain/blob/v0.5.4/.github/workflows/toolchain.yml).

## Linked network, media, and utility libraries

The network and media libraries are built from pinned sources in the **complete patched dependency source bundle** for this release. The bundle contains the actual source trees and all target modifications, submodules, overlays, build configuration, original notices, and port scripts. The project does not replace that source obligation with links to upstream repositories.

| Component | Pinned source or release | License / retained notice |
| --- | --- | --- |
| [libdatachannel](https://github.com/paullouisageneau/libdatachannel/tree/bdc5ff28e9d3b863144c94a677ecf5bf043aaf15) 0.24.6 | `bdc5ff28e9d3b863144c94a677ecf5bf043aaf15`; modified PS4 port and receive instrumentation | [MPL-2.0](docs/licenses/libdatachannel-MPL-2.0.txt) |
| [libjuice](https://github.com/paullouisageneau/libjuice/tree/b89c792e3612faf2f12cf35bcc56857313a06be3) 1.7.4 | `b89c792e3612faf2f12cf35bcc56857313a06be3`; modified PS4 network/clock port | [MPL-2.0](docs/licenses/libjuice-MPL-2.0.txt) |
| [usrsctp](https://github.com/paullouisageneau/usrsctp/tree/fec583d54493f879d2ae44a743423bf8a04371ab) | `fec583d54493f879d2ae44a743423bf8a04371ab`; PS4 socket layout adaptation | [BSD 3-Clause](docs/licenses/usrsctp-LICENSE.md) |
| [libsrtp](https://github.com/cisco/libsrtp/tree/d33b8ffb1491a0b4b58a206889f09800cf7310ab) | `d33b8ffb1491a0b4b58a206889f09800cf7310ab` | [BSD 3-Clause](docs/licenses/libsrtp-LICENSE.txt) |
| [Mbed TLS](https://github.com/Mbed-TLS/mbedtls/tree/068ff080b369adfac81509f9b57b2afabaf82dc5) 3.6.7 | `068ff080b369adfac81509f9b57b2afabaf82dc5` and [framework](https://github.com/Mbed-TLS/mbedtls-framework/tree/dde0c4a0e448a0552f18817dcea633bb851fd288) `dde0c4a0e448a0552f18817dcea633bb851fd288`; bundled Everest and p256-m | [Apache-2.0 OR GPL-2.0-or-later](docs/licenses/MbedTLS-dual-license.txt), [framework](docs/licenses/MbedTLS-framework-LICENSE.txt), [Everest](docs/licenses/MbedTLS-everest-README.md), and [p256-m](docs/licenses/MbedTLS-p256-m-README.md) attributions; bundled subcomponents retain their stated terms. |
| [Opus](https://opus-codec.org/release/stable/2024/04/12/libopus-1_5_2.html) 1.5.2 | Official release, used for live audio decoding | [BSD 3-Clause](docs/licenses/Opus-BSD-3-Clause.txt) |
| [zlib](https://github.com/madler/zlib/tree/v1.3.1) 1.3.1 | Bounded gzip response decoding | [zlib license](docs/licenses/zlib-LICENSE.txt) |
| [nlohmann/json](https://github.com/nlohmann/json/tree/55f93686c01528224f448c19128836e7df245f72) | `55f93686c01528224f448c19128836e7df245f72`, libdatachannel dependency | [MIT](docs/licenses/nlohmann-json-MIT.txt) |
| [plog](https://github.com/SergiusTheBest/plog/tree/94899e0b926ac1b0f4750bfbd495167b4a6ae9ef) | `94899e0b926ac1b0f4750bfbd495167b4a6ae9ef`, libdatachannel dependency | [MIT](docs/licenses/plog-MIT.txt) |

The retained Mbed TLS license includes both upstream alternatives. Everest's bundled code is Apache-2.0; bundled p256-m carries its own Apache-2.0 OR GPL-2.0-or-later attribution. See the exact README and license files rather than inferring a single license for every file in that tree. Modified libdatachannel and libjuice files keep their MPL headers in the corresponding source bundle. The source export must match the compiled package, including receiver NACK and receive diagnostic changes.

## UI assets and embedded helpers

| Component | Use | Retained terms |
| --- | --- | --- |
| [Inter](https://github.com/rsms/inter) | Bundled PSBox UI fonts in `assets/psbox/fonts` | [SIL Open Font License 1.1](assets/psbox/fonts/OFL.txt) and [copy in docs/licenses](docs/licenses/Inter-OFL.txt) |
| [stb_truetype](https://github.com/nothings/stb), stb_image, stb_image_resize2 by Sean Barrett and contributors | Font rasterization and image decode/resize in the PSBox UI | Original headers and [truetype](docs/licenses/stb_truetype-LICENSE.txt), [image](docs/licenses/stb_image-LICENSE.txt), [resize2](docs/licenses/stb_image_resize2-LICENSE.txt) notices; MIT/public-domain choice as stated in those files |
| [QR Code generator](https://www.nayuki.io/page/qr-code-generator-library) by Project Nayuki | Device sign-in QR display | MIT notice in source and [retained text](docs/licenses/qrcodegen-MIT.txt) |

The Nexo X4 geometric icon and in-app mark are original project artwork; see [branding](assets/branding/README.md). They do not imply endorsement by a platform owner.

## Public references without incorporated implementations

The bounded GPU readback uses public packet-field information from [OpenGNM/freegnm-examples](https://github.com/PS4-OpenGNM/freegnm-examples/tree/ffd055563e5b849ebef1b276ba29e730199670f8) and [OpenGNM](https://github.com/PS4-OpenGNM/opengnm/tree/4b295ca54c82c83acf308d1c646a2dfa9ae57350). The example's [MIT notice](docs/licenses/OpenGNM-examples-MIT.txt) is retained. [GreenVita](https://github.com/Day-OS/green-vita), [Better xCloud](https://github.com/redphx/better-xcloud), and [Moonlight PS4](https://github.com/JaimeJimenezG/Moonlight-ps4) informed protocol or performance investigation; their source implementations were not copied into this project. Separate PSBox code that *was* incorporated is identified above and by its retained source headers.

The public OAuth client identifier used for service compatibility is neither a secret nor a Nexo X4-owned app registration. Its use does not incorporate GreenVita code or confer ownership of that registration.
