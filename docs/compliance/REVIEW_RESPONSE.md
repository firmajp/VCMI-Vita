# Response to the supplied 0.1.24 distribution assessment

This describes the new 1.0.0-rc.1 local release package. It does not retroactively certify or change a previously uploaded artifact.

| Reported issue | RC action and evidence |
| --- | --- |
| Local user/build paths in the executable | Rebuilt the exported engine and modified dependencies with prefix maps; normalized FFmpeg's configuration-description string. Checked ELF, load segments and expanded SELF streams. See BINARY_CHECKS.json and PREVIOUS_PATH_CHECK.json. |
| Checksums change when committed | Root and nested Git attributes preserve bytes. Final checksums are verified against files, Git index blobs and a separate checkout with autocrlf enabled. See GIT_INTEGRITY.json. |
| Build and LGPL relink unverified | Completed an exported-source engine build using a dedicated existing VitaSDK and rebuilt modified dependencies. A separately modified pthread library was built and linked into a separate application; its probe was verified and the release library restored. See BUILD_VERIFICATION.json. This is not an independent bit-identical rebuild or a console execution of the probe. |
| Missing license scan | ScanCode license/info scan and final changed-file scan are merged with file-hash coverage checks. Retained detections and internal interpretations are in SCANCODE_REVIEW.json and scancode-licenses.json.gz. Tooling does not establish original authorship or find every possible copied expression. |
| Unused compiled platform bundles and missing headers | Omitted unused frameworks, JARs and test corpora; added missing Vita notice/SPDX headers. Required library implementation and original notices are retained. See SOURCE_DISTRIBUTION_EXCLUSIONS.json and SOURCE_CONTENT_CHECKS.json. |
| Artwork and exact public VPK not tested on Vita | Public geometric artwork is supplied with provenance/font notices and indexed-PNG format checks. The tested v038 engine baseline is distinguished from this rebuilt RC. Exact RC installation and visual checks remain pending; see RELEASE_CHECKLIST.md. |
| Same application title ID | H3VC00001 intentionally updates the test application. README instructs users to back up saves/settings. |
| Previously published history | The archive contains no Git history or remote configuration. A new repository is intended. Removal of old GitHub caches is not claimed. |

No commercial game Data/Maps/Mp3 files or proprietary Sony runtime modules are supplied. The user must provide their own compatible Complete data. The combined executable uses GPL-3.0-or-later; the LGPL libraries retain their own notices and source/relinking route. See the root legal/source documents for the actual distribution terms.
