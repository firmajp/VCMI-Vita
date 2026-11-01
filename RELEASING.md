# Publishing 1.0.1

Apply the matching source changes to the existing VCMI-Vita repository. Preserve Git attributes and original notices. Do not upload private logs, SDK/tool binaries, game data or generated build directories.

Use version/tag 1.0.1/v1.0.1, VPK vcmi_vita_1.0.1.vpk, Vita APP_VER 01.01 and title ID H3VC00001. The LiveArea cache counter is implementation metadata, not an alternative public version name.

Publish matching source, VPK and external SHA256 files together. Preserve the FFmpeg LGPL/source notice from README on the download page. Verify checksums before publication and after a fresh clone; use --git after staging. Mark an untested build as a prerelease. Record the 1.0.1 device checklist before presenting it as device-tested.

Generate SHA256SUMS only after all distribution files are final. Refresh it after any subsequent change. This local workflow does not upload, publish or edit a GitHub release.
