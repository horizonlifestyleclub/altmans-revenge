"""v09 rules/asset checks for both Singularity profiles."""
from pathlib import Path
import struct
ROOT = __import__('sgpaths').ROOT
code = (ROOT / 'work/singularity/build_mod.py').read_text(); ns = {}
exec(code[code.index('def readini'):code.index('r=readini')], ns); readini = ns['readini']
for profile in ['Singularity', 'Singularity - Adaptive']:
    mod = ROOT / 'outputs/redalert2-recomp/mods/yr' / profile
    r = readini(mod / 'rulesmd.ini'); a = readini(mod / 'artmd.ini')
    # Operators: runtime-owned, non-selectable drones; no engine spawners on carriers.
    for unit, rng in [('SGLINK', 10), ('SGOP', 8), ('SGTANK', 10)]:
        assert 'Spawns' not in r[unit] and float(r[r[unit]['Primary']]['Range']) == rng, unit
    assert r['SGPDRONE']['Selectable'] == 'yes' and r['SGDRONEBOOM'].get('Suicide', 'no') == 'no'
    assert r['SGTANK']['Passengers'] == '2' and r['SGTANK']['OpenTopped'] == 'yes'
    # No building spawners (engine spawn-manager crash).
    for key, d in r.items():
        if 'Spawns' in d and key in r['BuildingTypes'].values():
            raise AssertionError('building spawner ' + key)
    # B-2: regular Allied bomber, one bomb.
    b2 = r['SGB2']
    assert 'French' in b2['Owner'] and b2['Prerequisite'] == 'RADAR,GATECH' and b2['Ammo'] == '1' and b2['BuildLimit'] == '2'
    assert r['SGBOMB']['Warhead'] == 'SGB2BOMBWH' and int(r['SGBOMB']['Damage']) >= 2000
    assert r['SGTEMPEST']['TechLevel'] == '-1'
    area = [k for k, d in r.items() if d.get('Prerequisite') == 'SGAREA' and d.get('TechLevel') != '-1']
    assert sorted(area) == ['SGAURORA'], area
    # Singularity cards.
    for key, minutes in [('SGRESEARCH', '6'), ('SGSINGULAR', '2')]:
        assert key in r['SuperWeaponTypes'].values() and r[key]['ShowTimer'] == 'yes' and r[key]['RechargeTime'] == minutes
        shp = (mod / (r[key]['SidebarImage'] + '.shp')).read_bytes()
        assert struct.unpack_from('<4H', shp) == (0, 60, 48, 1), key
print('PASS: operator/Bastion runtime drones (non-selectable), no building spawners, B-2 rules, two Area 51 aircraft, Singularity cards and pictures (both profiles).')
