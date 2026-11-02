![VCMI-Vita](assets/github-banner.png)

**AI assistance:** The Vita adaptation, local optimisations and project documentation were developed with AI assistance. Upstream VCMI and third-party code retain their original authorship and licences. The README banner is AI-generated from the project's LiveArea artwork; see [banner provenance](docs/branding/BANNER_PROVENANCE.md).

# VCMI-Vita 1.1.0

An open-source port of the [VCMI](https://vcmi.eu) engine for PlayStation Vita, compatible with Heroes of Might and Magic III: Complete. Version 1.1.0 is the first public release.

## Features

- Native Vita engine with front-touchscreen and handheld button controls; rear touch is disabled.
- Optimised map and town rendering, image caches and reduced repeated UI work.
- Background preparation and persistent caching of ambient sounds to reduce loading stalls.
- Reduced diagnostic I/O and unnecessary work in adventure and battle AI, while retaining movement animations and AI simulation depth.
- Matching LiveArea, launch splash and loading background, with loading-stage messages.

The target is 30 FPS. Performance depends on the scene, map and battle; a constant 30 FPS is not guaranteed. Brief stalls can still occur during first-time asset loading and turn transitions.

## Requirements and installation

Use a homebrew-enabled Vita with VitaShell and your own legally obtained **Heroes of Might and Magic III: Complete** data. Steam HD Edition is incompatible. No original game files, installers or proprietary Sony runtime modules are included.

1. Back up saves and settings. Download `vcmi_vita_1.1.0.vpk` and its `.vpk.sha256` file from this repository's GitHub Releases, then install the VPK with VitaShell.
2. Copy your own `Data`, `Maps` and optionally `Mp3` folders directly into `ux0:data/vcmi/`.
3. Launch from LiveArea. The application title is **VCMI-Vita 1.1.0**, with title ID **H3VC00001**. Existing saves and settings stay in the data directory.

Move the cursor with the left stick and click with Cross, or use the front touchscreen. Saves are in `ux0:data/vcmi/Saves/`, settings in `ux0:data/vcmi/config/`, and engine logs in `ux0:data/vcmi/logs/`.

## Testing and diagnostics

The user reports successful testing of installation, LiveArea, launch, towns, battles and save/load on the exact 1.1.0 test executable, using a Vita 2000 with firmware 3.65 Enso. The preceding test build also completed a visible autocombat. Campaigns, arbitrary mods, large sieges and prolonged sessions have not received equivalent coverage.

Diagnostic logs are `ux0:data/vcmi-startup-1.1.0.log`, `ux0:data/vcmi-performance-1.1.0.log` and `ux0:data/vcmi-watchdog-1.1.0.log`. Hold L + R + Start for about two seconds to request a text diagnostic snapshot; this does not intentionally crash the application or create a system core dump. Review logs before sharing them.

## Source and licences

The combined executable uses **GPL-3.0-or-later** under VCMI's GPL-2.0-or-later option. Components retain their original terms. Matching engine/dependency sources, notices and build/install/relink material are supplied here; see `BUILDING.md`, `SOURCE_ACCESS.md`, `PTHREAD_LINKING.md` and `THIRD_PARTY_NOTICES.md`. Statically linked LGPL components include complete source and rebuilding/relinking material. Users may modify these libraries and reverse engineer the linked application to debug those modifications.

This software uses [FFmpeg](https://ffmpeg.org/) under LGPL-2.1-or-later. Its source is in `third_party/FFmpeg`, with the configuration and ABI adaptations supplied. GPL/nonfree options are disabled. LiveArea/boot artwork and the README banner follow the terms documented in `ARTWORK_PROVENANCE.md`; Noto font software retains SIL OFL notices. Built-in VCMI artwork retains its original credits and licences.

Verify the source tree with `python scripts/checksums.py verify`. After staging, `--git` also verifies indexed file bytes. Current evidence and its limits are in `docs/compliance/`; earlier test-build reports are retained as historical records. VPK downloads are attached to releases rather than duplicated in Git history.

Independent community port, not an official release from or affiliated with Ubisoft, Sony or the upstream VCMI team. Names describe compatibility and remain their owners' trademarks. Provided without warranty.
