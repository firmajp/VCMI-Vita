# RC distribution review — 2026-10-31

This internal technical review responds to the supplied assessment of public 0.1.24. The assessment's local-path finding is addressed as privacy/build hygiene; checksum mismatches are addressed as distribution integrity. Neither is represented here as a judicial finding of copyright infringement.

Remediation: rebuild the exported tree with source/SDK prefix maps; inspect the resulting executable; preserve exact file bytes in Git via .gitattributes; generate hashes only after final packaging; verify a Git index/checkout round-trip; add missing Vita license headers; remove unused compiled Apple frameworks/JARs/test corpus; include full source, notices and LGPL relink evidence; run and review ScanCode. Detailed outcomes are recorded in docs/compliance/.

The combined executable uses GPL-3.0-or-later under VCMI's GPL-2.0-or-later option. pthread-embedded follows its original LGPL 2.1 COPYING via section 6(a) source/relinking delivery, without relying on its broader README claim. FFmpeg 7.1.2 is configured with GPL/nonfree/version3 options disabled; its source and public-header ABI adaptation are supplied. minizip uses zlib only, without OpenSSL/encryption implementation code. Per-file licenses remain authoritative.

Public geometric artwork and Noto font notices are retained. Commercial game data, private generative images, proprietary Sony runtime modules, signing keys and SDK binaries are excluded. A new repository is intended; no old history is included or claimed to have been purged from GitHub's infrastructure.

Primary terms: [GPLv3](https://www.gnu.org/licenses/gpl-3.0.en.html), [LGPL 2.1](https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html), [FFmpeg licensing](https://www.ffmpeg.org/legal.html). Applicable texts and author notices are supplied in LICENSES and component sources.

The user tested the v038 engine baseline; the exact rebuilt RC's console installation remains pending. ScanCode is an external tool run by this project's maintainer, not an independent legal review. Its detections require interpretation and do not establish ownership or exclude all unattributed similarities. No certification of every legal outcome is claimed.
