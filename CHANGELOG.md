# 1.0.0-rc.1

First release candidate, based on the user-tested 0.1.25 / v038 implementation. No new gameplay or performance algorithm is introduced during RC preparation.

Since public 0.1.24: bounded town composition reuse, invalidation on animation/construction/hover/layout changes, memory release when the town closes, and town cache diagnostics. Existing startup/map/battle improvements remain.

Publication work: source/SDK path mapping; exact-byte Git attributes and reproducible checksum generation/verification; missing Vita SPDX headers; removal of unused framework/JAR/test binaries; matching source and license/build/relink evidence; neutral geometric public artwork; stable title ID H3VC00001 with APP_VER 01.00 and RC-specific logs.

The previously tested private v038 executable and this fresh RC rebuild are different artifacts. Test the RC installation, LiveArea, launch, town, battle and save/load before final promotion. The target remains 30 FPS, with scene-dependent performance.
