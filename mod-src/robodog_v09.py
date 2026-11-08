"""v09 robot dog (GHOSTHOUND): original quadruped model rendered into the game's
DogSequence frame layout (172 body frames + 172 shadow frames, 74x72).

The model is built from boxes in code, posed per animation frame, projected with
the RA2 isometric camera, lit from the upper left, supersampled 4x and reduced to
the unit palette. Team-colour stripes use the remap range (16-31).
Run: robodog_v09.py [--install MODDIR]
"""
from pathlib import Path
import argparse
import math
import struct
import numpy as np
from PIL import Image

ROOT = __import__('sgpaths').ROOT
SRC = ROOT / 'work/singularity'
OUT = ROOT / 'outputs/singularity-build/v09'
PAL = np.array([min(255, v * 4) for v in (SRC / 'sprites/unittem.pal').read_bytes()[:768]]).reshape(256, 3)
W, H, SS = 74, 72, 4
OX, OY = 37.0, 42.0          # ground point under the body centre
SCALE = 1.0                  # model units are final-size pixels

# Materials: base colour and how they are reduced to the palette.
MAT = {
    'hull': (176, 174, 162), 'plate': (126, 126, 116), 'dark': (44, 44, 42), 'joint': (72, 70, 64),
    'team': (255, 255, 255), 'lens': (90, 230, 255), 'warn': (230, 180, 40),
}
SAMPLE = 1.0 / SS * 0.9


def rot(axis, a):
    c, s = math.cos(a), math.sin(a)
    if axis == 'z':
        return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])
    if axis == 'y':
        return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])
    return np.array([[1, 0, 0], [0, c, -s], [0, s, c]])


class Model:
    def __init__(self):
        self.pts, self.nrm, self.mat = [], [], []

    def box(self, centre, half, R, mat):
        """Surface samples of an oriented box (R columns = box axes in model space)."""
        centre = np.asarray(centre, float)
        hx, hy, hz = half
        for axis, sign in [(0, 1), (0, -1), (1, 1), (1, -1), (2, 1), (2, -1)]:
            others = [i for i in range(3) if i != axis]
            ha, hb = half[others[0]], half[others[1]]
            ua = np.arange(-ha, ha + 1e-6, SAMPLE)
            ub = np.arange(-hb, hb + 1e-6, SAMPLE)
            A, B = np.meshgrid(ua, ub)
            local = np.zeros((A.size, 3))
            local[:, axis] = sign * half[axis]
            local[:, others[0]] = A.ravel()
            local[:, others[1]] = B.ravel()
            n = np.zeros(3)
            n[axis] = sign
            self.pts.append(centre + local @ R.T)
            self.nrm.append(np.tile(R @ n, (A.size, 1)))
            self.mat += [mat] * A.size

    def seg(self, p0, p1, w, d, mat):
        p0, p1 = np.asarray(p0, float), np.asarray(p1, float)
        v = p1 - p0
        L = np.linalg.norm(v)
        x = v / L
        up = np.array([0, 0, 1.0]) if abs(x[2]) < 0.9 else np.array([1.0, 0, 0])
        y = np.cross(up, x)
        y /= np.linalg.norm(y)
        z = np.cross(x, y)
        self.box((p0 + p1) / 2, (L / 2, w / 2, d / 2), np.stack([x, y, z], 1), mat)


