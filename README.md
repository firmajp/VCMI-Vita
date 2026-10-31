# VCMI-Vita

**1.0.0-rc.1 — first release candidate, LiveArea revision 101.** A native [VCMI](https://github.com/vcmi/vcmi) engine port for PlayStation Vita, based on the device-tested 0.1.25 / v038 implementation. This is a candidate for testing, not a claim that every map, campaign or hardware configuration works.

You need your own legally obtained **Heroes of Might and Magic III: Complete** data. The Steam HD Edition is incompatible. Original game archives, maps, music, installers and Windows executables are not included. This independent community port is not affiliated with or endorsed by Ubisoft, Sony or the upstream VCMI project. Names identify compatibility and remain their owners' trademarks.

## Download and install

The VPK is `dist/vcmi_vita_1.0.0-rc.1.vpk`. Keep the complete corresponding source ZIP with any binary download. This software uses [FFmpeg](https://ffmpeg.org/) licensed under LGPL-2.1-or-later; its complete source is [included here](third_party/FFmpeg), with configuration and relinking instructions in [BUILDING.md](BUILDING.md).

1. Use a homebrew-enabled Vita and VitaShell. Tested engine baseline: Vita 2000, firmware 3.65 Ensō.
2. Back up saves and settings. Transfer and install the VPK with VitaShell. Title ID **H3VC00001** is unchanged, so this updates the test application. Vita APP_VER is **01.00**; the public version is **1.0.0-rc.1**.
3. From your own Complete installation, copy `Data`, `Maps` and optionally `Mp3` directly into:

   ```text
   ux0:data/vcmi/Data/
   ux0:data/vcmi/Maps/
   ux0:data/vcmi/Mp3/
   ```

   Keep the original names and contents. Do not nest another game-installation folder inside `vcmi`. Do not copy the Windows executable or installer. Engine configuration, scripts, fonts and the built-in VCMI module are already included.
4. Launch from LiveArea. An early splash is followed by loading progress. Loading speed depends on storage and game data.

Saves: `ux0:data/vcmi/Saves/`; settings: `ux0:data/vcmi/config/`; engine logs: `ux0:data/vcmi/logs/`.

## Controls and testing

Use the left stick for the cursor and Cross to confirm/click, or use the front touchscreen. Rear touch is disabled. Prompts use PlayStation button symbols. CPU/GPU requests are 444/222 MHz; plugins can override them.

The user confirmed installation, LiveArea, launch, town, battle and save/load on the original exact RC1 VPK; its digest and checklist are in docs/compliance/DEVICE_TEST.json. This artwork revision preserves that tested executable, SFO, boot splash and game resources. Install this refreshed VPK and check the new bubble icon, LiveArea background/gate and launch; the new artwork has not yet been checked on a console. Large battles, sieges, campaigns, arbitrary mods and long sessions need broader testing.

Logs: `ux0:data/vcmi-startup-rc1.log`, `vcmi-performance-rc1.log` and `vcmi-watchdog-rc1.log`. Holding L + R + Start for about two seconds intentionally aborts the process for a diagnostic dump; unsaved progress is lost. Review logs before sharing and never upload commercial game data.

Town rendering reuses a bounded composition cache while preserving animation ticks and construction/hover changes. Overhanging layouts fall back to the original renderer. See [CHANGELOG.md](CHANGELOG.md) and [performance notes](docs/PERFORMANCE.md). No new FPS claim is made without RC device measurements.

## Build, source and licenses

The complete engine, modified libraries, dependency sources, original notices and build/installation material are supplied. See [BUILDING.md](BUILDING.md), [SOURCE_ACCESS.md](SOURCE_ACCESS.md), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [review evidence](COMPLIANCE_AUDIT.md). Checksums preserve exact bytes across Git clones through `.gitattributes`; verify them with `python scripts/checksums.py verify`.

The combined executable is supplied under GPL-3.0-or-later using VCMI's GPL-2.0-or-later option. Individual files retain their original licenses. Statically linked LGPL libraries include source and rebuilding/relinking material; no restriction is added on modifying the libraries or reverse engineering to debug those modifications. Noto fonts use SIL OFL. VCMI asset credits and exceptions are in `LICENSES/VCMI-ASSET-AUTHORS.txt`; new public geometric artwork uses CC0 with separate font notices.

The Vita work used AI assistance; upstream code retains its authorship and licenses. See [PORT_PROVENANCE.md](PORT_PROVENANCE.md). Technical checks and a license scanner do not certify legal rights or guarantee behavior. This software is provided without warranty.
