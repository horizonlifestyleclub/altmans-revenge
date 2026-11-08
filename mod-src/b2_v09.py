"""v09 B-2 flying wing: original voxel model, nose along +x (the old model was
built rotated, so it appeared to fly sideways). Swept leading edges, sawtooth
trailing edge, raised centre body with cockpit and dorsal intakes, small team
marks on the wingtips. Run: b2_v09.py [--install MODDIR]"""
from pathlib import Path
import argparse
import sys
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parent))
from voxel_kit import Shape, normal_table, write_vxl

OUT = __import__('sgpaths').ROOT / 'outputs/singularity-build/v09/models'
SKIN, INTAKE, GLASS, TEAM, EDGE = 58, 61, 13, 20, 55


def build(install):
    table = normal_table(4)
    nx, ny, nz = 34, 60, 9
    s = Shape(nx, ny, nz)
    x, y, z = s.c[..., 0], s.c[..., 1], s.c[..., 2]
    ay = np.abs(y)
    x_le = 13 - ay * 0.62                                   # swept leading edge
    saw = np.abs(((ay / 7.0) % 2) - 1)                      # 1 at 0,14,28; 0 at 7,21
    x_te = -9 + 5 * (1 - saw)                               # sawtooth trailing edge
    x_te = np.minimum(x_te, x_le - 2.5)
    thick = 2.4 - ay / 26 * 1.6                              # thins toward the tips
    inside = (x <= x_le) & (x >= x_te) & (ay <= 28.5)
    s.grid[inside & (z >= 2) & (z <= 2 + thick)] = SKIN
    s.grid[inside & (z >= 2) & (z <= 2.6) & ((x_le - x) < 1.2)] = EDGE      # leading-edge strip
    hump = (ay < 6) & (x > -5) & (x < x_le - 1) & (z >= 2) & (z <= 5.2 - ay * 0.25 - np.maximum(0, x - 6) * 0.3)
    s.grid[hump] = SKIN
    s.grid[(ay < 2.2) & (x > 6.5) & (x < 9.5) & (z > 4.0) & (z < 5.4) & (s.grid >= 0)] = GLASS   # cockpit
    for side in (-1, 1):                                     # dorsal intakes
        m = (np.abs(y - side * 6) < 1.8) & (x > 0) & (x < 4) & (z > 3.4) & (z < 4.8)
        s.grid[m] = INTAKE
        tip = (np.abs(y - side * 26.5) < 1.5) & inside & (z >= 2) & (z <= 3)
        s.grid[tip] = TEAM
    grid, normals = s.finish(table)
    OUT.mkdir(parents=True, exist_ok=True)
    write_vxl('sgb2', OUT, grid, normals)
    for mod in install:
        for ext in ('.vxl', '.hva'):
            (mod / ('sgb2' + ext)).write_bytes((OUT / ('sgb2' + ext)).read_bytes())
    print('sgb2 voxels', int((grid >= 0).sum()))


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--install', type=Path, action='append', default=[])
    build(ap.parse_args().install)