def dog(pose):
    """pose: dict with bob, pitch, legs[(thigh, shin) x4], head_yaw, mast, crouch, lean."""
    m = Model()
    z0 = 9.5 + pose.get('bob', 0) - pose.get('crouch', 0)
    Rb = rot('y', pose.get('pitch', 0)) @ rot('x', pose.get('roll', 0))
    body = lambda p: np.array([0, 0, z0]) + Rb @ np.asarray(p, float)
    # Hull: armoured torso, raised spine plate, chunky shoulder and hip housings.
    m.box(body((0, 0, 0)), (6.8, 3.2, 2.6), Rb, 'hull')
    m.box(body((0.3, 0, 3.0)), (5.0, 2.2, 0.7), Rb, 'plate')
    m.box(body((5.6, 0, 0.4)), (1.8, 3.7, 2.4), Rb, 'plate')        # shoulders
    m.box(body((-5.6, 0, 0.0)), (1.8, 3.6, 2.2), Rb, 'plate')       # hips
    for s in (1, -1):
        m.box(body((0, s * 3.3, -0.2)), (4.4, 0.25, 1.3), Rb, 'team')      # team-colour side panel
        m.box(body((-2.0, s * 3.3, 1.6)), (1.0, 0.25, 0.45), Rb, 'warn')   # hazard plate (explosive)
    # Sensor head on a raised neck: wedge block, dark visor, twin lenses.
    Rh = Rb @ rot('z', pose.get('head_yaw', 0)) @ rot('y', pose.get('head_pitch', 0) - 0.15)
    hc = body((9.4, 0, 2.6))
    m.seg(body((6.5, 0, 1.0)), hc - Rh @ np.array([1.8, 0, 0]), 2.2, 2.0, 'joint')
    m.box(hc, (2.4, 2.2, 1.7), Rh, 'hull')
    m.box(hc + Rh @ np.array([0.3, 0, 1.8]), (1.8, 1.6, 0.25), Rh, 'plate')
    m.box(hc + Rh @ np.array([2.45, 0, 0.1]), (0.15, 1.9, 0.75), Rh, 'dark')
    for s in (1, -1):
        m.box(hc + Rh @ np.array([2.65, s * 0.9, 0.15]), (0.15, 0.5, 0.45), Rh, 'lens')
    # Rear sensor mast (antenna) and small dish.
    mast = pose.get('mast', 0)
    base = body((-5.6, 0, 2.4))
    tip = base + Rb @ rot('z', mast) @ np.array([-0.8, 0, 5.0])
    m.seg(base, tip, 0.6, 0.6, 'dark')
    m.box(tip, (0.6, 1.1, 0.35), Rb @ rot('z', mast), 'lens' if pose.get('blink') else 'plate')
    # Four legs: hip, knee, foot; knees point backwards (robot quadruped).
    hips = [(5.8, 3.4), (5.8, -3.4), (-5.8, 3.4), (-5.8, -3.4)]
    for (hx, hy), (a1, a2) in zip(hips, pose['legs']):
        hip = body((hx, hy, -1.0))
        knee = hip + np.array([math.sin(a1) * 5.2, 0, -math.cos(a1) * 5.2])
        foot = knee + np.array([math.sin(a1 - a2) * 5.0, 0, -math.cos(a1 - a2) * 5.0])
        m.box(hip, (1.5, 1.1, 1.5), np.eye(3), 'joint')
        m.seg(hip, knee, 2.2, 2.4, 'plate')
        m.box(knee, (0.9, 1.0, 0.9), np.eye(3), 'joint')
        m.seg(knee, foot, 1.5, 1.6, 'dark')
        m.box(foot + np.array([0.3, 0, 0.35]), (1.1, 1.0, 0.4), np.eye(3), 'dark')
    return m


