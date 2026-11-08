"""v09 BASTION drone carrier: original voxel models (mobile and deployed).

Mobile: tracked armoured hull, front cab with vision slit, whip antennas, rear
launch deck with two FPV drones, and fibre-optic cable spools on the flanks.
Deployed: stabiliser jacks down, telescopic mast up with a crossbar antenna array
and dish, launch deck open with its drone racks.
Run: bastion_v09.py [--install MODDIR]
"""
from pathlib import Path
import argparse
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from voxel_kit import Shape, normal_table, write_vxl

OUT = __import__('sgpaths').ROOT / 'outputs/singularity-build/v09/models'
HULL, PLATE, DARK, TRACK, OLIVE, TEAM, BLACK, LIGHT, STEEL = 54, 58, 61, 62, 72, 20, 13, 9, 51


def hull(s, deployed):
    s.box((-25, -16, 0), (24, -10.5, 7), TRACK)              # tracks
    s.box((-25, 10.5, 0), (24, 16, 7), TRACK)
    for x in range(-20, 21, 8):                                # road wheels (outer hubs)
        for y in (-16.5, 16.5):
            s.cyl((x, y - 0.5, 3.5), (x, y + 0.5, 3.5), 2.6, STEEL)
    s.wedge((-26, -11, 3), (26, 11, 11), HULL, slope_front=0.9)  # lower hull with glacis
    for y in (-16, 16):                                          # side skirts, team stripe on top
        s.box((-24, y - 0.6 if y < 0 else y - 0.4, 5), (22, y + 0.4 if y < 0 else y + 0.6, 7.5), PLATE)
        s.box((-24, y - 0.6 if y < 0 else y - 0.4, 7.5), (22, y + 0.4 if y < 0 else y + 0.6, 9), TEAM)
    s.wedge((8, -9, 11), (24, 9, 17), HULL, slope_front=1.2)      # cab
    s.box((22.5, -7, 13.2), (23.5, 7, 14.4), BLACK)               # vision slit
    s.box((-26, -10, 11), (7, 10, 12.5), PLATE)                   # rear launch deck
    for y in (-9.5, 9.5):                                          # fibre-optic spools on the flanks
        s.cyl((-25, y, 15.5), (-19, y, 15.5), 3.4, OLIVE)
        s.cyl((-25.3, y, 15.5), (-18.7, y, 15.5), 1.2, DARK)
        s.box((-22.5, y - 0.4, 12), (-21.5, y + 0.4, 15), DARK)
    if not deployed:
        for x in (-14, -3):                                       # two FPV drones on their pads
            s.box((x - 4, -4, 12.5), (x + 4, 4, 13), DARK)
            s.box((x - 1.2, -1.2, 13), (x + 1.2, 1.2, 14.3), OLIVE)
            s.cyl((x - 3, -3, 13.6), (x + 3, 3, 13.6), 0.75, DARK)
            s.cyl((x - 3, 3, 13.6), (x + 3, -3, 13.6), 0.75, DARK)
            for dx in (-3, 3):
                for dy in (-3, 3):
                    s.cyl((x + dx, dy, 14.0), (x + dx, dy, 14.4), 1.3, STEEL)
        for y, h in ((-7, 25), (7, 23)):                         # whip antennas
            s.cyl((14, y, 17), (12, y, h), 0.8, BLACK)
        s.cyl((18, 0, 17), (18, 0, 21), 0.8, DARK)               # short mast + sensor block
        s.box((16.5, -1.6, 21), (19.5, 1.6, 23), PLATE)
        s.box((19.5, -0.8, 21.6), (20, 0.8, 22.4), LIGHT)
    else:
        for x in (-23, 21):                                       # stabiliser jacks
            for y in (-14, 14):
                s.cyl((x, y, 9), (x, y * 1.15, 0.5), 0.9, STEEL)
                s.box((x - 1.5, y * 1.15 - 1.5, 0), (x + 1.5, y * 1.15 + 1.5, 0.8), DARK)
        for x in (-16, -8, 0):                                    # open drone racks
            s.box((x - 3, -8, 12.5), (x + 3, 8, 14), DARK)
            s.box((x - 0.5, -8, 14), (x + 0.5, 8, 18), STEEL)
        s.cyl((14, 0, 17), (14, 0, 37), 1.4, STEEL)               # telescopic mast
        s.cyl((14, 0, 17), (14, 0, 25), 2.0, DARK)
        for z, w in ((36, 9), (32, 7), (28, 5)):                  # crossbar antenna array
            s.box((13.4, -w, z - 0.4), (14.6, w, z + 0.4), BLACK)
            for y in (-w, w):
                s.cyl((14, y, z), (14, y, z + 3), 0.8, BLACK)
        s.cyl((16, 0, 30), (19, 0, 33), 2.6, PLATE)               # dish
        s.box((14.6, -0.6, 37), (15.4, 0.6, 38.4), LIGHT)         # beacon
        for y, h in ((-7, 27), (7, 25)):
            s.cyl((19, y, 17), (17, y, h), 0.8, BLACK)


def build(install):
    table = normal_table(4)
    OUT.mkdir(parents=True, exist_ok=True)
    for name, deployed, nz in (('sgbastion', False, 27), ('sgbastiond', True, 40)):
        s = Shape(56, 40, nz)
        hull(s, deployed)
        grid, normals = s.finish(table)
        write_vxl(name, OUT, grid, normals)
        for mod in install:
            for ext in ('.vxl', '.hva'):
                (mod / (name + ext)).write_bytes((OUT / (name + ext)).read_bytes())
        print(name, 'voxels', int((grid >= 0).sum()))


if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--install', type=Path, action='append', default=[])
    build(ap.parse_args().install)
