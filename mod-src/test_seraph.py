from pathlib import Path
import configparser, subprocess
root=Path.cwd(); repo=root/'outputs/redalert2-recomp';mod=repo/'mods/yr/Singularity';p=mod/'rulesmd.ini';original=p.read_bytes()
c=configparser.ConfigParser(interpolation=None,strict=False,delimiters=("=",));c.optionxform=str;c.read(p)
for group in ['VehicleTypes','InfantryTypes']:
 for unit in c[group].values():
  if unit in c:c[unit]['AllowedToStartInMultiplayer']='no'
for unit in ['SGSERAPH','SGHOUND']:
 c[unit].update(AllowedToStartInMultiplayer='yes',TechLevel='1',Owner='Americans,Russians',RequiredHouses='',ForbiddenHouses='',Cost='300')
frames=root/'work/singularity/seraph-frames';frames.mkdir(exist_ok=True)
try:
 with p.open('w') as f:c.write(f,space_around_delimiters=False)
 with (root/'work/singularity/seraph.log').open('w') as log:
  result=subprocess.run(['./build-macos/ra2','--run','--mod','Singularity','--headless','--mute','--watchdog','45','--dump-frames',str(frames),'--press','0xE2:1667@1','--press','0x100:1401@2','--press','0x102:1559@3','--waitlog','Capture_Mouse@4'],cwd=repo,stdout=log,stderr=log)
 print('Test exit',result.returncode)
finally:p.write_bytes(original)
