from pathlib import Path
import configparser,struct
p=Path('outputs/redalert2-recomp/mods/yr/Singularity')
c=configparser.ConfigParser(interpolation=None,strict=False,delimiters=('=',));c.optionxform=str;c.read(p/'rulesmd.ini')
for unit in ['SGSERAPH','SGNEEDLE','SGLOCUST','SGINT','SGNDRONE','SGWDRONE','SGHOUND']:
 assert c[unit]['ImmuneToPsionics']=='yes',unit
 assert c[unit]['ImmuneToPsionicWeapons']=='yes',unit
assert c['SGFACT']['Cost']=='800'
assert c['SGSERAPH']['Cost']=='3000'
assert c['SGCOLO']['FactoryPlant']=='no'
assert all(c[k]['TechLevel']=='-1' for k in ['TESLA','SHK','TTNK'])
for wh in ['SGPREC','SGNESTWH','SGIMPACT','SGDOGWH']:
 assert len(c[wh]['Verses'].split(','))==11
for name in ['sgseraph','sgneedle','sglocust','sginter','sgscout','sgnest']:
 b=(p/(name+'.vxl')).read_bytes(); n=struct.unpack_from('<4I',b,16);assert n[1]==1
 offset=830+n[3];nx,ny,nz,normal=struct.unpack_from('4B',b,offset+88);assert len(b)==offset+92
 data=830+nx*ny*8
 for i in range(nx*ny):
  start=struct.unpack_from('<i',b,830+i*4)[0]
  if start<0:continue
  j=data+start;z=0
  while z<nz:
   skip,count=b[j:j+2];j+=2;z+=skip+count;assert z<=nz
   j+=2*count;assert b[j]==count;j+=1
  assert j<=offset
print('PASS: drone mind-control flags, restored prices, factory cost, Tesla exclusions, damage tables, and six voxel files.')
import wave
sound=configparser.ConfigParser(interpolation=None,strict=False,delimiters=('=',));sound.optionxform=str;sound.read(p/'soundmd.ini')
idx=(p/'audio.idx').read_bytes();_,_,count=struct.unpack_from('<III',idx);bag=(p/'audio.bag').read_bytes();entries={}
for i in range(count):
 name,offset,size,rate,flags,chunk=struct.unpack_from('<16s5I',idx,12+36*i);name=name.split(b'\0')[0].decode();entries[name]=(offset,size,rate,flags)
 assert offset+size<=len(bag)
for unit in ['SGLINK','SGSERAPH','SGHOUND','SGOP','SGNEEDLE','SGLOCUST']:
 for action in ['VoiceSelect','VoiceMove','VoiceAttack']:
  group=c[unit][action];assert group in sound
  for name in sound[group]['Sounds'].split():
   assert name in entries,name
   offset,size,rate,flags=entries[name];assert rate==22050 and flags==4 and size>4000
   with wave.open(str(p/(name+'.wav')),'rb') as w:assert w.getsampwidth()==2 and w.getnchannels()==1
assert c['SGGUARD']['ImmuneToPsionics']=='yes' and c['SGCOLO']['SpawnsNumber']=='6'
assert all(c['SGNODE'+str(i)]['SpawnsNumber']=='1' for i in range(1,6))
print('PASS: six unit voice sets, all packed PCM samples, six core guardians, five distributed guardians, guardian immunity.')
assert c['SGLINK']['Spawns']=='SGSCOUT' and c['SGLINK']['Deployer']=='no'
assert c['SGSCOUT']['Ammo']=='2' and c['SGINT']['Ammo']=='3'
assert c['SGOP']['Secondary']=='SGNLAUNCH'
assert c['SGSCOUT']['ImmuneToPsionics']=='yes'
for name in ['sgseicon','sglnicon','sgopicon','sgneicon','sgloicon','sghdicon','sgcoicon','sgnoicon','sgreicon','sgbaicon','sgfaicon','sgemicon']:
 b=(p/(name+'.shp')).read_bytes();assert len(b)==2912 and struct.unpack_from('<4H',b)==(0,60,48,1)
print('PASS: differentiated pilot roles, drone ammunition, scout immunity, twelve native portraits.')
