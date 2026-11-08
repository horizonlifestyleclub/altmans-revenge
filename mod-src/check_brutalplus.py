"""Consistency checks for the generated Singularity - Brutal Plus profile.

  .venv/bin/python mod-src/check_brutalplus.py
Checks every trigger -> team -> script/task force link, that every unit and
condition object exists in the profile's rules, the Kirov cap, that every side
has pool, scout, raid and defence teams, that the new units are used, and that
art/sound links resolve inside this checkout.
"""
from pathlib import Path
import os

root = __import__('sgpaths').ROOT
repo = root / 'outputs/redalert2-recomp'
mod = repo / 'mods/yr/Singularity - Brutal Plus'
code = (root / 'work/singularity/build_mod.py').read_text()
ns = {}
exec(code[code.index('def readini'):code.index('r=readini')], ns)
readini = ns['readini']
r = readini(mod / 'rulesmd.ini')
ai = readini(mod / 'aimd.ini')

trig = list(ai['AITriggerTypes'].items())
assert trig, 'no triggers'
teams = set(ai['TeamTypes'].values())
scripts = set(ai['ScriptTypes'].values())
tfs = set(ai['TaskForces'].values())
known = {v for k in ['InfantryTypes', 'VehicleTypes', 'AircraftTypes'] for v in r[k].values()}
used = set()
sides = {1: set(), 2: set(), 3: set()}
for tid, line in trig:
    f = line.split(',')
    assert len(f) == 18, (tid, len(f))
    team = f[1]
    assert team in teams and team in ai, (tid, team)
    t = ai[team]
    assert t['Script'] in scripts and t['Script'] in ai, (team, t['Script'])
    assert t['TaskForce'] in tfs and t['TaskForce'] in ai, (team, t['TaskForce'])
    assert f[5] == '<none>' or f[5] in r, (tid, f[5])
    assert f[2] == '<all>' or f[2] in r['Countries'].values(), (tid, f[2])
    for k, v in ai[t['TaskForce']].items():
        if k.isdigit():
            n, u = v.split(',')
            assert u in known, (team, u)
            used.add(u)
            if u == 'ZEP' and team.startswith('BP'):
                assert int(n) <= 2 and t['Max'] == '1', 'Kirov cap'
    if tid.startswith('BPA'):
        assert f[15:] == ['0', '1', '1'], ('Brutal Plus trigger must be off on the Classic slot', tid)
    else:
        assert f[15:] == ['1', '0', '0'], ('Classic trigger must be easy-slot only', tid)
        continue
    kind = team.split('_')[0]
    sides[int(f[12])].add(kind)
for side, kinds in sides.items():
    assert {'BPP', 'BPT', 'BPD'} <= kinds, (side, kinds)
for u in ['SGLINK', 'SGOP', 'SGTANK', 'SGLOCUST', 'SGB2']:
    assert u in used, 'new unit not used: ' + u
for b in ['SGFACT', 'SGJAM', 'SGAEGIS', 'SGCOLO', 'SGNODE1']:
    assert r[b].get('AIBuildThis') == 'yes', b
assert 'ZEP' in used
for p in mod.iterdir():
    if p.is_symlink():
        assert not os.path.isabs(os.readlink(p)), ('absolute link', p.name)
        assert p.resolve().is_relative_to(repo.resolve()), ('link leaves checkout', p.name)
        assert p.exists(), ('broken link', p.name)
hdr = (repo / 'src/linux/adaptive_bp_data.h').read_text()
assert '"SGCOLO"' in hdr and '"SGNODE5"' in hdr
print('Brutal Plus checks passed: %d triggers, %d teams, %d task forces; new units used: %s'
      % (len(trig), len(teams), len(tfs), ', '.join(sorted(u for u in used if u.startswith('SG')))))
