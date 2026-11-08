from pathlib import Path
import configparser,struct
p=Path('outputs/redalert2-recomp/mods/yr/Singularity')
def ini(name):
 c=configparser.ConfigParser(interpolation=None,strict=False,delimiters=('=',));c.optionxform=str;c.read(p/name,encoding='cp1252');return c
c=ini('rulesmd.ini');a=ini('artmd.ini');s=ini('soundmd.ini')
assert c['SGLINKLAUNCH']['Range']=='16' and c['SGNLAUNCH']['Range']=='16'
assert c['SGNEEDLEHIT']['Damage']=='320' and c['SGNEEDLEHIT']['Suicide']=='yes'
assert c['SGIMPACT']['CellSpread']=='1.6'
assert c['SGOP']['Spawns']=='SGKAMI' and c['SGKAMI']['Ammo']=='1'
assert c['SGAREA']['NumberOfDocks']=='4' and c['SGAREA']['ForbiddenHouses']==''
for u in ['SGB2','SGRAVEN','SGAURORA','SGTEMPEST']:
 assert u in c['AircraftTypes'].values() and u in c['General']['PadAircraft']
 assert c[u]['Dock']=='SGAREA' and c[u]['Prerequisite']=='SGAREA'
 assert c[u]['ImmuneToPsionics']=='yes'
 assert (p/(c[u]['Image']+'.vxl')).exists()
for u in ['SGSCOUT','SGKAMI','SGFACTDRONE','SGINT','SGNDRONE','SGNEEDLE','SGLOCUST','SGHOUND']:
 assert c[u]['ImmuneToPsionics']=='yes'
for u in ['GAWEAP','SGFACT']:assert c[u]['Spawns']=='SGFACTDRONE' and c[u]['SpawnsNumber']=='4'
idx=(p/'audio.idx').read_bytes();count=struct.unpack_from('<I',idx,8)[0]
entries={struct.unpack_from('<16s',idx,12+36*i)[0].split(b'\0')[0].decode().lower() for i in range(count)}
for unit in ['SGLINK','SGOP','SGSERAPH','SGNEEDLE','SGLOCUST','SGB2']:
 for action in ['VoiceSelect','VoiceMove','VoiceAttack']:
  group=c[unit][action];assert group in s,(unit,group)
  for sample in s[group]['Sounds'].split():assert sample.lstrip('$#').lower() in entries,(group,sample)
for file in p.glob('sg*ico*.shp'):
 b=file.read_bytes();_,w,h,n=struct.unpack_from('<4H',b);assert (w,h,n)==(60,48,1)
 x,y,fw,fh,comp,avg,zero,off=struct.unpack_from('<4H4I',b,8);assert comp==1 and off+fw*fh==len(b),file
for file in [p/'ggsgc.shp',p/'sgcorean.shp',p/'ggsga.shp',p/'sglink.shp',p/'sgop.shp']:
 b=file.read_bytes();_,w,h,n=struct.unpack_from('<4H',b)
 for i in range(n):
  x,y,fw,fh,comp,avg,zero,off=struct.unpack_from('<4H4I',b,8+24*i)
  assert x+fw<=w and y+fh<=h and off+fw*fh<=len(b),file
assert a['sglink']['Sequence']==a['ENGINEER']['Sequence']
print('PASS: range/damage, 4 Area 51 aircraft/docks, immunity, free factory defenders, stock voice samples, native icons, animated core, custom operator frames.')
