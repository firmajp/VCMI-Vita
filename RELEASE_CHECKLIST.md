# 1.0.0-rc.1 verification

Machine-readable outcomes are in AUDIT_RESULTS.json, docs/compliance/BUILD_VERIFICATION.json, the ScanCode review and BINARY_BUILD_PROVENANCE.json. Report unexecuted checks as pending.

Required preparation: clean exported-source client build; modified-LGPL-library source/build/relink probe; no private local paths in shipped ELF/load segments; complete license/source notices; no original game assets or proprietary Sony modules; byte-consistent VPK resources; valid indexed LiveArea PNGs/XML; short install paths; final checksums preserved across Git round-trip; review of ScanCode findings and unused binary exclusions.

The user confirmed the original exact RC1 installation, LiveArea, launch, town, battle and save/load checklist. See docs/compliance/DEVICE_TEST.json for the tested VPK hash. LiveArea101 preserves the tested executable/SFO/boot/game resources; check installation, new bubble/background/gate and launch on this artwork repack. More detailed town interactions, arbitrary maps/mods and prolonged stress coverage are not inferred from that confirmation. Test larger battles/sieges and longer sessions before final release. Retain saves and title ID when upgrading.

The supplied source/library texts and evidence address the reported distribution issues. Tool output and the maintainer's review are not an independent legal certification or proof of original authorship of every expression.
