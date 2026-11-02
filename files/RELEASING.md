# Publishing 1.1.0

Version 1.1.0 is the first public release. Apply the prepared source delta to the existing repository and review it before committing. Preserve `.gitattributes`, licences and original notices. Do not upload private logs, SDK/tool binaries, game data or generated build directories.

Use tag `v1.1.0`, VPK `vcmi_vita_1.1.0.vpk`, Vita APP_VER `01.60` and title ID `H3VC00001`. APP_VER is an internal monotonic installer counter; it must not be lowered to `01.10`. LiveArea content-rev is separate cache metadata.

Attach the final VPK and its `.sha256` to GitHub Releases. Keep binaries out of new Git commits to avoid uploading the same package twice. Publish the matching complete source tree/tag and retain the FFmpeg LGPL/source notice on the download page.

The public package preserves the user-tested executable and runtime resources byte-for-byte; only licence/provenance metadata is refreshed. Do not silently rebuild or replace that executable after acceptance. Refresh checksums after any change. `python scripts/checksums.py verify --git` checks staged source bytes. The local preparation workflow does not publish anything remotely.