def render(model, heading, extra=None):
    P = np.concatenate(model.pts)
    N = np.concatenate(model.nrm)
    mats = np.array(model.mat)
    Rz = rot('z', heading)
    P = P @ Rz.T
    N = N @ Rz.T
    if extra:
        P, N, mats = extra(P, N, mats)
    # Ground coordinates: x right, y away from the viewer (screen up); z up.
    sx = OX + P[:, 0] * SCALE
    sy = OY - P[:, 1] * 0.5 * SCALE - P[:, 2] * 0.866 * SCALE
    depth = -P[:, 1] * 0.866 + P[:, 2] * 0.5          # nearer the viewer = larger
    light = np.array([-0.45, 0.55, 0.70])
    light /= np.linalg.norm(light)
    lam = np.clip(N @ light, 0, 1)
    shade = 0.5 + 0.62 * lam
    ix = np.floor(sx * SS).astype(int)
    iy = np.floor(sy * SS).astype(int)
    ok = (ix >= 0) & (ix < W * SS) & (iy >= 0) & (iy < H * SS)
    zbuf = np.full((H * SS, W * SS), -1e9)
    cbuf = np.zeros((H * SS, W * SS, 3))
    team = np.zeros((H * SS, W * SS), bool)
    order = np.argsort(depth[ok])
    sel = np.nonzero(ok)[0][order]
    base = np.array([MAT[m] for m in mats[sel]], float)
    s = shade[sel][:, None]
    col = np.where((mats[sel] == 'lens')[:, None], base, base * s)
    yy, xx = iy[sel], ix[sel]
    zbuf[yy, xx] = depth[sel]           # later (nearer) writes win
    cbuf[yy, xx] = col
    team[yy, xx] = mats[sel] == 'team'
    cov = (zbuf > -1e9).reshape(H, SS, W, SS).mean(axis=(1, 3))
    teamfrac = team.reshape(H, SS, W, SS).mean(axis=(1, 3))
    filled = (zbuf > -1e9)[..., None]
    rgb = (cbuf * filled).reshape(H, SS, W, SS, 3).sum(axis=(1, 3)) / np.maximum(1, filled.reshape(H, SS, W, SS).sum(axis=(1, 3)))[..., None]
    return cov, rgb, teamfrac


ALLOWED = np.array([i for i in range(32, 224)])


def to_indexed(cov, rgb, teamfrac):
    out = np.zeros((H, W), np.uint8)
    pal = PAL[ALLOWED].astype(float)
    solid = cov >= 0.45
    teampx = solid & (teamfrac > 0.3 * cov)
    d = ((rgb[:, :, None, :] - pal[None, None]) ** 2).sum(-1)
    out[solid] = ALLOWED[d.argmin(-1)][solid]
    bright = np.clip(rgb.mean(-1) / 255.0, 0, 1)
    out[teampx] = (16 + np.round((1 - bright) * 15)).astype(np.uint8)[teampx]
    return out


def shadow(cov):
    """Soft ground shadow: the silhouette squashed onto the ground, offset to the lower right."""
    m = (cov > 0.3)
    out = np.zeros((H, W), np.uint8)
    ys, xs = np.nonzero(m)
    for y, x in zip(ys, xs):
        gy = int(OY + (y - OY) * 0.25) + 1
        gx = x + int((OY - y) * 0.35)
        if 0 <= gx < W and 0 <= gy < H:
            out[gy, gx] = 1
    return out


def heading_for(k):
    # Frame facings run N, NW, W, SW, S, SE, E, NE on screen (measured on adog.shp).
    return math.radians(90 + 45 * k)


STAND = [(0.25, 0.55)] * 4


def walk(t):
    legs = []
    for i, phase in enumerate([0, 0.5, 0.5, 0]):   # trot: diagonal pairs
        a = 2 * math.pi * (t + phase)
        legs.append((0.25 + 0.45 * math.sin(a), 0.55 + 0.35 * max(0, math.cos(a))))
    return dict(legs=legs, bob=0.5 * abs(math.sin(2 * math.pi * t * 2)), pitch=0.03 * math.sin(2 * math.pi * t))


