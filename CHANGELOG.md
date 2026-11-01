# 1.0.1 — development build

Based on the exact published 1.0.0 tag/commit; the published VPK digest was verified against the previously tested package. No new changes are taken from the older private development checkout.

New original sunset/camp LiveArea artwork, its CC0 generators and updated cache revision. The system launch image is copied byte-for-byte to the early splash/loading background; a translucent navy/gold loading panel preserves readable stage messages and an animated activity bar. The panel uses SDL drawing primitives and the existing cached background texture.

Public version, application title, VPK and log filenames now use 1.0.1. CMake generates the version header from VERSION.json, so subsequent version changes update these values together. Title ID remains H3VC00001; Vita APP_VER advances to 01.01.

Gameplay, battle/town/map optimisations, controller support and serialization remain as in 1.0.0. Performance claims and new-version console verification remain pending hardware measurements.
