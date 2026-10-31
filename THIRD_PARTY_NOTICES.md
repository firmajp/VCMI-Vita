# Third-party notices

This is a mixed-license source distribution. The combined executable is distributed under GPL-3.0-or-later; this does not replace the original licenses of individual components. VCMI permits GPL version 2 or later; GPLv3 is used for this combination with Apache-2.0 oneTBB. See [Apache's compatibility statement](https://www.apache.org/licenses/GPL-compatibility.html).

| Component | Version/origin | Terms and notices |
| --- | --- | --- |
| VCMI | abe535c4418b66bae8cdb56fee1473b109e8c55e + Vita changes | GPL-2.0-or-later; AUTHORS.h and original headers retained |
| oneTBB | 2022.3.0 / f1862f38f83568d96e814e469ab61f88336cc595 + Vita changes | Apache-2.0; original license and third-party notices retained |
| SDL2 | 2.32.8 + Vita row-copy change | zlib license, Sam Lantinga and contributors |
| SDL ARM SIMD/NEON blitters | bundled pixman ARM implementations | Per-file permissive terms and copyright notices reproduced in SDL-ARM-BLITTER-NOTICES.txt; implementation sources retained |
| SDL2_image | 2.8.12 | zlib license; embedded decoder notices retained in source |
| SDL2_mixer | 2.8.2 | zlib license; embedded minimp3, dr_libs and Timidity notices retained in source |
| SDL2_ttf | 2.24.0 | zlib license |
| FreeType | 2.14.3 | FreeType License option; copyright The FreeType Project, see FTL.TXT |
| libpng | 1.6.58 | PNG Reference Library License; LICENSE included |
| zlib | 1.3.2 | zlib license, Jean-loup Gailly and Mark Adler |
| bzip2 | 1.0.8 | bzip2 license, Julian Seward |
| libwebp | 1.6.0 | BSD-style license; AUTHORS, COPYING and PATENTS retained |
| libogg | 1.3.6 | BSD-style license, Xiph.Org Foundation |
| libvorbis | 1.3.7 | BSD-style license, Xiph.Org Foundation |
| Opus / opusfile | 1.6.1 / 0.12 | BSD-style licenses; relevant patent grants/notices retained |
| libmodplug | d1b97ed0020bc620a059d3675d1854b40bd2608d + Vita recipe patch | Public-domain/libmodplug notices in COPYING/headers; preserve all component notices |
| libxmp-lite | 4.7.2 | MIT, Claudio Matsuoka and Hipolito Carraro Jr; full notice in README |
| Boost | 1.78.0 | Boost Software License 1.0; installed headers plus sources of filesystem, atomic and program_options included |
| LuaJIT-Vita | c329ddd10691c1875f26087ba23c2ae278515e24 | MIT; COPYRIGHT and Vita changes retained |
| libsquish | c763145a30512c10450954b7a2b5b3a2f9a94e00 + build configuration | MIT, Simon Brown |
| minizip-ng | 4.2.2, zlib-only public configuration | zlib license, Nathan Moinvaziri and contributors |
| pthread-embedded | Vita source snapshot 11d2e5722d98c86f33c908fc47b2cf6e55205db5 | Original COPYING specifies LGPL 2.1; upstream README says LGPLv2-or-later. This distribution follows the LGPL-2.1 requirements without relying on that broader README statement; Vita-specific patches retain MIT. Static linking uses the section 6(a) source/relinking route, not a section 3 conversion; see PTHREAD_LINKING.md and SOURCE_ACCESS.md |
| vita-rss-libdl | 0.3.2 | MIT, Ivan Epifanov and Jaylon Gowie |
| taiHEN interface | 0.11 | MIT, Yifan Lu; system stubs are interfaces, not proprietary system modules |
| Newlib / configured iconv | Vita newlib fbb8375870d799758e0b4358d2f1708781aa26a6 | Multiple permissive notices; complete COPYING.NEWLIB retained |
| GCC / libstdc++ runtime | VitaSDK GCC 15.2.0 | GPLv3 with GCC Runtime Library Exception 3.1; copies supplied |
| Noto Sans / Noto Serif | bundled font files | SIL Open Font License 1.1; copyright notices retained in fonts and OFL-NOTO.txt |
| VCMI artwork | bundled built-in module | CC BY-SA 4.0 unless stated otherwise; VCMI-ASSET-AUTHORS.txt identifies authors, public-domain and CC BY exceptions |
| LiveArea / boot graphics | LiveArea101 Python/Pillow/numpy generator; unchanged System.Drawing boot generator | New design CC0-1.0 to the extent rights exist; Noto font SIL OFL; see ARTWORK_PROVENANCE.md |
| FFmpeg | 7.1.2, minimal Bink/Smacker build | LGPL-2.1-or-later; full source and ABI/header adaptation script supplied; GPL/nonfree options disabled |

Full license and notice files are in `LICENSES/` and alongside their sources. Source file headers remain authoritative for embedded or separately licensed portions. Embedded decoder source headers carrying their license text are included in the license collection as well.

The public executable does **not** include the legacy OpenSSL implementation previously linked through the SDK minizip build. Encryption and optional ZIP codecs were disabled and the public ELF checked for their implementation symbols. Do not publish the earlier private test binaries as if they were this audited artifact.

The repository excludes the original Heroes III Data/Maps/Mp3 folders, executables/installers and proprietary Sony runtime modules. Original game data must be supplied by the end user. Trademarks remain with their respective owners.

Dated modification notices and a file list are in `MODIFICATIONS.md`. Preserve these alongside the original component notices.
