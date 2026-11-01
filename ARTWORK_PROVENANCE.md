# Public artwork

## LiveArea and startup/loading artwork — 1.0.1

`sce_sys/icon0.png`, `sce_sys/pic0.png`, `sce_sys/livearea/contents/bg.png` and `sce_sys/livearea/contents/startup.png`
are original artwork drawn entirely by code (`tools/make_final_livearea.py` with `make_vcmi_livearea.py`, `make_bg_variants.py`
and `make_bg_heroic.py`; Python, Pillow, numpy): a sunset mountain landscape with a generic fantasy castle silhouette, an army
camp with tents and banners, a shield emblem with a "V" and a compass star, and the lettering "VCMI / VITA".
The supplied code draws the design from geometry and the bundled font; it does not load external game images or screenshots. The generic silhouettes are not presented as commercial game characters or logos.

To the extent any rights exist in this artwork and the generator scripts, they are dedicated to the public domain under
CC0 1.0 (`LICENSES/CC0-1.0.txt`). The lettering uses Noto Serif Bold (SIL OFL 1.1, notices in `LICENSES/OFL-NOTO.txt`).
The name "VCMI" identifies the project; no ownership of third-party names is asserted.

The launch image `packaging/sce_sys/pic0.png` is also copied unchanged to `packaging/vitaBoot.png`. The early app splash and loading background therefore match it exactly. Loading text, a translucent panel and an activity bar are drawn by SDL over that background; no additional scene artwork is embedded in the panel.

Built-in VCMI assets retain their separate CC BY-SA, CC BY or public-domain author/licence records in `LICENSES/VCMI-ASSET-AUTHORS.txt`.

## Repository banner

The README banner in assets/github-banner.png was generated with the built-in imagegen tool using the new sunset/camp LiveArea artwork as references. This is separate AI-generated repository artwork; it is not a code-generated LiveArea asset and is not installed in the VPK. It follows the project artwork's CC0 terms to the extent applicable rights exist. Prompt, references and method are recorded in docs/branding/BANNER_PROVENANCE.md. Upstream author and font-software notices remain supplied.

