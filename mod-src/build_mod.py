from pathlib import Path
import re,struct,json,math
ROOT=__import__('sgpaths').ROOT
SRC=ROOT/'work/singularity'
MOD=ROOT/'outputs/redalert2-recomp/mods/yr/Singularity'
MOD.mkdir(parents=True,exist_ok=True)
def readini(p):
    out={}; sec=None
    for line in p.read_text(encoding='cp1252').splitlines():
        line=line.split(';')[0].strip()
        if line.startswith('['):
            sec=line[1:line.index(']')];out.setdefault(sec,{})
        elif sec and '=' in line:
            k,v=line.split('=',1);out[sec][k.strip()]=v.strip()
    return out
r=readini(SRC/'patch/rulesmd.ini');a=readini(SRC/'local/artmd.ini')
labels={}
def add(section,base,kind,name,**kw):
    r[section]=dict(r[base]) if base else {}
    r[section].update({k:str(v) for k,v in kw.items()})
    if kind:
        registry=r[kind];registry[str(max([int(k) for k in registry if k.isdigit()]+[0])+1)]=section
    if name:
        r[section].update(Name=name,UIName='Name:'+section);labels['Name:'+section]=name
    return r[section]
US='Americans';CN='Russians';ALL='British,French,Germans,Americans,Alliance,Russians,Confederation,Africans,Arabs,YuriCountry'
labels['Name:Russians']='China';r['Russians']['Name']='China'
for k in ['SHK','TTNK','TESLA']:
    if k in r:r[k].update(TechLevel='-1',AllowedToStartInMultiplayer='no')
r['NAPOWR'].update(Name='Mini Nuclear Reactor',UIName='Name:SGREACT',Power='200',Strength='550',Explosion='S_BANG34',MaxDebris='3',MinDebris='1')
labels['Name:SGREACT']='Mini Nuclear Reactor'
# Built-in warhead damage tables: none, flak, plate, light, medium, heavy, wood, steel, concrete, special_1, special_2.
for key,verses,spread,anim in [('SGPREC','25%,25%,25%,100%,100%,100%,25%,25%,25%,100%,100%',0,'PIFFPIFF'),('SGNESTWH','100%,100%,100%,125%,125%,125%,25%,25%,25%,100%,100%',0,'S_BANG16'),('SGIMPACT','50%,50%,50%,100%,100%,100%,25%,25%,25%,100%,100%',0.35,'S_BANG24'),('SGDOGWH','100%,100%,100%,100%,100%,100%,25%,25%,25%,100%,100%',1.5,'S_BANG34')]:
    add(key,'AP',None,None,Verses=verses,CellSpread=spread,PercentAtMax='.2',AnimList=anim,InfDeath=3,Wall='no',Wood='no')
for key,damage,rof,rng,wh in [('SGPULSE',12,30,6,'SGPREC'),('SGLOCUSTGUN',10,30,4,'SGPREC'),('SGWIRE',45,75,4,'SGNESTWH'),('SGRADIO',20,45,8,'SGNESTWH'),('SGDOGBOOM',100,90,1.5,'SGDOGWH'),('SGNEEDLEHIT',40,30,1,'SGIMPACT')]:
    add(key,'20mm',None,None,Damage=damage,ROF=rof,Range=rng,Projectile='Invisible',Warhead=wh,Report='RobotTankAttack',Anim='VTMUZZLE',MinimumRange=0)
r['SGWIRE']['MinimumRange']='2';r['SGRADIO']['MinimumRange']='2'
r['SGNEEDLEHIT']['Suicide']='yes';r['SGDOGBOOM']['Suicide']='yes'
for key,n in [('SGLAUNCH',6),('SGNLAUNCH',8),('SGWLAUNCH',4)]:
    add(key,'HornetLauncher',None,None,Range=n,MinimumRange=2,ROF=45)
for key,name,damage in [('SGINT','Seraph Interceptor',12),('SGNDRONE','Nest Drone',20),('SGWDRONE','Wired Drone',45)]:
    add(key,'HORNET','AircraftTypes',name,Image='sginter',Primary={'SGINT':'SGPULSE','SGNDRONE':'SGRADIO','SGWDRONE':'SGWIRE'}[key],ElitePrimary={'SGINT':'SGPULSE','SGNDRONE':'SGRADIO','SGWDRONE':'SGWIRE'}[key],Strength=40 if key=='SGINT' else 15,Speed=20,Owner=ALL,Cost=0,Soylent=0,ImmuneToPsionics='yes',ImmuneToPsionicWeapons='yes',Explosion='S_BANG16',MaxDebris=0,MovementRestrictedTo='',Secondary='',GuardRange=6)
    r[key].pop('MovementRestrictedTo',None)
