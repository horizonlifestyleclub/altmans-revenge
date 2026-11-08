"""v10 verification lab: private mod copy + private executable. Normal profiles untouched.
Usage: lab_v09.py [--build] [--run SECONDS] [--fast FRAMES]"""
from pathlib import Path
import argparse
import configparser
import os
import shlex
import shutil
import subprocess

ROOT = __import__('sgpaths').ROOT
REPO = ROOT / 'outputs/redalert2-recomp'
LAB = ROOT / 'work/singularity/lab-v10'
SOURCE = REPO / 'mods/yr/Singularity'


def read(path):
    c = configparser.ConfigParser(interpolation=None, strict=False, delimiters=('=',))
    c.optionxform = str
    c.read(path, encoding='cp1252')
    return c


def write(c, path):
    with path.open('w', encoding='cp1252') as f:
        c.write(f, space_around_delimiters=False)


def prepare():
    mod = LAB / 'mods/yr/Singularity'
    if mod.exists():
        shutil.rmtree(mod)
    mod.mkdir(parents=True)
    private = {'ra2md.ini', 'aimd.ini', 'xmp29u2.map'}
    for src in SOURCE.iterdir():
        if src.is_file() and src.name.lower() not in private:
            (mod / src.name).symlink_to(src.resolve())
    (mod / 'aimd.ini').write_text('[TaskForces]\n\n[ScriptTypes]\n\n[TeamTypes]\n\n[AITriggerTypes]\n')
    scene = read(ROOT / 'work/singularity/maps/xmp29u2.map')
    for name in ['Triggers', 'Events', 'Actions', 'Tags', 'CellTags', 'TaskForces', 'ScriptTypes', 'TeamTypes',
                 'AITriggerTypes', 'AITriggerTypesEnable', 'Terrain', 'Smudge', 'Digest', 'OverlayPack', 'OverlayDataPack',
                 'Structures', 'Units', 'Infantry', 'Aircraft']:
        scene.remove_section(name)
    scene['Basic']['Name'] = 'Singularity v10 lab'
    scene['Basic']['FreeRadar'] = 'yes'
    scene['Waypoints']['0'] = '74055'
    scene['Waypoints']['1'] = '32080'
    write(scene, mod / 'xmp29u2.map')
    config = read(SOURCE / 'RA2MD.INI')
    config['Intro']['Play'] = 'no'
    config['Video'].update(ScreenWidth='1280', ScreenHeight='720')
    config['MultiPlayer'].update(Side='British', SideEx='-1', Color='2', ColorEx='-1', GameMode='1')
    config['Options']['GameSpeed'] = '2'
    config['Skirmish'].update(GameMode='1', ScenIndex='0', GameSpeed='2', Credits='10000', UnitCount='0',
                              ShortGame='no', SuperWeaponsAllowed='yes', BuildOffAlly='no',
                              MCVRepacks='yes', CratesAppear='no', Slot01='6,7,-2')
    for n in range(2, 8):
        config['Skirmish']['Slot%02d' % n] = '1,-2,-2'
    write(config, mod / 'RA2MD.INI')


def build():
    work = LAB / 'src'
    work.mkdir(parents=True, exist_ok=True)
    header = (REPO / 'src/linux/singularity.h').read_text()
    header = header.replace('void sg_tick(void) {', '#include "' + str(ROOT / 'work/singularity/lab_v10.h') + '"\nvoid sg_tick(void) {')
    header = header.replace('    sg_lastframe=f;', '    sg_lastframe=f;\n    sg_lab_tick(f);')
    (work / 'singularity_lab.h').write_text(header)
    (work / 'main-lab.c').write_text((REPO / 'src/linux/main.c').read_text().replace('#include "singularity.h"', '#include "singularity_lab.h"'))
    cmds = [__import__('sgpaths').fix_cmd(c) for c in (ROOT / 'work/singularity/build-commands.txt').read_text().splitlines()]
    cmd = shlex.split(next(c for c in cmds if ' -c ' in c and '/src/linux/main.c' in c))
    obj = str(work / 'main-lab.o')
    cmd[cmd.index('-o') + 1] = obj
    cmd[cmd.index('-MF') + 1] = obj + '.d'
    cmd[cmd.index('-c') + 1] = str(work / 'main-lab.c')
    subprocess.run(cmd, cwd=REPO / 'build-macos', check=True)
    link = shlex.split(cmds[-1].removeprefix(': && ').removesuffix(' && :'))
    link = [obj if a == 'CMakeFiles/ra2.dir/src/linux/main.c.o' else a for a in link]
    (LAB / 'build').mkdir(exist_ok=True)
    link[link.index('-o') + 1] = str(LAB / 'build/ra2-lab')
    subprocess.run(link, cwd=REPO / 'build-macos', check=True)


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--build', action='store_true')
    p.add_argument('--run', type=int, default=0)
    p.add_argument('--fast', type=int, default=300)
    p.add_argument('--keep', action='store_true', help='reuse the prepared mod copy')
    p.add_argument('--window', action='store_true', help='visible window instead of headless')
    a = p.parse_args()
    if not a.keep:
        prepare()
    if a.build:
        build()
    if a.run:
        frames = LAB / 'frames'
        if frames.exists():
            shutil.rmtree(frames)
        frames.mkdir()
        env = dict(os.environ, SG_FAST_RESEARCH=str(a.fast))
        cmd = [str(LAB / 'build/ra2-lab'), '--run', '--game', str(ROOT / 'outputs/Red-Alert-2-game-files'), '--mod', 'Singularity',
               '--press', '0xE2:1667@1', '--press', '0x100:1401@2', '--press', '0x102:1559@3', '--waitlog', 'Capture_Mouse@4',
               '--mute', '--watchdog', str(a.run), '--dump-frames', str(frames)] + ([] if a.window else ['--headless'])
        with (LAB / 'lab.log').open('wb') as log:
            subprocess.run(cmd, cwd=LAB, stdout=log, stderr=subprocess.STDOUT)
        print('Lab run finished; log', LAB / 'lab.log')
