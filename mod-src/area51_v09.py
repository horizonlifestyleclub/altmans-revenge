"""v09 AREA 51: a cold grey variant of the Airforce Command with its own antenna.

Building and pad animations are recoloured to neutral greys (team colour kept).
The rotating radar dish is replaced by a tall lattice mast carrying a large
upward-facing dish that slowly turns, with a blinking red warning light.
Files: ggsga.shp, ggsga_a.shp, ggsga_b.shp, ggsga_c.shp (generic theatre).
Run: area51_v09.py [--install MODDIR]
"""
from pathlib import Path
import argparse
import math
import struct
import numpy as np
from PIL import Image

ROOT = __import__('sgpaths').ROOT
SRC = ROOT / 'work/singularity'
OUT = ROOT / 'outputs/singularity-build/v09/area51'
PAL = np.array([min(255, v * 4) for v in (SRC / 'sprites/unittem.pal').read_bytes()[:768]]).reshape(256, 3).astype(float)
_src = (SRC / 'assets_v05.py').read_text()
UP = list(PAL.astype(int).ravel())
exec(_src[_src.index('def shp_read'):_src.index('def shp_write')])


def shp_write(path, frames):
    w, h = frames[0].shape[1], frames[0].shape[0]
    n = len(frames)
    head = bytearray(struct.pack('<4H', 0, w, h, n))
    blob = bytearray()
    off = 8 + n * 24
    for a in frames:
        ys, xs = np.nonzero(a)
        if not len(xs):
            head += struct.pack('<4H4I', 0, 0, 0, 0, 1, 0, 0, 0)
            continue
        x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
        head += struct.pack('<4H4I', x0, y0, x1 - x0, y1 - y0, 1, 0, 0, off + len(blob))
        blob += a[y0:y1, x0:x1].tobytes()
    path.write_bytes(bytes(head + blob))


def grey_lut():
    """Each palette entry -> the nearest neutral grey of equal brightness, slightly cooled."""
    lum = PAL @ np.array([0.3, 0.59, 0.11])
    neutral = [i for i in range(32, 224) if np.ptp(PAL[i]) < 18]
    lut = np.arange(256)
    for i in range(256):
        if i < 2 or 16 <= i <= 31:
            continue
        target = lum[i] * 0.92
        lut[i] = min(neutral, key=lambda j: abs(lum[j] - target))
    return lut.astype(np.uint8)


def greyed(name, lut, shadow_from):
    frames = [np.array(f) for f in shp_read(SRC / 'sprites' / (name + '.shp'))]
    return [lut[f] if i < shadow_from else f for i, f in enumerate(frames)]


def antenna(frames_normal=8):
    """Lattice mast + upward dish, rendered as points, 188x162 canvas like the stock anim."""
    W, H = 188, 162
    base = np.array([82.0, 64.0])            # mast foot on the tower roof (screen)
    out = []
    neutral = [i for i in range(32, 224) if np.ptp(PAL[i]) < 18]
    lum = PAL @ np.array([0.3, 0.59, 0.11])

    def grey(v):
        return min(neutral, key=lambda j: abs(lum[j] - v))
    for damaged in (False, True):
        for f in range(frames_normal):
            img = np.zeros((H, W), np.uint8)
            pts = []
            # Lattice mast: two rails with cross bracing, 34 px tall.
            for z in np.arange(0, 34, 0.5):
                for side in (-2.2, 2.2):
                    pts.append((side, 0, z, 150 if side < 0 else 95))
                if int(z * 2) % 6 == 0:
                    for t in np.arange(-2.2, 2.2, 0.5):
                        pts.append((t, 0, z + (t + 2.2) * 0.9, 120))
            # Dish: shallow paraboloid facing up and slightly toward the sweep angle.
            ang = 2 * math.pi * f / frames_normal + (0.6 if damaged else 0)
            tilt = 0.9 if not damaged else 1.5
            for r in np.arange(0, 11, 0.5):
                for th in np.arange(0, 2 * math.pi, 0.12):
                    x, y = r * math.cos(th), r * math.sin(th)
                    z = 0.06 * r * r
                    # tilt about the x axis, then rotate about the mast
                    y2, z2 = y * math.cos(tilt) - z * math.sin(tilt), y * math.sin(tilt) + z * math.cos(tilt)
                    xr, yr = x * math.cos(ang) - y2 * math.sin(ang), x * math.sin(ang) + y2 * math.cos(ang)
                    face = 0.55 + 0.45 * math.cos(th - ang - 2.2)
                    pts.append((xr, yr, 36 + z2, 60 + 150 * face))
            for p in pts:
                x, y, z, v = p
                sx, sy = int(base[0] + x), int(base[1] + y * 0.5 - z)
                if 0 <= sx < W and 0 <= sy < H:
                    img[sy, sx] = grey(v * (0.6 if damaged else 1.0))
            if f % 4 < 2 and not damaged:       # blinking warning light on top
                img[int(base[1] - 35):int(base[1] - 33), int(base[0]) - 1:int(base[0]) + 1] = 8
            out.append(img)
    shadows = []
    for img in out:
        s = np.zeros_like(img)
        ys, xs = np.nonzero(img)
        for y, x in zip(ys, xs):
            gy = int(base[1] + (y - base[1]) * -0.25) + 2
            gx = x + int((base[1] - y) * 0.6)
            if 0 <= gx < W and 0 <= gy < H:
                s[gy, gx] = 1
        shadows.append(s)
    return out + shadows


def build(install):
    OUT.mkdir(parents=True, exist_ok=True)
    lut = grey_lut()
    files = {
        'ggsga.shp': greyed('ggairc', lut, 3),
        'ggsga_a.shp': greyed('ggairc_a', lut, 8),
        'ggsga_b.shp': greyed('ggairc_b', lut, 12),
        'ggsga_c.shp': antenna(),
        'ggsgabb.shp': greyed('ggaircbb', lut, 10 ** 6),
    }
    for name, frames in files.items():
        shp_write(OUT / name, frames)
        for mod in install:
            (mod / name).write_bytes((OUT / name).read_bytes())
    # Preview: building + both overlay layers + antenna, normal state.
    canvas = Image.new('RGB', (188, 162), (70, 96, 58))
    pal = PAL.copy()
    pal[16:32] = [(40 + 6 * (15 - i), 70 + 8 * (15 - i), 140 + 7 * (15 - i)) for i in range(16)]
    for frames, k in ((files['ggsga.shp'], 0), (files['ggsga_b.shp'], 0), (files['ggsga_a.shp'], 0), (files['ggsga_c.shp'], 0)):
        a = frames[k]
        rgb = Image.fromarray(pal[a].astype(np.uint8))
        canvas.paste(rgb, (0, 0), Image.fromarray(((a > 1) * 255).astype(np.uint8)))
    canvas.resize((188 * 3, 162 * 3), Image.NEAREST).save(OUT / 'area51-preview.png')
    print('Area 51 files:', ', '.join(files), '| installed', len(install))


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--install', type=Path, action='append', default=[])
    build(ap.parse_args().install)