for key,name,cost,hp,speed,owner,weapon in [('SGSERAPH','SERAPH Mothership',3000,600,8,US,'SGLAUNCH'),('SGNEEDLE','Needle Kamikaze Drone',100,30,18,CN,'SGNEEDLEHIT'),('SGLOCUST','Locust Drone',200,60,12,CN,'SGLOCUSTGUN')]:
    d=add(key,'DISK','VehicleTypes',name,Image={'SGSERAPH':'sgseraph','SGNEEDLE':'sgneedle','SGLOCUST':'sglocust'}[key],Cost=cost,Soylent=0,Strength=hp,Speed=speed,JumpjetSpeed=speed,Owner=owner,RequiredHouses=owner,Prerequisite='GAWEAP,GATECH' if owner==US else 'SGFACT',Primary=weapon,ElitePrimary=weapon,Secondary='',EliteSecondary='',Turret='no',TurretSpins='no',ImmuneToPsionics='yes',ImmuneToPsionicWeapons='yes',Crewed='no',SelfHealing='no',Explosion='S_BANG34' if hp>100 else 'S_BANG16',MaxDebris=2,DeathWeapon='',DeathWeaponDamageModifier=0,Size=3 if hp>100 else 1,VoiceSelect='RobotTankSelect',VoiceMove='RobotTankMove',VoiceAttack='RobotTankAttackCommand',MoveSound='',CreateSound='',JumpjetHeight=550 if hp>100 else 350)
    if key=='SGSERAPH':d.update(Spawns='SGINT',SpawnsNumber='3',SpawnRegenRate='225',SpawnReloadRate='30',CanPassiveAquire='yes')
for key,name,cost,owner in [('SGLINK','LINK Drone Operator',700,US),('SGOP','Drone Nest Operator',600,CN)]:
    add(key,'GGI','InfantryTypes',name,Image='GGI',Cost=cost,Soylent=0,Owner=owner,RequiredHouses=owner,Prerequisite='GAPILE,RADAR' if owner==US else 'NAHAND,RADAR',Primary='SGIdle',Secondary='SGLAUNCH' if owner==US else 'SGNLAUNCH',ElitePrimary='SGIdle',EliteSecondary='SGLAUNCH' if owner==US else 'SGNLAUNCH',Speed=4,Strength=100,Spawns='SGINT' if owner==US else 'SGNDRONE',SpawnsNumber=1 if owner==US else 2,SpawnRegenRate=120 if owner==US else 90,SpawnReloadRate=45,DeployedCrushable='yes',ImmuneToPsionics='no',Deployer='yes',DeployFire='yes')
add('SGIdle','20mm',None,None,Damage=0,Range=0,ROF=900,Projectile='Invisible',Warhead='Special',Report='',Anim='')
add('SGHOUND','ADOG','InfantryTypes','GHOSTHOUND',Image='ADOG',Cost=350,Soylent=0,Strength=100,Speed=10,Sight=9,Owner=US,RequiredHouses=US,ForbiddenHouses='',Primary='SGDOGBOOM',Secondary='VirtualScanner',ElitePrimary='SGDOGBOOM',ImmuneToPsionics='yes',ImmuneToPsionicWeapons='yes',Natural='no',Organic='no',Explosion='S_BANG16',Explodes='no',VoiceSelect='RobotTankSelect',VoiceMove='RobotTankMove',VoiceAttack='RobotTankAttackCommand')
add('SGFACT','NAWEAP','BuildingTypes','Drone Factory',Image='NAWEAP',Cost=800,Strength=800,Power=-50,Prerequisite='NAHAND,NACNST',Owner=CN,RequiredHouses=CN,AIBuildThis='no')
# Separate prerequisite means the cheap drone factory cannot unlock conventional tanks.
add('SGCOLO','GATECH','BuildingTypes','COLOSSUS',Image='GATECH',Cost=5000,Strength=1500,Power=-800,Prerequisite='GATECH,GACNST',Owner=US,RequiredHouses=US,BuildLimit=1,FactoryPlant='no',InfantryCostBonus=1,UnitsCostBonus=1,AircraftCostBonus=1,BuildingsCostBonus=1,DefensesCostBonus=1,AIBuildThis='no')
for i in range(1,6):
    add('SGNODE'+str(i),'NATECH','BuildingTypes','Compute Node '+str(i),Image='NATECH',Cost=1000,Strength=650,Power=-100,Prerequisite='NATECH,NACNST'+(',SGNODE'+str(i-1) if i>1 else ''),Owner=CN,RequiredHouses=CN,BuildLimit=1,AIBuildThis='no')