def build():
    body, shadows = [None] * 172, [None] * 172

    def put(i, pose, k, extra=None):
        cov, rgb, team = render(dog(pose), heading_for(k), extra)
        body[i] = to_indexed(cov, rgb, team)
        shadows[i] = shadow(cov)

    for k in range(8):                                     # Ready/Guard 0..7
        put(k, dict(legs=STAND, mast=0.3), k)
    for k in range(8):                                     # Walk 8 + 6k
        for f in range(6):
            put(8 + 6 * k + f, walk(f / 6), k)
    for f in range(15):                                    # Idle1 (S): head scan, lens blink
        put(56 + f, dict(legs=STAND, head_yaw=0.6 * math.sin(2 * math.pi * f / 15), blink=f % 5 == 0, mast=0.3), 4)
    for f in range(15):                                    # Idle2 (E): mast sweep
        put(71 + f, dict(legs=STAND, mast=2 * math.pi * f / 15, head_pitch=-0.15), 6)
    for f in range(15):                                    # Die1 (S): legs fold, collapses
        t = f / 14
        legs = [(0.25 + 1.0 * t, 0.55 + 1.6 * t)] * 4
        put(86 + f, dict(legs=legs, crouch=6.5 * t, roll=0.5 * t, head_pitch=0.5 * t), 4)
    for f in range(15):                                    # Die2 (S): blown apart, parts scatter
        t = f / 14
        def scatter(P, N, mats, t=t):
            P = P.copy()
            r = np.linalg.norm(P[:, :2], axis=1, keepdims=True) + 1
            P[:, :2] += P[:, :2] / r * 9 * t
            P[:, 2] = np.maximum(0.2, P[:, 2] + 10 * t - 22 * t * t)
            keep = (np.arange(len(P)) % 7) >= int(6 * t)
            return P[keep], N[keep], mats[keep]
        put(101 + f, dict(legs=[(0.6, 1.2)] * 4, crouch=3 * t), 4, scatter)
    for k in range(8):                                     # FireUp 116 + 6k: crouch and lunge
        for f in range(6):
            t = f / 5
            lunge = math.sin(math.pi * t)
            legs = [(0.25 - 0.7 * lunge, 0.55 + 0.2 * lunge)] * 2 + [(0.25 + 0.6 * lunge, 0.9 * lunge + 0.3)] * 2
            put(116 + 6 * k + f, dict(legs=legs, pitch=-0.25 * lunge, bob=2.5 * lunge, blink=f in (2, 3)), k)
    for f in range(8):                                     # Cheer 164 (E): mast spin + hop
        put(164 + f, dict(legs=STAND, mast=2 * math.pi * f / 8, bob=1.2 * abs(math.sin(math.pi * f / 4)), blink=f % 2 == 0), 6)
    return body + shadows


def shp_write(path, frames):
    n = len(frames)
    head = bytearray(struct.pack('<4H', 0, W, H, n))
    blob = bytearray()
    off = 8 + n * 24
    for a in frames:
        ys, xs = np.nonzero(a)
        if not len(xs):
            head += struct.pack('<4H4I', 0, 0, 0, 0, 1, 0, 0, 0)
            continue
        x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
        tile = a[y0:y1, x0:x1]
        head += struct.pack('<4H4I', x0, y0, x1 - x0, y1 - y0, 1, 0, 0, off + len(blob))
        blob += tile.tobytes()
    path.write_bytes(bytes(head + blob))


def preview(frames, path):
    pal = PAL.copy()
    pal[16:32] = [(int(60 + 12 * (15 - i) * 0.9), int(80 + 10 * (15 - i)), 255 - 8 * i) for i in range(16)]  # blue team
    pick = list(range(0, 8)) + [8 + 6 * k + 2 for k in range(8)] + [116 + 6 * k + 3 for k in range(8)] + [56, 60, 71, 75, 86, 92, 100, 101, 105, 110, 164, 167]
    tile = 74
    sheet = Image.new('RGB', (tile * 8, tile * 5), (70, 96, 58))
    for n, i in enumerate(pick[:40]):
        sh = frames[172 + i]
        a = frames[i]
        im = Image.new('RGB', (W, H), (70, 96, 58))
        px = im.load()
        for y in range(H):
            for x in range(W):
                if sh[y, x]:
                    px[x, y] = (40, 56, 34)
                if a[y, x]:
                    px[x, y] = tuple(int(v) for v in pal[a[y, x]])
        sheet.paste(im, ((n % 8) * tile, (n // 8) * tile))
    sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save(path)


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--install', type=Path, action='append', default=[])
    args = ap.parse_args()
    frames = build()
    OUT.mkdir(parents=True, exist_ok=True)
    shp_write(OUT / 'sghound.shp', frames)
    preview(frames, OUT / 'robodog-preview.png')
    for mod in args.install:
        (mod / 'sghound.shp').write_bytes((OUT / 'sghound.shp').read_bytes())
    print('Robot dog: %d frames (74x72), preview %s, installed %d' % (len(frames), OUT / 'robodog-preview.png', len(args.install)))
