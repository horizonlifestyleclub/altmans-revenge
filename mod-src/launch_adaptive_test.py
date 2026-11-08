"""Prepare an isolated, repeatable skirmish without touching normal profiles."""
from pathlib import Path
import argparse
import configparser
import json
import os
import shutil

ROOT = __import__('sgpaths').ROOT
REPO = ROOT / 'outputs/redalert2-recomp'
TEST = REPO / 'adaptive-playtest'
PROFILE = 'Singularity - Adaptive'


def prepare():
    source = REPO / 'mods/yr' / PROFILE
    mod = TEST / 'mods/yr' / PROFILE
    mod.mkdir(parents=True, exist_ok=True)
    build = TEST / 'build'
    build.mkdir(exist_ok=True)
    binary = REPO / 'build-macos/ra2-singularity'
    target = build / 'ra2-singularity'
    # A real executable copy keeps the engine's mod/settings root isolated.
    if not target.exists() or (target.stat().st_size, target.stat().st_mtime_ns) != (binary.stat().st_size, binary.stat().st_mtime_ns):
        shutil.copy2(binary, target)
    for path in source.iterdir():
        if not path.is_file() or path.name.lower() == 'ra2md.ini':
            continue
        dest = mod / path.name
        if not dest.exists() and not dest.is_symlink():
            dest.symlink_to(path)
    config = configparser.ConfigParser(interpolation=None, strict=False, delimiters=('=',))
    config.optionxform = str
    config.read(source / 'RA2MD.INI', encoding='cp1252')
    config['Intro']['Play'] = 'no'
    config['Video'].update(ScreenWidth='1280', ScreenHeight='720')
    config['MultiPlayer'].update(Side='British', SideEx='-1', Color='2', ColorEx='-1', GameMode='1')
    config['Options']['GameSpeed'] = '2'
    config['Skirmish'].update(GameMode='1', ScenIndex='0', GameSpeed='2', Credits='10000', UnitCount='0',
                              ShortGame='yes', SuperWeaponsAllowed='yes', BuildOffAlly='no',
                              MCVRepacks='yes', CratesAppear='no', Slot01='4,7,-2')
    for n in range(2, 8):
        config['Skirmish']['Slot%02d' % n] = '1,-2,-2'
    with (mod / 'RA2MD.INI').open('w', encoding='cp1252') as handle:
        config.write(handle, space_around_delimiters=False)
    return target


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--smoke-test', action='store_true')
    args = parser.parse_args()
    binary = prepare()
    command = [str(binary), '--run', '--game', str(ROOT / 'outputs/Red-Alert-2-game-files'),
               '--mod', PROFILE, '--press', '0xE2:1667@1', '--press', '0x100:1401@2',
               '--press', '0x102:1559@3', '--waitlog', 'Capture_Mouse@4']
    if args.smoke_test:
        frames = TEST / 'check-frames'
        frames.mkdir(exist_ok=True)
        command += ['--headless', '--mute', '--watchdog', '70', '--dump-frames', str(frames)]
    if args.prepare_only:
        print('Prepared isolated test:', TEST)
        print('You: Allies. Opponent: Adaptive Commander / Soviets. $10,000 each; MCV only; no crates.')
    else:
        print('Starting the separate Adaptive Commander test. Normal game settings are preserved.', flush=True)
        log = TEST / ('smoke-test.log' if args.smoke_test else 'test-game.log')
        with log.open('wb') as handle:
            os.dup2(handle.fileno(), 1)
            os.dup2(handle.fileno(), 2)
        os.chdir(TEST)
        os.execv(str(binary), command)
