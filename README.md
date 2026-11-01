![VCMI-Vita](assets/github-banner.png)

**AI assistance:** The Vita adaptation, local optimisations and project documentation were developed with AI assistance. Upstream VCMI and third-party code retain their original authorship and licences. The README banner is AI-generated from the project's LiveArea artwork; see [banner provenance](docs/branding/BANNER_PROVENANCE.md).

# VCMI-Vita 1.0.1

Development build for the next update, based directly on the official [1.0.0 release](https://github.com/firmajp/VCMI-Vita/releases/tag/v1.0.0), commit `0b66197149246ce61fc94f484d5ff0640988c2d9`. This begins work towards 1.1. The 1.0.1 build needs its own console test; testing of 1.0.0 is recorded separately.

## Changes in 1.0.1

- New sunset mountain/castle/camp LiveArea artwork and matching bubble/launch image.
- The system launch image, first in-app splash and loading background use the same image, with a readable loading panel added during initialization.
- Version metadata in `VERSION.json` supplies the application title, package filename and diagnostic log names.
- Existing gameplay, rendering optimisations, controller mapping and save format are retained. No new FPS or startup-time improvement is claimed before device measurements.

## Requirements and installation

Use a homebrew-enabled Vita with VitaShell and your own legally obtained **Heroes of Might and Magic III: Complete** data. Steam HD Edition is incompatible. Original game archives, maps, music, installers and Windows executables are not included.

1. Back up saves and settings. Install `dist/vcmi_vita_1.0.1.vpk` using VitaShell; verify it with the adjacent `.vpk.sha256` file.
2. Copy your own `Data`, `Maps` and optionally `Mp3` folders directly into `ux0:data/vcmi/`.
3. Launch from LiveArea. Title ID **H3VC00001** is unchanged; Vita APP_VER is **01.01**, public version **1.0.1**. This updates the existing application.

Use the left stick to move the cursor and Cross to click, or use the front touchscreen. Rear touch is disabled. Saves are in `ux0:data/vcmi/Saves/`; settings in `ux0:data/vcmi/config/`; engine logs in `ux0:data/vcmi/logs/`.

## Testing and logs

The user tested 1.0.0 on a Vita 2000, firmware 3.65 Ensō, including installation, LiveArea, launch, towns, battles and save/load. That confirmation does not automatically cover this new executable. Test installation, updated LiveArea, the splash/loading transition, menu, town, battle and save/load on **1.0.1**. Campaigns, arbitrary mods, larger battles/sieges and long sessions need broader coverage.

Current diagnostic logs are `ux0:data/vcmi-startup-1.0.1.log`, `ux0:data/vcmi-performance-1.0.1.log` and `ux0:data/vcmi-watchdog-1.0.1.log`. Holding L + R + Start for about two seconds intentionally aborts the application to obtain a diagnostic dump; unsaved progress is lost. Review logs before sharing them.

## Source and licences

The combined executable uses **GPL-3.0-or-later** under VCMI's GPL-2.0-or-later option. Individual components retain their original terms. Matching engine/dependency sources, original notices and build/install/relink material are supplied; see `BUILDING.md`, `SOURCE_ACCESS.md`, `PTHREAD_LINKING.md` and `THIRD_PARTY_NOTICES.md`. Statically linked LGPL components include complete source and rebuilding/relinking material. Users may modify those libraries and reverse engineer the linked application to debug library modifications.

This software uses [FFmpeg](https://ffmpeg.org/) under LGPL-2.1-or-later; its source is in `third_party/FFmpeg` and its configuration/ABI adaptations are supplied. GPL/nonfree options are disabled. New code-drawn LiveArea/boot artwork follows CC0-1.0 to the extent rights exist; Noto font software retains SIL OFL notices. Built-in VCMI artwork retains its separate original credits/licences.

Verify the complete source distribution with `python scripts/checksums.py verify`. Exact bytes are preserved by Git attributes; after staging, `--git` also checks index blobs. Technical build/scan evidence is in `docs/compliance/`; previous-version reports are explicitly under `baseline-1.0.0/`.

Independent community port; not an official release from, affiliated with, or endorsed by Ubisoft, Sony or the upstream VCMI team. Names identify compatibility and remain their owners' trademarks. Vita work used AI assistance; see `PORT_PROVENANCE.md`. Provided without warranty; tooling is not independent legal certification.
