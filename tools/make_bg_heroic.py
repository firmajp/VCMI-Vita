#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Fantasy-adventure LiveArea backgrounds for VCMI-Vita. Generic original silhouettes only
(camp, dragon, rider): no game art, characters, creatures, logos or town designs. CC0."""
import argparse, math, os, sys, random
from PIL import Image, ImageDraw, ImageFilter
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_vcmi_livearea as M
import make_bg_variants as V
SS = M.SS
DARK = (6, 11, 24, 255)


def T(pts, x, y, s, flip=False):
    return [(x + (-px if flip else px) * s, y + py * s) for px, py in pts]


def glow(img, cx, cy, r, col, a=1.0):
    m = Image.new('L', img.size, 0)
    ImageDraw.Draw(m).ellipse([cx - r * 0.4, cy - r * 0.4, cx + r * 0.4, cy + r * 0.4], fill=int(255 * a))
    m = m.filter(ImageFilter.GaussianBlur(r * 0.5))
    img.paste(col, (0, 0) + img.size, m)


def tent(d, img, x, y, s, flag=True):
    d.polygon(T([(-10, 0), (0, -15), (10, 0)], x, y, s), fill=DARK)
    d.polygon(T([(-10, 0), (-13, 1.5), (0, -17), (13, 1.5), (10, 0)], x, y, s), fill=DARK)
    if flag:
        d.line(T([(0, -15), (0, -26)], x, y, s), fill=DARK, width=max(1, int(0.8 * s)))
        d.polygon(T([(0, -26), (9, -24), (0, -21.5)], x, y, s), fill=DARK)
    return img


def banner(d, x, y, s):
    d.line(T([(0, 0), (0, -34)], x, y, s), fill=DARK, width=max(2, int(1.0 * s)))
    d.polygon(T([(0, -34), (11, -34), (11, -20), (5.5, -24), (0, -20)], x, y, s), fill=DARK)


def camp(fg, w, h, rnd):
    d = ImageDraw.Draw(fg)
    gy = 0.925 * h
    for side in (0, 1):
        for i in range(4):
            x = (rnd.uniform(0.04, 0.26) if side == 0 else rnd.uniform(0.72, 0.96)) * w
            y = gy + rnd.uniform(-0.02, 0.03) * h
            s = rnd.uniform(2.3, 3.3) * SS
            tent(d, fg, x, y, s)
            glow(fg, x, y - 4 * s, 16 * s, (255, 190, 100, 255), 0.95)
    for x in (0.31, 0.69, 0.40, 0.60):
        banner(d, x * w, gy + 0.015 * h, 2.4 * SS)
    for x in (0.20, 0.80):
        cx, cy = x * w, gy + 0.045 * h
        glow(fg, cx, cy, 44 * SS, (255, 160, 70, 255), 1.0)
        d.polygon([(cx - 5 * SS, cy + 3 * SS), (cx, cy - 13 * SS), (cx + 5 * SS, cy + 3 * SS)], fill=(255, 214, 130, 255))
    return fg


DRAGON = [(-44, 11), (-32, 7), (-18, 4), (-7, 0), (4, -4), (12, -9), (20, -15), (28, -18), (36, -15), (29, -12),
          (22, -9), (15, -3), (7, 5), (-5, 9), (-18, 11), (-31, 13), (-46, 15)]
WING1 = [(3, -3), (-4, -22), (-16, -40), (-27, -52), (-25, -38), (-33, -40), (-29, -28), (-38, -26), (-30, -14), (-14, -4), (-6, 2)]
WING2 = [(7, -5), (14, -24), (27, -38), (41, -44), (35, -33), (45, -31), (36, -22), (44, -17), (30, -10), (14, -3)]


def dragon(fg, w, h, rnd, x=0.20, y=0.30, s=2.1):
    d = ImageDraw.Draw(fg)
    px, py, k = x * w, y * h, s * SS
    col = (8, 14, 30, 255)
    d.polygon(T(WING2, px, py, k), fill=(10, 20, 40, 255))
    d.polygon(T(DRAGON, px, py, k), fill=col)
    d.polygon(T(WING1, px, py, k), fill=col)
    d.polygon(T([(-44, 11), (-52, 6), (-50, 14), (-58, 12), (-46, 15)], px, py, k), fill=col)
    d.polygon(T([(26, -17), (22, -24), (28, -19)], px, py, k), fill=col)
    d.ellipse([px + 29 * k, py - 16 * k, px + 30.6 * k, py - 14.4 * k], fill=(255, 210, 120, 255))
    return fg


HORSE = [(-17,-19),(-14,-23),(-5,-24),(5,-24),(9,-31),(12,-38),(14,-41),(15,-37),(19,-33),(23,-28),(22,-26),(18,-26),(14,-27),
         (11,-21),(11,-12),(11,0),(8,0),(8,-10),(5,-12),(4,0),(1,0),(1,-12),(-3,-12),(-6,-10),(-8,0),(-11,0),(-12,-11),(-15,-13),
         (-18,-9),(-23,-4),(-20,-13),(-18,-17)]


def rider(fg, w, h, rnd, x=0.18, y=0.74, s=2.0):
    """Mounted knight with lance and pennant on a hill (generic silhouette)."""
    d = ImageDraw.Draw(fg)
    k = s * SS
    px, py = x * w, y * h
    pts = [(px + dx * k, py + 1 * k + 0.0042 * dx * dx * k) for dx in range(-170, 171, 10)]
    d.polygon(pts + [(px + 170 * k, h + 5), (px - 170 * k, h + 5)], fill=DARK)
    d.polygon(T(HORSE, px, py, k), fill=DARK)
    d.polygon(T([(-5, -23), (-4, -35), (-1, -41), (3, -35), (4, -23)], px, py, k), fill=DARK)
    d.ellipse([px - 4 * k, py - 49 * k, px + 2 * k, py - 41 * k], fill=DARK)
    d.polygon(T([(-1, -49), (-0.2, -53), (0.6, -49)], px, py, k), fill=DARK)
    d.line(T([(3, -34), (17, -78)], px, py, k), fill=DARK, width=max(2, int(0.9 * k)))
    d.polygon(T([(17, -78), (31, -74), (21, -69)], px, py, k), fill=DARK)
    return fg


def v_camp():
    return V.landscape(61, [(0, '#0b1230'), (0.30, '#26306a'), (0.55, '#7a5c86'), (0.72, '#e08a62'), (0.86, '#f4b06a'), (1, '#fad08e')],
                       (255, 175, 100), (0.50, 0.72),
                       [(0.58, 0.09, 0.55, '#8b6f9a', '#a8809a', 0.65), (0.66, 0.08, 0.45, '#5c5a8c', '#6e6a96', 0.5),
                        (0.75, 0.07, 0.35, '#383f70', '#2c3764', 0.35), (0.85, 0.05, 0.2, '#1a2548', '#121c3a', 0.2),
                        (0.94, 0.025, 0.1, '#0a1230', '#070d22', 0.0)],
                       (255, 190, 110), castle_x=0.84, castle_layer=1, mist_col='#e0b0a8', star_n=150, glow_amt=0.62,
                       pines=False, extra=camp)


def v_dragon():
    def ex(fg, w, h, rnd):
        return dragon(fg, w, h, rnd)
    return V.landscape(73, [(0, '#12091f'), (0.30, '#3a1a4a'), (0.55, '#9a3f4e'), (0.74, '#ec7a48'), (0.88, '#f8b25c'), (1, '#fdd486')],
                       (255, 150, 80), (0.62, 0.72),
                       [(0.58, 0.09, 0.6, '#7a4a6a', '#9a5a68', 0.7), (0.66, 0.08, 0.5, '#503a66', '#604268', 0.55),
                        (0.74, 0.07, 0.4, '#2e2858', '#262050', 0.4), (0.83, 0.05, 0.25, '#17183a', '#101230', 0.2),
                        (0.93, 0.03, 0.1, '#090c22', '#060818', 0.0)],
                       (255, 170, 90), castle_x=0.86, castle_layer=2, castle_col='#0b0f26', window=(255, 170, 80),
                       mist_col='#e69a7a', star_n=70, glow_amt=0.7, extra=ex)


def v_rider():
    def ex(fg, w, h, rnd):
        return rider(fg, w, h, rnd, x=0.18, y=0.77, s=2.0)
    return V.landscape(85, [(0, '#0a1330'), (0.30, '#1f3a72'), (0.55, '#5f78a6'), (0.72, '#d8a888'), (0.87, '#f6c98c'), (1, '#fde4a8')],
                       (255, 205, 130), (0.60, 0.72),
                       [(0.60, 0.09, 0.5, '#7d86ac', '#9a90a8', 0.6), (0.68, 0.08, 0.4, '#56668f', '#667396', 0.5),
                        (0.76, 0.065, 0.3, '#334670', '#2a3c64', 0.35), (0.86, 0.045, 0.15, '#1a2b4a', '#122240', 0.2),
                        (0.94, 0.025, 0.1, '#0a1530', '#070f24', 0.0)],
                       (255, 205, 130), castle_x=0.78, castle_layer=1, mist_col='#e8cfc0', star_n=90, glow_amt=0.55,
                       pines=False, extra=ex)


VARIANTS = {'bg_e_camp': v_camp, 'bg_f_dragon': v_dragon, 'bg_g_rider': v_rider}

if __name__ == '__main__':
    ap = argparse.ArgumentParser(); ap.add_argument('--out', default='bg_heroic'); a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    for n, f in VARIANTS.items():
        M.save_png8(f(), os.path.join(a.out, n + '.png'))