add('SGBATT','GAPOWR','BuildingTypes','Battery Storage',Image='GAPOWR',Cost=500,Strength=450,Power=0,Prerequisite='NAPOWR,NACNST',Owner=CN,RequiredHouses=CN,AIBuildThis='no')
for key,base,name,cost,owner in [('SGAEGIS','GAPILL','AEGIS EMP Array',1200,US),('SGJAM','NABNKR','Interference Array',500,CN)]:
    add(key,base,'BuildingTypes',name,Image=base,Cost=cost,Strength=500,Power=-150 if owner==US else -75,Prerequisite='GATECH,GACNST' if owner==US else 'NARADR,NACNST',Owner=owner,RequiredHouses=owner,Primary='',ElitePrimary='',Powered='yes',CanBeOccupied='no',AIBuildThis='no')
# Native voxel models and existing game-format infantry/building art are deliberately distinct.
for name,cameo in [('sgseraph','diskicon'),('sgneedle','dronicon'),('sglocust','roboicon'),('sginter','hornicon')]:
    a[name]={'Voxel':'yes','Remapable':'yes','Cameo':cameo,'PrimaryFireFLH':'0,0,0'}
# v03: defensive data-centre swarm. The native fire gate enables it only when ACTIVE.
add('SGGUARD','SGINT','AircraftTypes','Colossus Guardian',Strength=60,GuardRange=8,Speed=20,ImmuneToPsionics='yes',ImmuneToPsionicWeapons='yes')
add('SGCORELAUNCH','HornetLauncher',None,None,Range=8,MinimumRange=0,ROF=30)
r['SGCOLO'].update(Primary='SGCORELAUNCH',ElitePrimary='SGCORELAUNCH',Spawns='SGGUARD',SpawnsNumber='6',SpawnRegenRate='120',SpawnReloadRate='30',GuardRange='8',CanPassiveAquire='yes',Powered='no',ThreatPosed='40')
for i in range(1,6):
    r['SGNODE'+str(i)].update(Primary='SGCORELAUNCH',ElitePrimary='SGCORELAUNCH',Spawns='SGGUARD',SpawnsNumber='1',SpawnRegenRate='120',SpawnReloadRate='30',GuardRange='8',CanPassiveAquire='yes',Powered='no')
# Custom speech is packed into audio.bag/idx, with original samples retained.
sounds=readini(SRC/'patch/soundmd.ini')
manifest=json.loads((SRC/'voices/manifest.json').read_text())
for group,samples in manifest.items():
    registry=sounds['SoundList'];registry[str(max(int(k) for k in registry if k.isdigit())+1)]=group
    sounds[group]={'Sounds':' '.join(samples),'Control':'random','Volume':'90','Priority':'critical','Type':'global'}
for unit,prefix in [('SGLINK','SGLink'),('SGSERAPH','SGSeraph'),('SGHOUND','SGHound'),('SGOP','SGChina'),('SGNEEDLE','SGChina'),('SGLOCUST','SGChina')]:
    r[unit].update(VoiceSelect=prefix+'Select',VoiceMove=prefix+'Move',VoiceAttack=prefix+'Attack',VoiceFeedback='',VoiceSpecialAttack=prefix+'Attack')
r['SGNEEDLE']['VoiceAttack']='SGNeedleAttack'
# v04: clear operator roles and useful attack sorties.
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
exec((SRC/'rules_v05.py').read_text())
exec((SRC/'rules_v06.py').read_text())
exec((SRC/'rules_v07.py').read_text())
exec((SRC/'rules_v09.py').read_text())
exec((SRC/'rules_v10.py').read_text())
def iniwrite(path,data):
    path.write_text('; SINGULARITY local prototype. Generated by build_mod.py\n'+''.join('\n['+s+']\n'+''.join(k+'='+str(v)+'\n' for k,v in d.items()) for s,d in data.items()),encoding='cp1252')
iniwrite(MOD/'rulesmd.ini',r);iniwrite(MOD/'artmd.ini',a);iniwrite(MOD/'soundmd.ini',sounds)
# Preserve every original string and append unique new labels; rename Russia in place.
b=(SRC/'lang/ra2md.csf').read_bytes();magic,version,nlabels,nstrings,unused,lang=struct.unpack_from('<4s5I',b);pos=24;entries=[]
for _ in range(nlabels):
    assert b[pos:pos+4]==b' LBL';count,n=struct.unpack_from('<II',b,pos+4);start=pos;pos+=12;label=b[pos:pos+n].decode('ascii');pos+=n
    vals=[]
    for j in range(count):
        tag=b[pos:pos+4];ln=struct.unpack_from('<I',b,pos+4)[0];pos+=8
        value=bytes(x^255 for x in b[pos:pos+ln*2]).decode('utf-16le');pos+=ln*2
        extra=b''
        if tag==b'WRTS':nn=struct.unpack_from('<I',b,pos)[0];extra=b[pos:pos+4+nn];pos+=4+nn
        vals.append((tag,value,extra))
    if label in labels:vals=[(b' RTS',labels.pop(label),b'')]
    entries.append((label,vals))
