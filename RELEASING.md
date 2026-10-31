# Publishing the first RC

Use a new repository named VCMI-Vita and the contents of the prepared source ZIP. No previous Git history or remote is included. Do not copy old private VPKs, logs, SDK/tool binaries or commercial game data into it. Preserve .gitattributes (`* -text`) so Git does not rewrite source bytes.

1. Verify `python scripts/checksums.py verify` before uploading and again after a fresh clone. The optional `--git` check verifies Git index blobs too.
2. Keep version 1.0.0-rc.1 and title ID H3VC00001 consistent with VERSION.json, VPK and provenance. Vita APP_VER 01.00 is a platform package counter; it does not encode the RC suffix.
3. Attach the matching VPK, complete corresponding source ZIP and external SHA256 files together. Mark any GitHub Release as a prerelease and use tag v1.0.0-rc.1. Include the FFmpeg license/source notice from README on the download page, and preserve all component notices.
4. Perform the exact-RC console checklist in RELEASE_CHECKLIST.md. Record actual results before claiming those checks passed. Do not substitute baseline engine tests for this artifact's installation test.

This package is prepared locally. It does not upload, recreate or modify a GitHub repository. Source code review and tooling do not provide an absolute legal guarantee. When changing the build, repeat path/license/resource audits and source/relink checks. Generate SHA256SUMS only after all files are final; never edit a listed file afterward without refreshing and re-verifying the list.
