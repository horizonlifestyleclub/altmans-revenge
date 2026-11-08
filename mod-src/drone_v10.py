"""v10 OPERATOR DRONE: one larger quadcopter (team-colour body, dark arms, light
rotor discs, sensor lens), built with the same voxel kit and art wiring as the
v09 Drone Swarm, which renders correctly as a hovering vehicle. Replaces the
tiny borrowed aircraft model the operator drone used before.
Run: drone_v10.py [--install MODDIR]"""
from pathlib import Path
import argparse
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from voxel_kit import Shape, normal_table, write_vxl

OUT = __import__('sgpaths').ROOT / 'outputs/singularity-build/v10/models'
BODY, ARM, ROTOR, LENS = 20, 61, 54, 9


def build(install):
    s = Shape(28, 28, 12)
    x, y, z = 0, 0, 6
    s.box((x - 3.5, y - 3.5, z - 1.6), (x + 3.5, y + 3.5, z + 1.6), BODY)       # body
    s.box((x + 3.2, y - 1.2, z - 0.8), (x + 4.4, y + 1.2, z + 0.6), LENS)       # camera
    s.box((x - 1.0, y - 1.0, z - 3.2), (x + 1.0, y + 1.0, z - 1.4), ARM)        # payload pod
    for dx, dy in ((7, 7), (7, -7), (-7, 7), (-7, -7)):
        s.cyl((x, y, z), (x + dx, y + dy, z), 1.2, ARM)
        s.cyl((x + dx, y + dy, z + 1.2), (x + dx, y + dy, z + 1.9), 3.6, ROTOR)
    grid, normals = s.finish(normal_table(4))
    OUT.mkdir(parents=True, exist_ok=True)
    write_vxl('sgpdrone', OUT, grid, normals)
    for mod in install:
        for ext in ('.vxl', '.hva'):
            (mod / ('sgpdrone' + ext)).write_bytes((OUT / ('sgpdrone' + ext)).read_bytes())
    print('sgpdrone voxels', int((grid >= 0).sum()))


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--install', type=Path, action='append', default=[])
    build(ap.parse_args().install)