entries.extend((k,[(b' RTS',v,b'')]) for k,v in labels.items())
out=bytearray(struct.pack('<4s5I',magic,version,len(entries),sum(len(v) for _,v in entries),unused,lang))
for label,vals in entries:
    raw=label.encode('ascii');out+=b' LBL'+struct.pack('<II',len(vals),len(raw))+raw
    for tag,value,extra in vals:
        raw=value.encode('utf-16le');out+=tag+struct.pack('<I',len(raw)//2)+bytes(x^255 for x in raw)+extra
(MOD/'ra2md.csf').write_bytes(out)
# Build actual VXL/HVA assets; original geometry, static rigid body, full engine rotation.
template=(SRC/'local/robo.vxl').read_bytes();palette=template[32:802]
ident=struct.pack('<12f',1,0,0,0,0,1,0,0,0,0,1,0)
def voxel(name,nx,ny,nz,shape,scale):
    starts=[];ends=[];span=bytearray()
    for y in range(ny):
        for x in range(nx):
            col=[shape((x-nx/2)/(nx/2),(y-ny/2)/(ny/2),z,nz) for z in range(nz)]
            if not any(col):starts.append(-1);ends.append(-1);continue
            starts.append(len(span));z=0
            while z<nz:
                skip=0
                while z<nz and not col[z]:skip+=1;z+=1
                run=[]
                while z<nz and col[z]:run.extend((col[z],16));z+=1
                count=len(run)//2;span+=bytes([skip,count]+run+[count])
            ends.append(len(span)-1)
    n=nx*ny;body=struct.pack('<%di'%n,*starts)+struct.pack('<%di'%n,*ends)+span
    head=b'Voxel Animation\0'+struct.pack('<4I',1,1,1,len(body))+palette+name.encode().ljust(16,b'\0')+struct.pack('<3I',0,1,2)
    footer=struct.pack('<3If',0,n*4,n*8,1/12)+ident+struct.pack('<6f',-nx*scale/2,-ny*scale/2,0,nx*scale/2,ny*scale/2,nz*scale)+bytes([nx,ny,nz,4])
    (MOD/(name+'.vxl')).write_bytes(head+body+footer)
    (MOD/(name+'.hva')).write_bytes(b'\0'*16+struct.pack('<II',1,1)+name.encode().ljust(16,b'\0')+ident)
def seraph(x,y,z,n):
    edge=abs(x)*.7+abs(y)
    if 3<=z<=6 and .4<edge<.95:return 18 if edge<.49 else 46
    if 6<z<10 and y>.3 and (abs(x)<.12 or .42<abs(x)<.58):return 18 if z==9 else 48
    return 0
def needle(x,y,z,n):
    if 2<=z<=4 and abs(x)+abs(y)<.85:return 18 if abs(y)<.14 else 45
    return 0
def locust(x,y,z,n):
    if 2<=z<=5 and abs(x)<.28 and abs(y)<.45:return 18 if z==5 else 48
    if z==3 and abs(abs(x)-abs(y))<.12 and abs(x)<.7:return 45
    if z==4 and any(.13<math.hypot(x-i,y-j)<.27 for i in [-.58,.58] for j in [-.58,.58]):return 46
    return 0
voxel('sgseraph',48,48,12,seraph,1.6);voxel('sgneedle',20,24,6,needle,.8);voxel('sglocust',24,24,8,locust,1);voxel('sginter',18,18,6,needle,.7)
(MOD/'README.txt').write_text('SINGULARITY development build\nNew units and custom voxel drones. Advanced progression and EMP require native runtime extension.\n')
print('Wrote',MOD,'with',len(r),'rules sections and four native voxel models.')

exec((SRC/'models_v04.py').read_text())

exec((SRC/'assets_v05.py').read_text())

exec((SRC/'assets_v06.py').read_text())

# Original v08 cameo portraits replace the old relabelled stock portraits last,
# so rebuilding the mod cannot silently restore those placeholders.
import subprocess, sys
subprocess.run([sys.executable, str(SRC/'build_cameos_v08.py'), '--install', str(MOD)], check=True)
# v09: robot dog sprite and synthesized sounds replace the stock dog copy.
subprocess.run([sys.executable, str(SRC/'robodog_v09.py'), '--install', str(MOD)], check=True)
subprocess.run([sys.executable, str(SRC/'sfx_v09.py'), '--install', str(MOD)], check=True)
for tool in ['bastion_v09.py','area51_v09.py','voice_v09.py','b2_v09.py','swarm_v09.py','voice_v10.py','drone_v10.py']:
    subprocess.run([sys.executable, str(SRC/tool), '--install', str(MOD)], check=True)
