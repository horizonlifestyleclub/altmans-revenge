"""v09 voxel toolkit: read/write RA2 VXL/HVA, recover the 244-entry RA2 normal
table by measuring the stock models, and build shaded models from code.

The engine lights each voxel by its normal index, so models built with a single
normal (as before v09) look flat. Here every surface voxel gets the table entry
nearest its true outward direction.
"""
from pathlib import Path
import json
import struct
import numpy as np

ROOT = __import__('sgpaths').ROOT
BASE = ROOT / 'work/singularity/base-local'
CACHE = ROOT / 'work/singularity/ra2_normals_%d.json'


def read_vxl(path):
    b = Path(path).read_bytes()
    assert b[:16] == b'Voxel Animation\0'
    nlimb, nlimb2, bodysize = struct.unpack_from('<III', b, 20)
    hdr = 32 + 770
    body = hdr + nlimb * 28
    tail = body + bodysize
    limbs = []
    for i in range(nlimb):
        t = tail + 92 * i
        so, eo, do = struct.unpack_from('<III', b, t)
        scale = struct.unpack_from('<f', b, t + 12)[0]
        bounds = struct.unpack_from('<6f', b, t + 64)
        nx, ny, nz, ntype = b[t + 88:t + 92]
        grid = np.zeros((nx, ny, nz), np.int16) - 1
        norm = np.zeros((nx, ny, nz), np.int16) - 1
        starts = struct.unpack_from('<%di' % (nx * ny), b, body + so)
        for k, st in enumerate(starts):
            if st < 0:
                continue
            x, y = k % nx, k // nx
            pos = body + do + st
            z = 0
            while z < nz:
                skip, count = b[pos], b[pos + 1]
                pos += 2
                z += skip
                for c in range(count):
                    if z < nz:
                        grid[x, y, z] = b[pos]
                        norm[x, y, z] = b[pos + 1]
                    pos += 2
                    z += 1
                pos += 1
        limbs.append(dict(grid=grid, norm=norm, scale=scale, bounds=bounds, ntype=ntype))
    return limbs


def outward(grid):
    """Approximate outward surface direction per filled voxel (occupancy gradient)."""
    occ = (grid >= 0).astype(float)
    p = np.pad(occ, 3)
    g = np.zeros(occ.shape + (3,))
    r = 3
    for dx in range(-r, r + 1):
        for dy in range(-r, r + 1):
            for dz in range(-r, r + 1):
                if dx == dy == dz == 0 or dx * dx + dy * dy + dz * dz > r * r:
                    continue
                v = np.array([dx, dy, dz], float)
                sh = p[3 + dx:3 + dx + occ.shape[0], 3 + dy:3 + dy + occ.shape[1], 3 + dz:3 + dz + occ.shape[2]]
                g += (1 - sh)[..., None] * v / np.linalg.norm(v)
    n = np.linalg.norm(g, axis=-1, keepdims=True)
    return np.where(n > 0, g / np.maximum(n, 1e-9), 0)


def normal_table(ntype=4):
    size = 244 if ntype == 4 else 36
    cache = Path(str(CACHE) % ntype)
    if cache.exists():
        return np.array(json.loads(cache.read_text()))
    acc = np.zeros((256, 3))
    cnt = np.zeros(256)
    for f in sorted(BASE.glob('*.vxl')):
        try:
            limbs = read_vxl(f)
        except Exception:
            continue
        for L in limbs:
            if L['ntype'] != ntype or L['grid'].size > 200000:
                continue
            d = outward(L['grid'])
            m = L['grid'] >= 0
            idx = L['norm'][m]
            v = d[m]
            ok = np.linalg.norm(v, axis=1) > 0.5
            np.add.at(acc, idx[ok], v[ok])
            np.add.at(cnt, idx[ok], 1)
    table = acc[:size] / np.maximum(1e-9, np.linalg.norm(acc[:size], axis=1, keepdims=True))
    cache.write_text(json.dumps(table.round(5).tolist()))
    print('normal table type %d from stock models; samples per entry min/median' % ntype, int(cnt[:size].min()), int(np.median(cnt[:size])))
    return table


def write_vxl(name, out_dir, grid, normals, scale=1 / 12, palette_from='htnk.vxl'):
    """Single-limb VXL + HVA. grid: (nx,ny,nz) colour index or -1; normals: same shape, index."""
    nx, ny, nz = grid.shape
    starts, ends, span = [], [], bytearray()
    for y in range(ny):
        for x in range(nx):
            col = grid[x, y]
            if (col < 0).all():
                starts.append(-1)
                ends.append(-1)
                continue
            starts.append(len(span))
            z = 0
            while z < nz:
                skip = 0
                while z < nz and col[z] < 0 and skip < 255:
                    skip += 1
                    z += 1
                run = []
                while z < nz and col[z] >= 0 and len(run) < 2 * 255:
                    run += [int(col[z]), int(normals[x, y, z])]
                    z += 1
                cnt = len(run) // 2
                span += bytes([skip, cnt] + run + [cnt])
            ends.append(len(span) - 1)
    n = nx * ny
    body = struct.pack('<%di' % n, *starts) + struct.pack('<%di' % n, *ends) + span
    pal = (BASE / palette_from).read_bytes()[32:802]
    head = b'Voxel Animation\0' + struct.pack('<4I', 1, 1, 1, len(body)) + pal + name.encode().ljust(16, b'\0') + struct.pack('<3I', 0, 1, 2)
    ident = struct.pack('<12f', 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0)
    s = 1.0 / scale / 12
    footer = struct.pack('<3If', 0, n * 4, n * 8, scale) + ident + struct.pack('<6f', -nx / 2, -ny / 2, 0, nx / 2, ny / 2, nz) + bytes([nx, ny, nz, 4])
    Path(out_dir, name + '.vxl').write_bytes(head + body + footer)
    Path(out_dir, name + '.hva').write_bytes(b'\0' * 16 + struct.pack('<II', 1, 1) + name.encode().ljust(16, b'\0') + ident)


