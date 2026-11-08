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


def quad():
    s = Shape(28, 28, 12)
    x, y, z = 0, 0, 6
    s.box((x - 3.5, y - 3.5, z - 1.6), (x + 3.5, y + 3.5, z + 1.6), BODY)       # body
    s.box((x + 3.2, y - 1.2, z - 0.8), (x + 4.4, y + 1.2, z + 0.6), LENS)       # camera
    s.box((x - 1.0, y - 1.0, z - 3.2), (x + 1.0, y + 1.0, z - 1.4), ARM)        # payload pod
    for dx, dy in ((7, 7), (7, -7), (-7, 7), (-7, -7)):
        s.cyl((x, y, z), (x + dx, y + dy, z), 1.2, ARM)
        s.cyl((x + dx, y + dy, z + 1.2), (x + dx, y + dy, z + 1.9), 3.6, ROTOR)
    return s


def fibre():
    """Bastion fibre-optic drone: six short arms, a fat cable spool slung
    underneath and a long sensor boom - heavier and visibly different."""
    import math
    s = Shape(28, 28, 14)
    x, y, z = 0, 0, 8
    s.box((x - 2.6, y - 2.6, z - 1.2), (x + 2.6, y + 2.6, z + 1.4), ARM)        # dark hub
    s.box((x - 1.6, y - 1.6, z + 1.4), (x + 1.6, y + 1.6, z + 2.4), BODY)       # team-colour cap
    s.cyl((x, y, z - 1.4), (x, y, z - 4.6), 2.8, SPOOL)                          # cable spool
    s.cyl((x, y, z - 4.6), (x, y, z - 5.6), 0.6, ROTOR)                          # cable exit
    s.box((x + 2.6, y - 0.6, z - 0.6), (x + 7.0, y + 0.6, z + 0.4), ARM)        # sensor boom
    s.box((x + 6.6, y - 1.0, z - 1.0), (x + 7.8, y + 1.0, z + 0.6), LENS)
    for k in range(6):
        a = math.pi / 3 * k + math.pi / 6
        dx, dy = 6.2 * math.cos(a), 6.2 * math.sin(a)
        s.cyl((x, y, z), (x + dx, y + dy, z), 0.9, BODY)
        s.cyl((x + dx, y + dy, z + 0.9), (x + dx, y + dy, z + 1.5), 2.4, ROTOR)
    return s


def shahed():
    """Shahed-style one-way attack drone: cropped delta wing, pusher prop."""
    s = Shape(30, 30, 10)
    z = 5
    for i in range(14):                       # delta wing: wide at the back
        x = 6 - i
        half = 1.0 + i * 0.85
        s.box((x - 0.5, -half, z - 0.5), (x + 0.5, half, z + 0.4), BODY)
    s.box((-8.5, -9.0, z - 0.4), (-7.0, -8.0, z + 3.0), ARM)   # wingtip fins
    s.box((-8.5, 8.0, z - 0.4), (-7.0, 9.0, z + 3.0), ARM)
    s.cyl((8.0, 0, z), (-8.0, 0, z), 1.3, ARM)                 # fuselage
    s.box((8.0, -0.8, z - 0.8), (9.2, 0.8, z + 0.8), LENS)      # warhead nose
    s.cyl((-8.2, 0, z), (-9.0, 0, z), 3.0, ROTOR)              # pusher prop
    return s


def tictac():
    """TIC TAC: smooth white capsule, no wings, no visible engines, a thin
    team-colour stripe on top so you can tell whose it is."""
    s = Shape(30, 30, 12)
    z = 6
    s.cyl((-5.5, 0, z), (5.5, 0, z), 3.4, WHITE)
    for i, rad in enumerate((3.1, 2.6, 1.9, 1.0)):
        for sign in (1, -1):
            s.cyl((sign * (5.5 + i), 0, z), (sign * (6.5 + i), 0, z), rad, WHITE)
    s.box((-4.0, -0.5, z + 3.0), (4.0, 0.5, z + 3.5), BODY)
    s.box((-5.0, -0.4, z - 3.5), (5.0, 0.4, z - 3.1), GREY)
    return s


SPOOL = 58
WHITE, GREY = 34, 44
MODELS = {'sgpdrone': quad, 'sgfibre': fibre, 'sgshahed': shahed, 'sgtictac': tictac}


def build(install):
    OUT.mkdir(parents=True, exist_ok=True)
    for name, make in MODELS.items():
        grid, normals = make().finish(normal_table(4))
        write_vxl(name, OUT, grid, normals)
        for mod in install:
            for ext in ('.vxl', '.hva'):
                (mod / (name + ext)).write_bytes((OUT / (name + ext)).read_bytes())
        print(name, 'voxels', int((grid >= 0).sum()))


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--install', type=Path, action='append', default=[])
    build(ap.parse_args().install)
