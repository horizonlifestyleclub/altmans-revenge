"""v10 rules checks: drones, LINK voice, infantry anti-drone fire, prices."""
from pathlib import Path
import struct
ROOT = __import__('sgpaths').ROOT
code = (ROOT / 'work/singularity/build_mod.py').read_text(); ns = {}
exec(code[code.index('def readini'):code.index('r=readini')], ns); readini = ns['readini']
mod = ROOT / 'outputs/redalert2-recomp/mods/yr/Singularity'
r = readini(mod / 'rulesmd.ini'); s = readini(mod / 'soundmd.ini')
d = r['SGPDRONE']
assert d['Selectable'] == 'yes' and d['VoiceSelect'] == 'SGDroneSelect' and 'Seal' not in ''.join(d.get(k, '') for k in d if k.startswith('Voice'))
gun = r['SGDRONEGUNWH']['Verses'].split(',')
assert r['SGDRONEGUN']['Warhead'] == 'SGDRONEGUNWH' and gun[5] == '0%' and gun[8] == '0%', gun
boom = r['SGDRONEWH']['Verses'].split(',')
assert boom[5] == '100%' and boom[8] == '15%' and float(r['SGDRONEWH']['CellSpread']) <= .5
# LINK speaks only with its own radio lines (no Tanya, no Engineer clips).
link = r['SGLINK']
assert link['VoiceSelect'] == 'SGLinkV10Select' and not any(x in ''.join(v for k, v in link.items() if k.startswith('Voice')) for x in ('Tanya', 'EngAll'))
for g in ['SGLinkV10Select', 'SGLinkV10Move', 'SGLinkV10Attack']:
    assert all(n.startswith('sgl') for n in s[g]['Sounds'].split()) and len(s[g]['Sounds'].split()) >= 4, g
idx = (mod / 'audio.idx').read_bytes(); n = struct.unpack_from('<I', idx, 8)[0]
names = {idx[12 + 36 * i:28 + 36 * i].split(b'\0')[0].decode() for i in range(n)}
for group in ['SGLinkRadioSelect', 'SGDroneSelect', 'SGDroneDive']:
    for clip in s[group]['Sounds'].split():
        assert clip in names, clip
# Infantry small arms can hit drones.
assert r['InvisibleLowAA']['AA'] == 'yes'
for w in ['M60', 'M1Carbine', 'MP5', 'AKM']:
    assert r[w]['Projectile'] == 'InvisibleLowAA', w
a = readini(mod / 'artmd.ini')
# Voxels load by art section name: Image must equal the .vxl file name.
for u, img in [('SGPDRONE', 'sgpdrone'), ('SGSWARM', 'sgswarm'), ('SGSWARMC', 'sgswarm'), ('SGTANK', 'sgbastion'), ('SGTANKD', 'sgbastiond')]:
    sec = r[u].get('Image', u)
    assert sec.lower() == img and sec in a and 'Image' not in a[sec] or a[sec].get('Image', '').lower() == img, (u, sec)
    assert (mod / (img + '.vxl')).exists(), img
# Singularity price.
assert r['SGCOLO']['Cost'] == '7500' and all(r['SGNODE%d' % i]['Cost'] == '1500' for i in range(1, 6))
print('PASS v10: visible selectable drones with own voice, gun/suicide warheads, LINK radio voice, infantry AA, $7,500 Singularity.')
