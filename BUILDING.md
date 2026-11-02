# Building and relinking 1.1.0

Use a dedicated VitaSDK installation; the script installs rebuilt libraries into it. Reference tools: VitaSDK GCC 15.2.0, CMake 4.4.5, Ninja 1.13.2, Python 3.12.14 and PowerShell. Python 3.10 or newer is required for the supplied scripts. You also need POSIX `sh` and GNU make; MSYS2 UCRT64 make with its matching runtime dependencies is suitable on Windows. Put the shell utilities and make runtime DLL directory on PATH. Do not mix MINGW64 and UCRT64 gettext runtime DLLs. Toolchain binaries are not distributed in the source package.

The SDK must already contain the dependency packages identified in SOURCE_MANIFEST.json and dependency-recipes/. Their source and Vita recipes are supplied. This is a client/dependency build, not a bootstrap of GCC/newlib itself. Use checkout and SDK paths without spaces for the POSIX FFmpeg build.

```powershell
$env:VITASDK='C:/vita/vitasdk'
./scripts/build.ps1 -Jobs 4 -Shell 'C:/tools/busybox/sh.exe' -Make 'C:/tools/make/mingw32-make.exe'
```

The script rebuilds pthread-embedded, modified SDL2, minimal FFmpeg, oneTBB, libsquish, configured newlib iconv and zlib-only minizip, then the whole engine and VPK. Precompiled headers are disabled: the native Windows cross-compiler failed to map its PCH files reliably. A monolithic install configuration keeps unused install-path constants relative. `-BuildOnly` stops before publication packaging/audit. `-SkipDependencies` is for relinking with dependency libraries you already rebuilt and installed, not for the first build.

`-ffile-prefix-map` and `-fdebug-prefix-map` replace the checkout/SDK paths with `/vcmi-vita` and `/vitasdk`, including FFmpeg. Its makefiles expand source filenames to absolute paths, so absolute prefix maps are required. `sanitize_ffmpeg_config.py` normalizes those paths only in the generated configuration-description string; compilation still uses the real paths. Do not remove these steps. Run the binary path check before shipping a rebuilt VPK.

FFmpeg 7.1.2 builds only Bink/Smacker and relevant audio/PCM decoding, with programs, networking, assembly, pthreads, GPL/nonfree options and autodetection disabled. Its implementation uses int-sized enums. The supplied normalization script marks installed public enum definitions with GCC mode(SI), preserving calls from the engine's short-enum ABI. Do not change the engine's enum ABI globally. The original FFmpeg source and all relevant notices are retained.

SDL's non-overlapping wide-row copy uses sceClibMemcpy; overlapping copies retain their original behavior. Its Vita CMake build enables ASM so the selected ARM SIMD/NEON implementation files are actually compiled. Their exact permissive notices are in LICENSES/SDL-ARM-BLITTER-NOTICES.txt. Static dependencies use POSITION_INDEPENDENT_CODE=OFF and oneTBB uses TBBMALLOC_BUILD=OFF, matching the working Vita configuration and avoiding unsupported relocations/allocator code. See patches/SDL-ROW-COPY-NOTICE.txt. pthread-embedded follows the supplied Vita CMakeLists.txt with BUILD_TESTSUITE=OFF. Test corpus/framework/JAR exclusions are recorded in SOURCE_DISTRIBUTION_EXCLUSIONS.json; none is needed by this Vita client build.

## Relinking modified LGPL libraries

For pthread, edit `third_party/pthread-embedded`, rebuild/install it into the dedicated SDK with the Vita toolchain and BUILD_TESTSUITE=OFF, then run `build.ps1 -SkipDependencies`. For FFmpeg, edit `third_party/FFmpeg`, rerun `scripts/build-ffmpeg.ps1` and then `build.ps1 -SkipDependencies`. Full application source is provided, so proprietary application object files are unnecessary. VitaSDK creates the homebrew SELF/VPK; no project signing key is required.

`scripts/test-lgpl-relink.ps1` performs a separate source modification/rebuild/relink test, verifies the probe in the executable, and restores the release library and original executable. It is a build test, not a device execution test. Results and exact scope are in docs/compliance/BUILD_VERIFICATION.json.

Existing bare engine VPK packaging:

```powershell
python scripts/package.py --bare-vpk build/vcmi/clientapp/vcmiclient.vpk
python scripts/audit.py
python scripts/checksums.py generate
python scripts/checksums.py verify
```

Regenerate LiveArea with packaging/generate-livearea.ps1, which calls tools/make_final_livearea.py with the bundled NotoSerif-Bold.ttf and the cache counter recorded in VERSION.json. Python/Pillow/numpy and the supplied helper generators are required. The wrapper also copies the 960x544 launch image byte-for-byte to vitaBoot.png; generate-boot.ps1 repeats only that copy. Release PNGs are supplied indexed and must not be recompressed. preview*.png are local mockups, not installable assets.

VERSION.json is the public version source: CMake generates VitaVersion.h and the SFO title/version; the package script reads the same JSON. The Vita metadata parser requires CMake 3.19 or newer; 4.4.5 is the reference build tool. Public artifacts and diagnostic logs use 1.1.0, while content-rev is only the LiveArea cache counter.
