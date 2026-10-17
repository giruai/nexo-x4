# Retained licenses and attribution notices

This directory retains upstream license and attribution texts for code and assets used by Nexo X4. The table records the source revision or release for key dependency notices and SHA-256 values of the local copies. It does not assert a particular developer machine or SDK archive build revision. Preserve original file headers and the matching complete patched dependency source bundle when distributing a package.

| Local file | Original source/location | SHA-256 |
|---|---|---|
| `libdatachannel-MPL-2.0.txt` | libdatachannel `LICENSE`, `bdc5ff28e9d3b863144c94a677ecf5bf043aaf15` | `fab3dd6bdab226f1c08630b1dd917e11fcb4ec5e1e020e2c16f83a0a13863e85` |
| `libjuice-MPL-2.0.txt` | libjuice `LICENSE`, `b89c792e3612faf2f12cf35bcc56857313a06be3` | `fab3dd6bdab226f1c08630b1dd917e11fcb4ec5e1e020e2c16f83a0a13863e85` |
| `usrsctp-LICENSE.md` | usrsctp `LICENSE.md`, `fec583d54493f879d2ae44a743423bf8a04371ab` | `fa53711b25af4b9a9b8dadfea3cb38166ec4b96760c8d62b284055554537d9ef` |
| `libsrtp-LICENSE.txt` | libsrtp `LICENSE`, `d33b8ffb1491a0b4b58a206889f09800cf7310ab` | `8e19d42a1eec9561f3f347253ddf2e385c55f392f025bb0fd41b88dbf38db5ae` |
| `nlohmann-json-MIT.txt` | json `LICENSE.MIT`, `55f93686c01528224f448c19128836e7df245f72` | `46a65cffd1ea955132d95a8dd921640714a8d6b537d2e4e482d31145ae95b603` |
| `plog-MIT.txt` | plog `LICENSE`, `94899e0b926ac1b0f4750bfbd495167b4a6ae9ef` | `e4d01796524cbc13b1571a1f0914823c6a23b316ba039e7014f47a4eef7fd4c3` |
| `MbedTLS-dual-license.txt` | Mbed TLS `LICENSE`, `068ff080b369adfac81509f9b57b2afabaf82dc5` | `9b405ef4c89342f5eae1dd828882f931747f71001cfba7d114801039b52ad09b` |
| `MbedTLS-framework-LICENSE.txt` | framework `LICENSE`, `dde0c4a0e448a0552f18817dcea633bb851fd288` | `11402351e38392230bb8934ba1095c0c0049a296c0f8821f76e4672dff54b490` |
| `MbedTLS-everest-README.md` | Mbed TLS `3rdparty/everest/README.md`, same Mbed TLS revision | `96a16739f1453480c84b1c787f0d48ea15c1d307af0a2e9baea4d98b0f1d83f2` |
| `MbedTLS-p256-m-README.md` | Mbed TLS `3rdparty/p256-m/README.md`, same Mbed TLS revision | `9708f7be7a7775254eacf8d7b3b2961e0905d203bda67c8be95e61c471664e39` |
| `p256-m-upstream-README.md` | Mbed TLS `3rdparty/p256-m/p256-m/README.md`, same Mbed TLS revision | `ad5c8b957fd06e7969fa4a0f8831de6c29cc8c94efa73dc536922d636a9f862e` |
| `Opus-BSD-3-Clause.txt` | Opus 1.5.2 `COPYING` | `01e1167d54a096d123cf6dfbbeb19587278845c6481d2d66d545669846079551` |

The general [OpenOrbis GPL-3.0 text](OpenOrbis-GPL-3.0.txt) and LLVM [libc++](LLVM-libcxx-11-LICENSE.TXT)/[libc++abi](LLVM-libcxxabi-11-LICENSE.TXT) notices are also retained. The Mbed TLS file includes both full licenses because upstream offers **Apache-2.0 OR GPL-2.0-or-later**. Its bundled Everest/p256-m directories have no separate `LICENSE` file in this checkout; exact READMEs establish their applicable license and attribution.

The [PSBox](PSBox-GPL-3.0.txt), [Inter](Inter-OFL.txt), [zlib](zlib-LICENSE.txt), [Project Nayuki QR generator](qrcodegen-MIT.txt), [stb_truetype](stb_truetype-LICENSE.txt), [stb_image](stb_image-LICENSE.txt), [stb_image_resize2](stb_image_resize2-LICENSE.txt), and [OpenGNM example](OpenGNM-examples-MIT.txt) notices are also retained here. Inter's OFL text accompanies the bundled fonts under `assets/psbox/fonts/OFL.txt`. Source files keep their individual copyright and SPDX notices, including PSBox files marked GPL-3.0-or-later.

For component pins, use and corresponding-source distribution, see [Third-party notices](../../THIRD_PARTY_NOTICES.md). These notices accompany each release package and its corresponding application and dependency source bundles.

SDK-provided static C/compiler runtime notices: [musl COPYRIGHT](musl-COPYRIGHT.txt) and [LLVM compiler-rt](LLVM-compiler-rt-LICENSE.TXT). Their provenance limits are explained in [Third-party notices](../../THIRD_PARTY_NOTICES.md).
