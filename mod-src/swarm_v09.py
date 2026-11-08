"""v09 DRONE SWARM: one voxel unit showing a loose cloud of seven small
quadcopters (team-colour bodies, dark arms, light rotor discs).
Run: swarm_v09.py [--install MODDIR]"""
from pathlib import Path
import argparse
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from voxel_kit import Shape, normal_table, write_vxl

OUT = __import__('sgpaths').ROOT / 'outputs/singularity-build/v09/models'
BODY, ARM, ROTOR, LENS = 20, 61, 54, 9
DRONES = [(0, 0, 9), (9, 6, 6), (-8, 7, 11), (7, -8, 12), (-9, -6, 5), (14, -1, 10), (-2, 13, 8)]


def build(install):
    s = Shape(40, 40, 18)
    for x, y, z in DRONES:
        s.box((x - 1.6, y - 1.6, z - 0.8), (x + 1.6, y + 1.6, z + 0.8), BODY)
        s.box((x + 1.4, y - 0.6, z - 0.3), (x + 2.0, y + 0.6, z + 0.3), LENS)
        for dx, dy in ((3, 3), (3, -3), (-3, 3), (-3, -3)):
            s.cyl((x, y, z), (x + dx, y + dy, z), 0.7, ARM)
            s.cyl((x + dx, y + dy, z + 0.6), (x + dx, y + dy, z + 1.0), 1.6, ROTOR)
    grid, normals = s.finish(normal_table(4))
    OUT.mkdir(parents=True, exist_ok=True)
    write_vxl('sgswarm', OUT, grid, normals)
    for mod in install:
        for ext in ('.vxl', '.hva'):
            (mod / ('sgswarm' + ext)).write_bytes((OUT / ('sgswarm' + ext)).read_bytes())
    print('sgswarm voxels', int((grid >= 0).sum()))


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--install', type=Path, action='append', default=[])
    build(ap.parse_args().install)