class Shape:
    """Voxel modelling by primitives. Coordinates in voxels; x forward, y left, z up."""
    def __init__(self, nx, ny, nz):
        self.grid = np.full((nx, ny, nz), -1, np.int16)
        self.c = np.stack(np.meshgrid(np.arange(nx) - nx / 2 + .5, np.arange(ny) - ny / 2 + .5, np.arange(nz) + .5, indexing='ij'), -1)

    def box(self, lo, hi, color):
        x, y, z = self.c[..., 0], self.c[..., 1], self.c[..., 2]
        m = (x >= lo[0]) & (x <= hi[0]) & (y >= lo[1]) & (y <= hi[1]) & (z >= lo[2]) & (z <= hi[2])
        self.grid[m] = color
        return m

    def wedge(self, lo, hi, color, slope_front=0.0, slope_back=0.0):
        """Box whose top slopes down toward the front/back (armour glacis)."""
        x, y, z = self.c[..., 0], self.c[..., 1], self.c[..., 2]
        top = hi[2] - slope_front * np.maximum(0, x - (hi[0] - (hi[2] - lo[2]) / max(slope_front, 1e-6))) if slope_front else hi[2]
        if slope_back:
            top = np.minimum(top, hi[2] - slope_back * np.maximum(0, (lo[0] + (hi[2] - lo[2]) / slope_back) - x))
        m = (x >= lo[0]) & (x <= hi[0]) & (y >= lo[1]) & (y <= hi[1]) & (z >= lo[2]) & (z <= top)
        self.grid[m] = color

    def cyl(self, p0, p1, r, color):
        p0, p1 = np.array(p0, float), np.array(p1, float)
        v = p1 - p0
        L2 = v @ v
        d = self.c - p0
        t = np.clip((d @ v) / L2, 0, 1)
        dist = np.linalg.norm(d - t[..., None] * v, axis=-1)
        self.grid[dist <= r] = color

    def carve(self, lo, hi):
        x, y, z = self.c[..., 0], self.c[..., 1], self.c[..., 2]
        m = (x >= lo[0]) & (x <= hi[0]) & (y >= lo[1]) & (y <= hi[1]) & (z >= lo[2]) & (z <= hi[2])
        self.grid[m] = -1

    def finish(self, table):
        g = self.grid
        d = outward(g)
        idx = np.argmax(d @ table.T, axis=-1).astype(np.int16)
        # Hollow interior voxels never show; keep only the shell to stay small.
        occ = g >= 0
        p = np.pad(occ, 1)
        inner = occ.copy()
        for ax in range(3):
            for s in (-1, 1):
                inner &= np.roll(p, s, axis=ax)[1:-1, 1:-1, 1:-1]
        grid = np.where(inner, -1, g)
        return grid, np.where(grid >= 0, idx, 0)


def colours(path='htnk.vxl'):
    """Colour indices used by a stock model, most common first (for picking metal tones)."""
    L = read_vxl(BASE / path)[0]['grid']
    v, c = np.unique(L[L >= 0], return_counts=True)
    return [int(x) for x in v[np.argsort(-c)]]


if __name__ == '__main__':
    t = normal_table()
    print('table', t.shape, 'example', t[0], t[100])
    print('htnk colours', colours()[:24])


def preview(grid, normals, table, path, heading_deg=225, scale=4, ntype_table=None):
    """Quick isometric preview of a voxel model, lit from the upper left (for review only)."""
    from PIL import Image
    import math
    pal = np.array([min(255, v * 4) for v in (ROOT / 'work/singularity/sprites/unittem.pal').read_bytes()[:768]]).reshape(256, 3).astype(float)
    pal[16:32] = [(40 + 6 * (15 - i), 70 + 8 * (15 - i), 140 + 7 * (15 - i)) for i in range(16)]
    ix = np.argwhere(grid >= 0)
    nx, ny, nz = grid.shape
    p = ix - np.array([nx / 2, ny / 2, 0]) + 0.5
    a = math.radians(heading_deg)
    R = np.array([[math.cos(a), -math.sin(a), 0], [math.sin(a), math.cos(a), 0], [0, 0, 1]])
    p = p @ R.T
    nrm = table[normals[grid >= 0]] @ R.T
    light = np.array([-0.45, 0.55, 0.7])
    light /= np.linalg.norm(light)
    shade = 0.45 + 0.75 * np.clip(nrm @ light, 0, 1)
    col = pal[grid[grid >= 0]] * shade[:, None]
    W, H = int((nx + ny) * scale * 1.1), int((nx + ny) * scale * 0.6 + nz * scale * 1.1)
    sx = (W / 2 + p[:, 0] * scale).astype(int)
    sy = (H - nz * scale * 0.2 - (p[:, 1] * 0.5 + p[:, 2] * 0.866) * scale - (nx + ny) * scale * 0.2).astype(int)
    depth = -p[:, 1] * 0.866 + p[:, 2] * 0.5
    img = np.zeros((H, W, 3)) + (70, 96, 58)
    zb = np.full((H, W), -1e9)
    for k in np.argsort(depth):
        for dx in range(scale):
            for dy in range(scale):
                x, y = sx[k] + dx - scale // 2, sy[k] + dy - scale // 2
                if 0 <= x < W and 0 <= y < H:
                    img[y, x] = col[k]
    Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).save(path)
