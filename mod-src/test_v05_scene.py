from pathlib import Path
import shutil,configparser,subprocess
root=Path.cwd();lab=root/'work/singularity/lab-v05';mod=lab/'mods/yr/Singularity';mod.mkdir(parents=True,exist_ok=True);(lab/'build').mkdir(exist_ok=True)
source=root/'outputs/redalert2-recomp'
for p in (source/'mods/yr/Singularity').iterdir():
 if p.is_file():shutil.copy2(p,mod/p.name)
shutil.copy2(source/'build-macos/ra2-singularity',lab/'build/ra2')
c=configparser.ConfigParser(interpolation=None,strict=False,delimiters=('=',));c.optionxform=str;c.read(mod/'rulesmd.ini')
c['AMCV']['DeploysInto']='SGCOLO';c['SGCOLO']['Power']='2000'
for group in ['VehicleTypes','InfantryTypes']:
 for key in c[group].values():
  if key in c:c[key]['AllowedToStartInMultiplayer']='no'
for key in ['SGLINK','SGSERAPH']:
 c[key].update(AllowedToStartInMultiplayer='yes',TechLevel='1',Owner='Americans,Russians',RequiredHouses='',ForbiddenHouses='',Cost='300')
with (mod/'rulesmd.ini').open('w') as f:c.write(f,space_around_delimiters=False)
c=configparser.ConfigParser();c.optionxform=str;c.read(mod/'RA2MD.INI');c['Video'].update(ScreenWidth='640',ScreenHeight='480');c['Skirmish']['UnitCount']='2'
with (mod/'RA2MD.INI').open('w') as f:c.write(f,space_around_delimiters=False)
frames=lab/'frames';frames.mkdir(exist_ok=True)
args=[str(lab/'build/ra2'),'--run','--game',str(root/'outputs/Red-Alert-2-game-files'),'--mod','Singularity','--headless','--mute','--watchdog','65','--dump-frames',str(frames),'--press','0xE2:1667@1','--press','0x100:1401@2','--press','0x102:1559@3','--waitlog','Capture_Mouse@4','--drag','0,40,470,450@6','--key','0x44@7','--drag','0,40,470,450@11','--key','0x44@12']
with (lab/'test.log').open('w') as log:subprocess.run(args,stdout=log,stderr=log)
print('Scene test completed')
