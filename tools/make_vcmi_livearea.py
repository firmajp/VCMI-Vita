#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""VCMI-Vita LiveArea artwork generator.

Original artwork drawn entirely by code (no external images, no game assets,
no third-party logos). Dedicated to the public domain under CC0 1.0 to the
extent any rights exist. Text uses Noto Serif Bold (SIL OFL 1.1).

Requires: Python 3, Pillow, numpy.
Usage:    python make_vcmi_livearea.py --font NotoSerif-Bold.ttf --out out_dir
Output:   <out>/sce_sys/icon0.png            128x128
          <out>/sce_sys/pic0.png             960x544
          <out>/sce_sys/livearea/contents/bg.png       840x500
          <out>/sce_sys/livearea/contents/startup.png  280x158
          <out>/sce_sys/livearea/contents/template.xml
          <out>/preview.png                  mock-up (not part of the package)
All PNGs are 8-bit indexed, <=128 colours, no alpha, not interlaced.
"""
import argparse
import math
import os
import random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

SS = 3  # supersampling factor

GOLD = [(0.0, '#f8e0a0'), (0.5, '#dcab4a'), (1.0, '#a8742a')]
GOLD_L = [(0.0, '#fbe6ae'), (1.0, '#dfb257')]
GOLD_R = [(0.0, '#d3a047'), (1.0, '#8d5f24')]
NAVY = [(0.0, '#18325a'), (1.0, '#08122a')]


def hexc(h):
    h = h.lstrip('#')
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def vgrad(w, h, stops):
    ys = np.linspace(0, 1, h, dtype=np.float32)
    out = np.empty((h, w, 3), np.float32)
    ts = [s[0] for s in stops]
    for c in range(3):
        out[:, :, c] = np.interp(ys, ts, [hexc(s[1])[c] for s in stops])[:, None]
    return out


def radial(w, h, cx, cy, rx, ry, power=2.0):
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d = np.sqrt(((xx - cx) / rx) ** 2 + ((yy - cy) / ry) ** 2)
    return np.clip(1 - d, 0, 1) ** power


def to_pil(a):
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))


def from_pil(im):
    return np.asarray(im.convert('RGB'), dtype=np.float32).copy()


def ridge(w, h, base, amp, seed, jag):
    r = random.Random(seed)
    x = np.linspace(0, 1, w)
    y = np.zeros(w)
    for k in range(1, 9):
        f = r.uniform(0.8, 1.3) * k * 1.1
        a = amp / (k ** 1.15)
        p = r.uniform(0, 6.283)
        s = np.sin(2 * np.pi * f * x + p)
        y += a * ((1 - jag) * s + jag * (1 - 2 * np.abs(s)))
    return (base + y) * h


def add_layer(a, w, h, rdg, col_top, col_bot, glow, rim=0.0):
    yy = np.arange(h, dtype=np.float32)[:, None]
    r = rdg[None, :].astype(np.float32)
    t = np.clip((yy - r) / (0.30 * h), 0, 1)[..., None]
    top = np.array(hexc(col_top), np.float32)
    bot = np.array(hexc(col_bot), np.float32)
    col = top * (1 - t) + bot * t
    alpha = np.clip((yy - r) / (1.2 * SS) + 0.5, 0, 1)[..., None]
    if rim > 0:
        edge = np.clip(1 - np.abs(yy - r - 1.5 * SS) / (2.2 * SS), 0, 1)[..., None]
        gx = glow[np.clip(rdg.astype(int), 0, h - 1), np.arange(w)][None, :, None]
        col = col + edge * gx * np.array([255, 190, 115], np.float32) * rim
    return a * (1 - alpha) + col * alpha


def mist(a, w, h, y_c, amp, color='#a9bcdc', width=0.06):
    yy = np.arange(h, dtype=np.float32)[:, None]
    m = (amp * np.exp(-(((yy - y_c * h) / (width * h)) ** 2)))[..., None]
    return a * (1 - m) + np.array(hexc(color), np.float32) * m


def draw_castle(img, cx, y0, u, body, wm=None):
    """Original fantasy castle silhouette. (cx,y0)=centre of base, u=unit."""
    d = ImageDraw.Draw(img)

    def R(x0, ya, x1, yb):
        d.rectangle([cx + x0 * u, y0 + ya * u, cx + x1 * u, y0 + yb * u], fill=body)

    def P(pts):
        d.polygon([(cx + x * u, y0 + y * u) for x, y in pts], fill=body)

    P([(-17, 60), (-17, 7), (-12, -1), (12, -1), (17, 7), (17, 60)])  # rock
    R(-12, -9, 12, 2)  # curtain wall
    x = -12.0
    while x < 11.4:  # battlements
        R(x, -10.4, x + 1.4, -9)
        x += 2.8
    R(-12.5, -22, -6, -9)  # left tower
    P([(-13.3, -22), (-5.2, -22), (-9.2, -32)])
    R(6, -19, 12.5, -9)  # right tower
    P([(5.3, -19), (13.2, -19), (9.2, -28)])
    R(-4.6, -27, 4.6, -9)  # keep
    P([(-5.5, -27), (5.5, -27), (0, -40)])
    R(-0.6, -49, 0.6, -39)  # spire + flag
    P([(0.6, -49), (5.5, -47.2), (0.6, -45.4)])
    R(-17.5, 2, -11, 6)  # outer wall stub
    P([(-17.5, -2), (-11.5, -2), (-11.5, 3), (-17.5, 3)])
    if wm is not None:
        wd = ImageDraw.Draw(wm)
        for (wx, wy, ww, wh) in [(-1.8, -23, 1.1, 2.2), (1.8, -23, 1.1, 2.2), (0, -16, 1.2, 2.4),
                                 (-9.2, -18, 1.0, 2.0), (9.2, -15, 1.0, 2.0), (-9.2, -13, 0.9, 1.6),
                                 (0, -5.5, 2.6, 5.5)]:
            wd.rectangle([cx + (wx - ww / 2) * u, y0 + wy * u, cx + (wx + ww / 2) * u, y0 + (wy + wh) * u], fill=255)


def pine(d, x, yb, hgt, color):
    tiers = 4
    for i in range(tiers):
        t0 = i / tiers
        top = yb - hgt * (1 - t0 * 0.78)
        wid = hgt * (0.20 + 0.18 * i / tiers)
        bot = yb - hgt * (1 - (i + 1.35) / tiers * 0.78) if i < tiers - 1 else yb - hgt * 0.06
        d.polygon([(x, top), (x - wid, bot), (x + wid, bot)], fill=color)
    d.rectangle([x - hgt * 0.02, yb - hgt * 0.10, x + hgt * 0.02, yb + hgt * 0.02], fill=color)


def scene(W, H, seed=11):
    S = SS
    w, h = W * S, H * S
    rnd = random.Random(seed)
    sky = vgrad(w, h, [(0, '#050a18'), (0.28, '#0f1d40'), (0.52, '#26467a'),
                       (0.70, '#9a6f86'), (0.84, '#ee9f66'), (1, '#f7c987')])
    glow = radial(w, h, 0.60 * w, 0.70 * h, 0.62 * w, 0.64 * h, 2.0)
    sky += glow[..., None] * np.array([255, 190, 115], np.float32) * 0.50
    base = to_pil(sky).convert('RGBA')

    st = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    sd = ImageDraw.Draw(st)
    for _ in range(int(330 * W / 840)):
        x = rnd.random() * w
        y = (rnd.random() ** 1.7) * 0.52 * h
        b = int(255 * rnd.uniform(0.35, 1.0) * max(0.0, 1 - y / (0.60 * h)))
        r = rnd.choice([0.5, 0.5, 0.7, 1.0, 1.4]) * S
        sd.ellipse([x - r, y - r, x + r, y + r], fill=(255, 244, 220, b))
    for _ in range(6):  # a few sparkles
        x = rnd.uniform(0.05, 0.95) * w
        y = rnd.uniform(0.04, 0.30) * h
        L = rnd.uniform(5, 9) * S
        sd.line([(x - L, y), (x + L, y)], fill=(255, 244, 220, 120), width=max(1, S // 2))
        sd.line([(x, y - L), (x, y + L)], fill=(255, 244, 220, 120), width=max(1, S // 2))
    sd.ellipse([0.0, 0.0, 0.0, 0.0])
    a = from_pil(Image.alpha_composite(base, st))

    layers = [  # base, amp, jag, top, bottom, rim
        (0.56, 0.10, 0.60, '#6b7ea6', '#8f86a6', 0.55),
        (0.63, 0.09, 0.50, '#4b6892', '#6c77a0', 0.45),
        (0.71, 0.08, 0.35, '#304c78', '#3f5a86', 0.35),
        (0.81, 0.06, 0.25, '#1a3053', '#152945', 0.20),
        (0.91, 0.035, 0.10, '#0b1630', '#08101f', 0.0),
    ]
    ridges = []
    for i, (bs, am, jg, ct, cb, rm) in enumerate(layers):
        rd = ridge(w, h, bs, am, seed + 17 * i, jg)
        ridges.append(rd)
        a = add_layer(a, w, h, rd, ct, cb, glow, rim=rm)
        if i == 1:  # distant small castle
            im = to_pil(a)
            cx = int(0.17 * w)
            draw_castle(im, cx, rd[cx] + 2 * S, 0.30 * h / 100.0 * 0.62, hexc('#4d6794'))
            a = from_pil(im)
            a = mist(a, w, h, 0.69, 0.22)
        if i == 2:  # main castle on the right
            im = to_pil(a)
            cx = int(0.795 * w)
            u = h / 100.0 * 0.92
            wm = Image.new('L', (w, h), 0)
            draw_castle(im, cx, rd[cx] + 3 * u, u, hexc('#0b1630'), wm)
            wg = wm.filter(ImageFilter.GaussianBlur(2.6 * S))
            arr = from_pil(im)
            gold = np.array([255, 196, 108], np.float32)
            arr += (np.asarray(wg, np.float32) / 255.0)[..., None] * gold * 1.4
            sharp = (np.asarray(wm, np.float32) / 255.0)[..., None]
            arr = arr * (1 - sharp) + np.array([255, 214, 130], np.float32) * sharp
            a = mist(arr, w, h, 0.79, 0.20)
        if i == 3:
            a = mist(a, w, h, 0.885, 0.20)

    fg = to_pil(a).convert('RGBA')
    d = ImageDraw.Draw(fg)
    tcol = (6, 11, 24, 255)
    for _ in range(int(15 * W / 840)):
        pine(d, rnd.uniform(-0.01, 0.20) * w, h * rnd.uniform(0.99, 1.06), h * rnd.uniform(0.14, 0.34), tcol)
    for _ in range(int(12 * W / 840)):
        pine(d, rnd.uniform(0.84, 1.01) * w, h * rnd.uniform(0.99, 1.06), h * rnd.uniform(0.12, 0.28), tcol)
    a = from_pil(fg)
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    v = np.clip(((xx - w / 2) / (w / 2)) ** 2 + ((yy - h / 2) / (h / 2)) ** 2, 0, 1.4)
    a *= (1 - 0.30 * v)[..., None]
    return to_pil(a).resize((W, H), Image.LANCZOS)


def bez(p0, p1, p2, p3, n=28):
    pts = []
    for i in range(n + 1):
        t = i / n
        mt = 1 - t
        x = mt ** 3 * p0[0] + 3 * mt * mt * t * p1[0] + 3 * mt * t * t * p2[0] + t ** 3 * p3[0]
        y = mt ** 3 * p0[1] + 3 * mt * mt * t * p1[1] + 3 * mt * t * t * p2[1] + t ** 3 * p3[1]
        pts.append((x, y))
    return pts


def shield_pts(cx, cy, sc):
    right = bez((0.5, 0.02), (0.5, 0.40), (0.18, 0.56), (0.0, 0.70))
    left = [(-x, y) for x, y in reversed(right)]
    top = [(-0.5, -0.55), (-0.25, -0.60), (0, -0.64), (0.25, -0.60), (0.5, -0.55)]
    return [(cx + x * sc, cy + y * sc) for x, y in top + right + left]


def poly_grad(img, pts, stops):
    x0 = max(0, int(min(p[0] for p in pts)))
    x1 = min(img.width, int(max(p[0] for p in pts)) + 1)
    y0 = max(0, int(min(p[1] for p in pts)))
    y1 = min(img.height, int(max(p[1] for p in pts)) + 1)
    if x1 <= x0 or y1 <= y0:
        return
    mask = Image.new('L', img.size, 0)
    ImageDraw.Draw(mask).polygon(pts, fill=255)
    g = to_pil(vgrad(x1 - x0, y1 - y0, stops))
    img.paste(g, (x0, y0), mask.crop((x0, y0, x1, y1)))


def draw_emblem(img, cx, cy, sc):
    """Shield with a gold V and a compass star."""
    sh = Image.new('L', img.size, 0)
    ImageDraw.Draw(sh).polygon(shield_pts(cx + 0.02 * sc, cy + 0.04 * sc, sc * 1.03), fill=255)
    sh = sh.filter(ImageFilter.GaussianBlur(0.05 * sc)).point(lambda v: int(v * 0.65))
    img.paste((0, 0, 0), (0, 0) + img.size, sh)
    poly_grad(img, shield_pts(cx, cy, sc), GOLD)
    poly_grad(img, shield_pts(cx, cy, sc * 0.89), NAVY)
    d = ImageDraw.Draw(img)
    ln = shield_pts(cx, cy, sc * 0.80)
    d.line(ln + [ln[0]], fill=(176, 134, 64), width=max(1, int(0.014 * sc)))

    def T(pts):
        return [(cx + x * sc, cy + y * sc) for x, y in pts]

    A, B, C, D, E, F = (-0.38, -0.36), (-0.20, -0.36), (0, 0.16), (0.20, -0.36), (0.38, -0.36), (0, 0.46)
    poly_grad(img, T([A, B, C, F]), GOLD_L)
    poly_grad(img, T([C, D, E, F]), GOLD_R)
    star = []
    for i in range(8):
        ang = -math.pi / 2 + i * math.pi / 4
        rr = 0.125 if i % 2 == 0 else 0.036
        star.append((0 + rr * math.cos(ang), -0.20 + rr * math.sin(ang)))
    d.polygon(T(star), fill=(255, 240, 196))


def text_gold(img, x, y_base, text, font, spacing, stops=GOLD, shadow=True):
    mask = Image.new('L', img.size, 0)
    md = ImageDraw.Draw(mask)
    xs = x
    for ch in text:
        md.text((xs, y_base), ch, font=font, fill=255, anchor='ls')
        xs += font.getlength(ch) + spacing
    bb = mask.getbbox()
    if not bb:
        return 0
    if shadow:
        sm = mask.filter(ImageFilter.GaussianBlur(max(1.0, font.size * 0.04)))
        sm = sm.point(lambda v: int(v * 0.8))
        off = max(1, int(font.size * 0.05))
        sh = Image.new('L', img.size, 0)
        sh.paste(sm, (off, off))
        img.paste((0, 0, 0), (0, 0) + img.size, sh)
    g = to_pil(vgrad(bb[2] - bb[0], bb[3] - bb[1], stops))
    img.paste(g, (bb[0], bb[1]), mask.crop(bb))
    return xs - x - spacing


def mini_scene(W, H, seed, glow_c=(0.5, 0.8)):
    """Small night sky + two ridge bands, used for icon and startup banners."""
    S = SS
    w, h = W * S, H * S
    rnd = random.Random(seed)
    a = vgrad(w, h, [(0, '#060c1c'), (0.5, '#12264a'), (0.85, '#2d4f7e'), (1, '#486b94')])
    g = radial(w, h, glow_c[0] * w, glow_c[1] * h, 0.7 * w, 0.7 * h, 2.0)
    a += g[..., None] * np.array([240, 170, 100], np.float32) * 0.35
    im = to_pil(a).convert('RGBA')
    d = ImageDraw.Draw(im)
    for _ in range(int(60 * W / 128)):
        x = rnd.random() * w
        y = (rnd.random() ** 1.5) * 0.6 * h
        r = rnd.choice([0.5, 0.6, 0.8, 1.1]) * S
        d.ellipse([x - r, y - r, x + r, y + r], fill=(255, 244, 220, int(255 * rnd.uniform(0.3, 0.9))))
    arr = from_pil(im)
    gl = radial(w, h, glow_c[0] * w, glow_c[1] * h, 0.7 * w, 0.7 * h, 2.0)
    for i, (bs, am, jg, ct, cb) in enumerate([(0.80, 0.07, 0.5, '#26416b', '#1a2f52'), (0.92, 0.04, 0.3, '#0b1630', '#08101f')]):
        arr = add_layer(arr, w, h, ridge(w, h, bs, am, seed + 5 * i, jg), ct, cb, gl, rim=0.3 if i == 0 else 0)
    return to_pil(arr)


def make_icon(font_path):
    S = SS
    im = mini_scene(128, 128, 3, (0.5, 0.78))
    im = im.convert('RGB')
    d = ImageDraw.Draw(im)
    r = 60.5 * S
    cx = cy = 64 * S
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(214, 172, 84), width=int(1.8 * S))
    r2 = 57.2 * S
    d.ellipse([cx - r2, cy - r2, cx + r2, cy + r2], outline=(88, 112, 150), width=max(1, int(0.8 * S)))
    draw_emblem(im, cx, 61.5 * S, 62 * S)
    return im.resize((128, 128), Image.LANCZOS)


def make_startup(font_path):
    S = SS
    W, H = 280, 158
    im = mini_scene(W, H, 8, (0.22, 0.85)).convert('RGB')
    d = ImageDraw.Draw(im)
    bar = max(1, int(1.6 * S))
    d.rectangle([0, 0, W * S, bar], fill=(214, 172, 84))
    d.rectangle([0, H * S - bar, W * S, H * S], fill=(214, 172, 84))
    draw_emblem(im, 66 * S, 79 * S, 78 * S)
    big = ImageFont.truetype(font_path, int(46 * S))
    small = ImageFont.truetype(font_path, int(15 * S))
    wv = text_gold(im, 124 * S, 80 * S, 'VCMI', big, 2.5 * S)
    d.line([(125 * S, 91 * S), (125 * S + wv, 91 * S)], fill=(214, 172, 84), width=int(1.4 * S))
    text_gold(im, 126 * S, 113 * S, 'V I T A'.replace(' ', ''), small, 9.5 * S, stops=[(0, '#e9c777'), (1, '#c79a45')], shadow=False)
    return im.resize((W, H), Image.LANCZOS)


def make_pic0(font_path):
    S = SS
    im = scene(960, 544, seed=11).convert('RGB')
    big_im = im.resize((960 * S, 544 * S), Image.LANCZOS)
    big = ImageFont.truetype(font_path, int(84 * S))
    small = ImageFont.truetype(font_path, int(27 * S))
    draw_emblem(big_im, 134 * S, 142 * S, 112 * S)
    wv = text_gold(big_im, 218 * S, 158 * S, 'VCMI', big, 5 * S)
    ImageDraw.Draw(big_im).line([(220 * S, 178 * S), (220 * S + wv, 178 * S)], fill=(214, 172, 84), width=int(2 * S))
    text_gold(big_im, 222 * S, 215 * S, 'VITA', small, 17 * S, stops=[(0, '#e9c777'), (1, '#c79a45')], shadow=False)
    return big_im.resize((960, 544), Image.LANCZOS)


def save_png8(im, path, colors=128):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    q = im.convert('RGB').quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
    q.save(path, optimize=True)


TEMPLATE = '''<?xml version="1.0" encoding="utf-8"?>
<livearea style="a1" format-ver="01.00" content-rev="{rev}">
  <livearea-background><image>bg.png</image></livearea-background>
  <gate><startup-image>startup.png</startup-image></gate>
</livearea>
'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--font', default='NotoSerif-Bold.ttf')
    ap.add_argument('--out', default='out')
    ap.add_argument('--rev', default='101', help='content-rev; raise it when you change the art (LiveArea caches it)')
    args = ap.parse_args()
    o = args.out
    bg = scene(840, 500, seed=11)
    startup = make_startup(args.font)
    icon = make_icon(args.font)
    pic0 = make_pic0(args.font)
    save_png8(icon, os.path.join(o, 'sce_sys/icon0.png'))
    save_png8(pic0, os.path.join(o, 'sce_sys/pic0.png'))
    save_png8(bg, os.path.join(o, 'sce_sys/livearea/contents/bg.png'))
    save_png8(startup, os.path.join(o, 'sce_sys/livearea/contents/startup.png'))
    with open(os.path.join(o, 'sce_sys/livearea/contents/template.xml'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(TEMPLATE.format(rev=args.rev))
    # mock-up: LiveArea paper with the gate button in the middle + bubble previews
    pv = Image.open(os.path.join(o, 'sce_sys/livearea/contents/bg.png')).convert('RGB')
    st = Image.open(os.path.join(o, 'sce_sys/livearea/contents/startup.png')).convert('RGB')
    pv.paste(st, ((840 - 280) // 2, 168))
    canvas = Image.new('RGB', (840 + 190, 500), (24, 28, 36))
    canvas.paste(pv, (0, 0))
    ic = Image.open(os.path.join(o, 'sce_sys/icon0.png')).convert('RGB')
    for size, y in ((128, 40), (64, 200)):
        t = ic.resize((size, size), Image.LANCZOS)
        m = Image.new('L', (size * 4, size * 4), 0)
        ImageDraw.Draw(m).ellipse([0, 0, size * 4 - 1, size * 4 - 1], fill=255)
        m = m.resize((size, size), Image.LANCZOS)
        canvas.paste(t, (840 + (190 - size) // 2, y), m)
    canvas.save(os.path.join(o, 'preview.png'))


if __name__ == '__main__':
    main()
