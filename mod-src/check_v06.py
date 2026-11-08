from pathlib import Path
import configparser,struct,hashlib
p=Path('outputs/redalert2-recomp/mods/yr/Singularity')
def ini(name):
 c=configparser.ConfigParser(interpolation=None,strict=False,delimiters=('=',));c.optionxform=str;c.read(p/name,encoding='cp1252');return c
r=ini('rulesmd.ini');a=ini('artmd.ini');s=ini('soundmd.ini')
assert r['SGSERAPH']['TechLevel']=='-1'
for u in ['SGLINK','SGOP']:assert 'Spawns' not in r[u] and r[u]['Trainable']=='yes'
assert r['SGPDRONE']['TechLevel']=='-1' and r['SGPDRONE']['Selectable']=='yes'
assert r['SGPDRONE']['Secondary']=='SGDRONEBOOM' and r['SGDRONEBOOM']['Suicide']=='no' and r['SGDRONEWH']['AffectsAllies']=='no'
assert r['SGDRONEBOOM']['Damage']=='420' and r['SGDRONEGUN']['Damage']=='8'
assert r['SGDRONEWH']['CellSpread']=='1.5'
assert r['SGTANK']['Passengers']=='0' and r['SGTANK']['Image']=='sgtank'
assert a['GASGC']['Foundation']=='4x4' and 'SGCOREAN' in r['Animations'].values()
assert r['SGPDRONE']['ImmuneToPsionics']=='yes'
idx=(p/'audio.idx').read_bytes();count=struct.unpack_from('<I',idx,8)[0]
entries={struct.unpack_from('<16s',idx,12+36*i)[0].split(b'\0')[0].decode().lower() for i in range(count)}
for u in ['SGLINK','SGOP','SGTANK','SGPDRONE']:
 for action in ['VoiceSelect','VoiceMove','VoiceAttack']:
  group=r[u][action];assert group in s
  for sample in s[group]['Sounds'].split():assert sample.lstrip('$#').lower() in entries,(group,sample)
for name in ['ggsgc','sgcorean','sgtankico','sgcoicon','sglnicon','sgopicon']:
 b=(p/(name+'.shp')).read_bytes();_,w,h,n=struct.unpack_from('<4H',b)
 for i in range(n):
  x,y,fw,fh,comp,avg,zero,off=struct.unpack_from('<4H4I',b,8+24*i)
  assert comp==1 and x+fw<=w and y+fh<=h and off+fw*fh<=len(b)
assert hashlib.sha256((p/'sgtank.vxl').read_bytes()).digest()==hashlib.sha256(Path('work/singularity/local/bfrt.vxl').read_bytes()).digest()
print('PASS: Seraph removal, independent drone unit/weapon settings, 420-damage suicide, tank prerequisites, immunity, performed voice references, native sprites and Colossus foundation/animation.')
