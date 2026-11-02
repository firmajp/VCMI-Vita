# Corresponding source - 1.1.0

Version 1.1.0 is the first public release. Publish the matching source commit/tag alongside the VPK and its checksum. Preserve this complete source tree: `src/vcmi`, full relevant library sources in `third_party`, original notices, dependency recipes and compilation/packaging/install material. A different upstream revision is not a substitute.

The combined executable is GPL-3.0-or-later. pthread-embedded and FFmpeg retain their LGPL terms; this distribution follows the LGPL 2.1 section 6(a) source/relinking route. Full application source permits relinking with modified libraries. Users may modify those libraries and reverse engineer the application to debug library changes. No contrary restriction or project signing-key requirement is added. See `BUILDING.md` and `PTHREAD_LINKING.md`.

Build and source evidence identifies the actual executable and the historical tests of unchanged dependencies. The 1.1.0 engine was built incrementally from this maintained source tree; a new independent clean rebuild or new modified-library relink test for this version is not claimed. Earlier relink evidence is retained with its original version and hashes. Source and build/relink instructions remain supplied.

Compiler/SDK binaries, proprietary system modules, original game data, installers, private logs and local build outputs are excluded. `SOURCE_DISTRIBUTION_EXCLUSIONS.json` records unused platform/test-reference exclusions. Keep the source available with every public VPK download.
