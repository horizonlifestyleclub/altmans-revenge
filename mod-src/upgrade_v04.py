from pathlib import Path
p=Path('work/singularity/build_mod.py');s=p.read_text()
pos=s.index('def iniwrite(')
s=s[:pos]+'''# v04: clear operator roles and useful attack sorties.
add('SGSCOUT','SGINT','AircraftTypes','LINK precision drone',Image='sgscout',Ammo=2,ROT=12,Speed=16,Primary='SGSCOUTGUN',ElitePrimary='SGSCOUTGUN')
add('SGSCOUTGUN','MissileLauncher',None,None,Damage=32,ROF=8,Range=4,MinimumRange=0,Warhead='SGPREC',OmniFire='yes',Anim='VTMUZZLE')
add('SGLINKLAUNCH','HornetLauncher',None,None,Range=7,MinimumRange=0,ROF=15)
add('SGSIDEARM','M60',None,None,Damage=5,ROF=25,Range=3)
r['SGPULSE'].update(Damage='22',ROF='8',Range='4',Projectile='AAHeatSeeker2',Speed='30',OmniFire='yes',Report='GuardianGIDeployedAttack')
r['SGRADIO'].update(Damage='14',ROF='8',Range='4',MinimumRange='0',Projectile='Invisible3',OmniFire='yes',IsLaser='yes',LaserDuration='1',LaserInnerColor='255,190,90',LaserOuterColor='90,45,10',LaserOuterSpread='0,0,0')
r['SGNESTWH']['Verses']='100%,100%,100%,75%,75%,75%,15%,15%,15%,100%,100%'
for key in ['SGLOCUSTGUN','SGNEEDLEHIT']:r[key]['OmniFire']='yes'
r['SGLOCUSTGUN'].update(Projectile='Invisible3',IsLaser='yes',LaserDuration='1',LaserInnerColor='255,190,90',LaserOuterColor='90,45,10',LaserOuterSpread='0,0,0')
r['SGINT'].update(Ammo='3',ROT='12',Speed='16',GuardRange='10')
r['SGNDRONE'].update(Image='sgnest',Ammo='2',ROT='12',Speed='14',GuardRange='8')
r['SGWDRONE'].update(Image='sgnest',Ammo='1',ROT='12',Speed='14')
r['SGLAUNCH'].update(Range='8',MinimumRange='0',ROF='15',Projectile='Invisible3')
r['SGNLAUNCH'].update(Range='7',MinimumRange='1',ROF='15')
r['SGLINK'].update(Primary='SGLINKLAUNCH',Secondary='SGLINKLAUNCH',ElitePrimary='SGLINKLAUNCH',EliteSecondary='SGLINKLAUNCH',Spawns='SGSCOUT',GuardRange='7',Deployer='no',DeployFire='no',SpawnReloadRate='45')
r['SGOP'].update(Primary='SGSIDEARM',ElitePrimary='SGSIDEARM',Secondary='SGNLAUNCH',EliteSecondary='SGNLAUNCH',GuardRange='7',SpawnReloadRate='45')
labels['Name:SGLINK']='LINK - Precision Drone Pilot'
labels['Name:SGOP']='NEST Operator - Deploy to Launch'
for unit,prefix in [('SGNEEDLE','SGNeedle'),('SGLOCUST','SGLocust')]:
    r[unit].update(VoiceSelect=prefix+'Select',VoiceMove=prefix+'Move',VoiceAttack=prefix+'Attack',VoiceSpecialAttack=prefix+'Attack')
# Separate art aliases keep ordinary GIs/buildings and their cameos unchanged.
import shutil
for unit,base,alias,source,icon in [
 ('SGLINK','GGI','sglink','ggi','sglnicon'),('SGOP','GGI','sgop','ggi','sgopicon'),('SGHOUND','ADOG','sghound','adog','sghdicon'),
 ('SGCOLO','GATECH','GASGC','ggtech','sgcoicon'),('SGBATT','GAPOWR','GASGB','ggpowr','sgbaicon'),('SGFACT','NAWEAP','NASGF','ngweap','sgfaicon'),('SGAEGIS','GAPILL','GASGE','ggpill','sgemicon'),('SGJAM','NABNKR','NASGJ','ngbnkr','sgemicon')]:
    a[alias]=dict(a[base]);a[alias].update(Cameo=icon,AltCameo=icon);r[unit]['Image']=alias
    target=(alias[0]+'G'+alias[2:]).lower() if alias.startswith(('GA','NA')) else alias
    shutil.copy2(SRC/'sprites'/(source+'.shp'),MOD/(target+'.shp'))
a['NASGN']=dict(a['NATECH']);a['NASGN'].update(Cameo='sgnoicon',AltCameo='sgnoicon')
shutil.copy2(SRC/'sprites/ngtech.shp',MOD/'ngsgn.shp')
for i in range(1,6):r['SGNODE'+str(i)]['Image']='NASGN'
a['NAPOWR'].update(Cameo='sgreicon',AltCameo='sgreicon')
for name,icon in [('sgseraph','sgseicon'),('sgneedle','sgneicon'),('sglocust','sgloicon'),('sginter','sgseicon'),('sgscout','sglnicon'),('sgnest','sgopicon')]:
    a.setdefault(name,{'Voxel':'yes','Remapable':'yes','PrimaryFireFLH':'0,0,0'}).update(Cameo=icon,AltCameo=icon)
''' +s[pos:]
# Delegate native geometry to the new shape generator, retaining the old implementation as history.
s+="\nexec((SRC/'models_v04.py').read_text())\n"
p.write_text(s)
p=Path('outputs/redalert2-recomp/src/linux/singularity.h');s=p.read_text().replace('!strcmp(id,"SGSERAPH")||','!strcmp(id,"SGSCOUT")||!strcmp(id,"SGSERAPH")||');p.write_text(s)
