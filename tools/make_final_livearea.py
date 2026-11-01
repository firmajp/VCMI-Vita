#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""VCMI-Vita LiveArea final set (sunset camp). Original code-generated artwork, CC0 1.0.
Lettering: Noto Serif Bold (SIL OFL 1.1). Needs make_vcmi_livearea.py, make_bg_variants.py, make_bg_heroic.py next to it.
Usage: python make_final_livearea.py --font NotoSerif-Bold.ttf --out outdir --rev 102"""
import argparse, os, random, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_vcmi_livearea as M
import make_bg_variants as V
import make_bg_heroic as Hh
from make_vcmi_livearea import SS, vgrad, radial, to_pil, from_pil, ridge, hexc

SKY = [(0, '#12091f'), (0.30, '#3a1a4a'), (0.55, '#9a3f4e'), (0.74, '#ec7a48'), (0.88, '#f8b25c'), (1, '#fdd486')]
LAYERS = [(0.58, 0.09, 0.6, '#7a4a6a', '#9a5a68', 0.7), (0.66, 0.08, 0.5, '#503a66', '#604268', 0.55),
          (0.74, 0.07, 0.4, '#2e2858', '#262050', 0.4), (0.83, 0.05, 0.25, '#17183a', '#101230', 0.2),
          (0.93, 0.03, 0.1, '#090c22', '#060818', 0.0)]


def scene(W, H):
    V.W, V.H = W, H
    return V.landscape(61, SKY, (255, 150, 80), (0.50, 0.72), LAYERS, (255, 170, 90), castle_x=0.84, castle_layer=1,
                       castle_col='#0b0f26', window=(255, 170, 80), mist_col='#e69a7a', star_n=70, glow_amt=0.7,
                       pines=False, extra=Hh.camp).convert('RGB')


def mini(W, H, seed, stops, glow_c, ridges):
    w, h = W * SS, H * SS
    rnd = random.Random(seed)
    a = vgrad(w, h, stops)
    g = radial(w, h, glow_c[0] * w, glow_c[1] * h, 0.8 * w, 0.8 * h, 2.0)
    a += g[..., None] * np.array([255, 150, 80], np.float32) * 0.45
    im = to_pil(a).convert('RGBA')
    d = ImageDraw.Draw(im)
    for _ in range(int(55 * W / 128)):
        x = rnd.random() * w; y = (rnd.random() ** 1.6) * 0.55 * h
        r = rnd.choice([0.5, 0.6, 0.8, 1.1]) * SS
        d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 240, 214, int(255 * rnd.uniform(0.3, 0.9))))
    arr = from_pil(im)
    for i, (bs, am, jg, ct, cb, rm) in enumerate(ridges):
        arr = V.layer(arr, w, h, ridge(w, h, bs, am, seed + 5 * i, jg), ct, cb, g, rm, (255, 170, 90))
    return to_pil(arr)


def icon(font):
    S = SS
    im = mini(128, 128, 3, [(0, '#12091f'), (0.45, '#3a1a4a'), (0.75, '#9a3f4e'), (1, '#e0734a')], (0.5, 0.95),
              [(0.80, 0.07, 0.5, '#4a2a5e', '#3a2050', 0.5), (0.92, 0.04, 0.3, '#0c0a24', '#080818', 0.0)]).convert('RGB')
    d = ImageDraw.Draw(im)
    cx = cy = 64 * S
    r = 60.5 * S
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(214, 172, 84), width=int(1.8 * S))
    r2 = 57.2 * S
    d.ellipse([cx - r2, cy - r2, cx + r2, cy + r2], outline=(120, 70, 100), width=max(1, int(0.8 * S)))
    M.draw_emblem(im, cx, 61.5 * S, 62 * S)
    return im.resize((128, 128), Image.LANCZOS)


def startup(font, bg):
    """Gate button: the piece of the background behind it, with a soft dark glass panel, so it blends into the scene."""
    S = SS
    W, H = 280, 158
    x0, y0 = (840 - W) // 2, (500 - H) // 2 + 0
    base = bg.crop((x0, y0, x0 + W, y0 + H)).resize((W * S, H * S), Image.LANCZOS).convert('RGBA')
    pad, rad = 7 * S, 14 * S
    ov = Image.new('RGBA', base.size, (0, 0, 0, 0))
    m = Image.new('L', base.size, 0)
    ImageDraw.Draw(m).rounded_rectangle([pad, pad, W * S - pad, H * S - pad], radius=rad, fill=255)
    grad = np.linspace(0.40, 0.66, H * S, dtype=np.float32)[:, None] * np.ones((1, W * S), np.float32)
    a = (np.asarray(m, np.float32) / 255.0 * grad * 255).astype(np.uint8)
    tint = Image.new('RGBA', base.size, (14, 7, 28, 0))
    tint.putalpha(Image.fromarray(a))
    im = Image.alpha_composite(base, tint)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([pad, pad, W * S - pad, H * S - pad], radius=rad, outline=(214, 172, 84, 235), width=int(1.6 * S))
    d.rounded_rectangle([pad + 3 * S, pad + 3 * S, W * S - pad - 3 * S, H * S - pad - 3 * S], radius=rad - 3 * S,
                        outline=(214, 172, 84, 70), width=max(1, int(0.8 * S)))
    im = im.convert('RGB')
    M.draw_emblem(im, 68 * S, 79 * S, 74 * S)
    big = ImageFont.truetype(font, int(44 * S)); small = ImageFont.truetype(font, int(14.5 * S))
    wv = M.text_gold(im, 122 * S, 80 * S, 'VCMI', big, 2.3 * S)
    ImageDraw.Draw(im).line([(123 * S, 91 * S), (123 * S + wv, 91 * S)], fill=(214, 172, 84), width=int(1.4 * S))
    M.text_gold(im, 124 * S, 113 * S, 'VITA', small, 9 * S, stops=[(0, '#e9c777'), (1, '#c79a45')], shadow=False)
    return im.resize((W, H), Image.LANCZOS)


def pic0(font):
    S = SS
    im = scene(960, 544)
    b = im.resize((960 * S, 544 * S), Image.LANCZOS)
    big = ImageFont.truetype(font, int(84 * S)); small = ImageFont.truetype(font, int(27 * S))
    M.draw_emblem(b, 134 * S, 142 * S, 112 * S)
    wv = M.text_gold(b, 218 * S, 158 * S, 'VCMI', big, 5 * S)
    ImageDraw.Draw(b).line([(220 * S, 178 * S), (220 * S + wv, 178 * S)], fill=(214, 172, 84), width=int(2 * S))
    M.text_gold(b, 222 * S, 215 * S, 'VITA', small, 17 * S, stops=[(0, '#e9c777'), (1, '#c79a45')], shadow=False)
    return b.resize((960, 544), Image.LANCZOS)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--font', default='NotoSerif-Bold.ttf'); ap.add_argument('--out', default='out'); ap.add_argument('--rev', default='102')
    a = ap.parse_args(); o = a.out
    bg = scene(840, 500)
    M.save_png8(bg, os.path.join(o, 'sce_sys/livearea/contents/bg.png'))
    M.save_png8(startup(a.font, bg), os.path.join(o, 'sce_sys/livearea/contents/startup.png'))
    M.save_png8(icon(a.font), os.path.join(o, 'sce_sys/icon0.png'))
    M.save_png8(pic0(a.font), os.path.join(o, 'sce_sys/pic0.png'))
    with open(os.path.join(o, 'sce_sys/livearea/contents/template.xml'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(M.TEMPLATE.format(rev=a.rev))
    pv = Image.open(os.path.join(o, 'sce_sys/livearea/contents/bg.png')).convert('RGB')
    pv.paste(Image.open(os.path.join(o, 'sce_sys/livearea/contents/startup.png')).convert('RGB'), (280, 168))
    cv = Image.new('RGB', (840 + 190, 500), (24, 28, 36)); cv.paste(pv, (0, 0))
    ic = Image.open(os.path.join(o, 'sce_sys/icon0.png')).convert('RGB')
    for size, y in ((128, 40), (64, 200)):
        t = ic.resize((size, size), Image.LANCZOS); m = Image.new('L', (size * 4, size * 4), 0)
        ImageDraw.Draw(m).ellipse([0, 0, size * 4 - 1, size * 4 - 1], fill=255)
        cv.paste(t, (840 + (190 - size) // 2, y), m.resize((size, size), Image.LANCZOS))
    cv.save(os.path.join(o, 'preview.png'))
    Image.open(os.path.join(o, 'sce_sys/pic0.png')).convert('RGB').save(os.path.join(o, 'preview_pic0.png'))


if __name__ == '__main__':
    main()
