"""A populated, isolated late-game unit showcase, not a fresh AI skirmish."""
from pathlib import Path
import argparse
import configparser
import json
import os
import shutil

ROOT = __import__('sgpaths').ROOT
REPO = ROOT / 'outputs/redalert2-recomp'
SHOW = REPO / 'unit-showcase'
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
    mod = SHOW / 'mods/yr/Singularity'
    mod.mkdir(parents=True, exist_ok=True)
    build = SHOW / 'build'
    build.mkdir(exist_ok=True)
    target = build / 'ra2-showcase'
    private = {'rulesmd.ini', 'ra2md.ini', 'aimd.ini', 'xmp29u2.map'}
    for src in SOURCE.iterdir():
        if not src.is_file() or src.name.lower() in private:
            continue
        dst = mod / src.name
        if not dst.exists() and not dst.is_symlink():
            dst.symlink_to(src)

    rules = read(SOURCE / 'rulesmd.ini')
    # Only this sandbox allows the player to rebuild both factions' new kit.
    custom = ['SGLINK','SGOP','SGHOUND','SGNEEDLE','SGLOCUST','SGTANK','SGCOLO','SGNODE1','SGNODE2',
              'SGNODE3','SGNODE4','SGNODE5','SGBATT','SGFACT','SGAEGIS','SGJAM','SGAREA',
              'SGB2','SGRAVEN','SGAURORA','SGTEMPEST']
    for key in custom:
        for prop in ('Owner', 'RequiredHouses'):
            houses = rules[key].get(prop, '').split(',')
            if houses != [''] and 'British' not in houses:
                rules[key][prop] = ','.join(houses + ['British'])
        rules[key]['ForbiddenHouses'] = ','.join(h for h in rules[key].get('ForbiddenHouses','').split(',') if h != 'British')
    write(rules, mod / 'rulesmd.ini')
    # Static target army: no scripted rushes while the player inspects units.
    (mod / 'aimd.ini').write_text('[TaskForces]\n\n[ScriptTypes]\n\n[TeamTypes]\n\n[AITriggerTypes]\n')

    scene = read(ROOT / 'work/singularity/maps/xmp29u2.map')
    for name in ['Triggers','Events','Actions','Tags','CellTags','TaskForces','ScriptTypes','TeamTypes',
                 'AITriggerTypes','AITriggerTypesEnable','Terrain','Smudge','Digest','OverlayPack','OverlayDataPack']:
        scene.remove_section(name)
    for name in ('Structures','Units','Infantry','Aircraft'):
        scene.remove_section(name)
        scene[name] = {}
    scene['Basic']['Name'] = 'Singularity - All New Units Showcase'
    scene['Basic']['FreeRadar'] = 'yes'
    scene['Waypoints']['0'] = '74055'
    scene['Waypoints']['1'] = '32080'
    inventory = []
    def put(kind, unit, x, y, owner='<Player @ A>', rank=0):
        if kind == 'Structures':
            fields = f'{owner},{unit},256,{x},{y},0,None,1,0,1,0,0,None,None,None,0,0'
        elif kind == 'Infantry':
            fields = f'{owner},{unit},256,{x},{y},0,Guard,0,None,{rank},-1,0,1,1'
        elif kind == 'Aircraft':
            fields = f'{owner},{unit},256,{x},{y},64,Guard,None,0,-1,1,1'
        else:
            fields = f'{owner},{unit},256,{x},{y},0,Guard,None,{rank},-1,0,-1,1,1'
        scene[kind][str(99000+len(inventory))] = fields
        inventory.append(dict(kind=kind, unit=unit, x=x, y=y, owner=owner, rank=rank))
    buildings = [
        ('GACNST',40,78),('NACNST',40,84),('GATECH',46,78),('NATECH',46,84),
        ('GAPILE',52,78),('NAHAND',52,84),('GAWEAP',58,78),('SGFACT',58,84),('NAWEAP',64,84),
        ('AMRADR',64,78),('NARADR',68,78),('GAREFN',40,90),('NAREFN',46,90),
        ('NANRCT',34,84),('NANRCT',34,91),('NAPOWR',34,78),('NAPOWR',34,73),
        ('SGCOLO',46,68),('SGAREA',56,68),('SGAEGIS',39,70),('SGJAM',40,74),
        ('SGNODE1',40,62),('SGNODE2',45,62),('SGNODE3',50,62),('SGNODE4',55,62),('SGNODE5',60,62),
        ('SGBATT',65,63),('SGBATT',68,63),('CAOILD',52,90),('CAOILD',56,90),
        ('CAWASH05',61,90), # Empty civilian building for garrison testing.
    ]
    for unit,x,y in buildings:
        put('Structures',unit,x,y,owner='Neutral' if unit=='CAWASH05' else '<Player @ A>')
    for unit,x,y,rank in [('SGLINK',52,73,0),('SGLINK',54,73,1),('SGLINK',56,73,2),
                           ('SGOP',52,75,0),('SGOP',54,75,1),('SGOP',56,75,2),
                           ('SGHOUND',58,73,0),('SGHOUND',58,75,0),('GGI',59,88,0),
                           ('GGI',60,88,0),('ENGINEER',58,88,0),('E1',62,88,0)]:
        put('Infantry',unit,x,y,rank=rank)
    for unit,x,y in [('SGTANK',62,71),('SGTANK',65,71),('SGNEEDLE',66,74),('SGNEEDLE',68,74),
                     ('SGLOCUST',66,76),('SGLOCUST',68,76),('MTNK',70,76),('HTNK',72,76)]:
        put('Units',unit,x,y)
    for unit,x,y in [('SGB2',56,69),('SGRAVEN',58,69),('SGAURORA',56,70),('SGTEMPEST',58,70)]:
        put('Aircraft',unit,x,y)
    for unit,x,y in [('NACNST',75,35),('NANRCT',81,35),('NAREFN',75,41),('NAHAND',81,41),
                     ('NAWEAP',75,47),('NARADR',81,45),('NATECH',85,41),('NAPOWR',86,35)]:
        put('Structures',unit,x,y,'<Player @ B>')
    for unit,x,y in [('HTNK',70,55),('HTNK',73,55),('HTK',76,55)]:
        put('Units',unit,x,y,'<Player @ B>')
    for x,y in [(68,52),(69,52),(70,52),(72,52),(73,52),(74,52)]:
        put('Infantry','E2',x,y,'<Player @ B>')
    write(scene, mod / 'xmp29u2.map')
    power = sum(int(rules[u].get('Power','0')) for u,x,y in buildings if u!='CAWASH05')
    assert power > 1000, power
    (SHOW / 'inventory.json').write_text(json.dumps(dict(power_surplus=power,objects=inventory),indent=2)+'\n')

    config = read(SOURCE / 'RA2MD.INI')
    config['Intro']['Play'] = 'no'
    config['Video'].update(ScreenWidth='1280', ScreenHeight='720')
    config['MultiPlayer'].update(Side='British', SideEx='-1', Color='2', ColorEx='-1', GameMode='1')
    config['Options']['GameSpeed'] = '2'
    config['Skirmish'].update(GameMode='1', ScenIndex='0', GameSpeed='2', Credits='10000', UnitCount='0',
                              ShortGame='no', SuperWeaponsAllowed='yes', BuildOffAlly='no',
                              MCVRepacks='yes', CratesAppear='no', Slot01='6,7,-2')
    for n in range(2,8):
        config['Skirmish']['Slot%02d'%n] = '1,-2,-2'
    write(config, mod / 'RA2MD.INI')
    return target


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--prepare-only', action='store_true')
    parser.add_argument('--smoke-test', action='store_true')
    parser.add_argument('--capture-startup', action='store_true')
    args = parser.parse_args()
    binary = prepare()
    command = [str(binary), '--run', '--game', str(ROOT/'outputs/Red-Alert-2-game-files'), '--mod', 'Singularity',
               '--press','0xE2:1667@1','--press','0x100:1401@2','--press','0x102:1559@3',
               '--waitlog','Capture_Mouse@4']
    if args.smoke_test:
        frames = SHOW/'check-frames'
        frames.mkdir(exist_ok=True)
        command += ['--headless','--mute','--watchdog','85','--dump-frames',str(frames)]
    elif args.capture_startup:
        frames = SHOW/'live-frames'
        frames.mkdir(exist_ok=True)
        command += ['--dump-frames',str(frames)]
    if args.prepare_only:
        print('Prepared populated showcase:',SHOW)
    else:
        print('Opening the populated unit showcase. Your normal game profiles are unchanged.',flush=True)
        with (SHOW/('smoke-test.log' if args.smoke_test else 'showcase.log')).open('wb') as log:
            os.dup2(log.fileno(),1)
            os.dup2(log.fileno(),2)
        os.chdir(SHOW)
        os.execv(str(binary),command)
