# v09: operators attack with carrier-style drones; Singularity becomes two sidebar
# superweapon cards; B-2 is a regular Allied bomber; Area 51 builds two aircraft.
ALLIES='British,French,Germans,Americans,Alliance'

# Drone operators: only the operator is selectable. The native runtime
# (sg_control_tick) keeps its drones airborne beside it; they attack the
# operator's target, or the nearest enemy in range, then return. (The engine's
# carrier launcher crashed on infantry and land carriers in testing.)
# The operator's own weapon is a short-burst designator so it acquires and
# approaches targets exactly like a GI.
add('SGDESIGNATE','20mm',None,None,Damage=3,ROF=40,Range=7,Projectile='Invisible3',Warhead='SA',Report='',Anim='',OmniFire='yes')
for key,rng in [('SGLINK',7),('SGOP',6),('SGTANK',8)]:
 for k in ['Spawns','SpawnsNumber','SpawnRegenRate','SpawnReloadRate']:r[key].pop(k,None)
 add('SGDESIGNATE_'+key,'SGDESIGNATE',None,None,Range=rng)
 r[key].update(Primary='SGDESIGNATE_'+key,ElitePrimary='SGDESIGNATE_'+key,Secondary='',EliteSecondary='',GuardRange=str(rng),Trainable='yes',Deployer='no',DeployFire='no')
r['SGPDRONE'].update(Name='Operator drone',Selectable='no',IsSelectableCombatant='no')
labels['Name:SGPDRONE']='Operator drone'
r['SGDRONEGUN'].update(Damage='25',ROF='30',Range='5')
labels['Name:SGLINK']='LINK Drone Operator'
labels['Name:SGOP']='NEST Drone Operator'

# Bastion: two strike drones on the move, two passenger seats. (Deployed
# ten-drone nest, antennas and fibre-optic trails arrive with its new model.)
r['SGTANK'].update(Passengers='2',OpenTopped='yes',SizeLimit='1')
labels['Name:SGTANK']='BASTION Drone Carrier'

# B-2: regular Allied heavy bomber from the Airforce Command. One bomb levels
# any building; Construction Yards and War Factories survive badly damaged
# (native damage clamp in singularity.h).
add('SGB2BOMBWH','SGBOMBWH',None,None,CellSpread=1.5,PercentAtMax='.3',
 Verses='100%,100%,100%,100%,100%,100%,100%,100%,100%,100%,100%',AnimList='S_BANG48',InfDeath=3,Wall='yes',Wood='yes')
r['SGBOMB'].update(Damage='3000',Warhead='SGB2BOMBWH',ROF='60',Burst='1')
r['SGB2'].update(Owner=ALLIES,RequiredHouses='',ForbiddenHouses='',Prerequisite='RADAR,GATECH',
 Dock='GAAIRC,AMRADR',Cost='2500',BuildLimit='2',Ammo='1',Strength='900',Cloakable='no',RadarInvisible='yes',TechLevel='10')
labels['Name:SGB2']='B-2 Stealth Bomber'

# Area 51 builds two aircraft only.
r['SGTEMPEST'].update(TechLevel='-1')

# Singularity: two sidebar cards with the nuke-style countdown. Granted and
# advanced by the native runtime; never fired by clicking.
for key,name,minutes,icon in [('SGRESEARCH','Singularity Research','6','sgresico'),('SGSINGULAR','Singularity','2','sgsingico')]:
 r['SuperWeaponTypes'][str(max(int(k) for k in r['SuperWeaponTypes'] if k.isdigit())+1)]=key
 r[key]={'UIName':'Name:'+key,'Name':name,'IsPowered':'true','RechargeTime':minutes,'Type':'PsychicReveal',
  'Action':'PsychicReveal','SidebarImage':icon,'ShowTimer':'yes','DisableableFromShell':'no','AIDefendAgainst':'no','Range':'0'}
 labels['Name:'+key]=name
# Building-launched drone swarms (Colossus, compute nodes, factories) crash the
# engine's spawn manager once they activate; buildings get a gated laser instead.
add('SGCORELASER','20mm',None,None,Damage=30,ROF=20,Range=8,Projectile='Invisible3',Warhead='SGPREC',
 IsLaser='yes',LaserDuration='6',LaserInnerColor='180,255,255',LaserOuterColor='20,90,160',LaserOuterSpread='0,0,0',Report='GuardianGIDeployedAttack',Anim='')
add('SGNODELASER','SGCORELASER',None,None,Damage=15,ROF=30,Range=6)
for key in ['SGCOLO','SGNODE1','SGNODE2','SGNODE3','SGNODE4','SGNODE5','GAWEAP','SGFACT']:
 for k in ['Spawns','SpawnsNumber','SpawnRegenRate','SpawnReloadRate']:r[key].pop(k,None)
 weapon='SGCORELASER' if key=='SGCOLO' else 'SGNODELASER' if key.startswith('SGNODE') else ''
 r[key].update(Primary=weapon,ElitePrimary=weapon)

# v09 robot dog (GHOSTHOUND): new sprite (robodog_v09.py) and synthesized robot
# sounds (sfx_v09.py), no borrowed voices. Ledge climbing deliberately not added.
import json as _json
_sfx=_json.loads((ROOT/'outputs/singularity-build/v09/sfx/manifest.json').read_text())
for group,names in _sfx.items():
 if group not in sounds['SoundList'].values():
  sounds['SoundList'][str(max(int(k) for k in sounds['SoundList'] if k.isdigit())+1)]=group
 sounds[group]={'Sounds':' '.join(names),'Control':'random','Volume':'85','Priority':'critical','Type':'global' if group!='SGDogDie' else 'normal'}
r['SGHOUND'].update(VoiceSelect='SGDogSelect',VoiceMove='SGDogMove',VoiceAttack='SGDogAttack',VoiceSpecialAttack='SGDogAttack',VoiceFeedback='',DieSound='SGDogDie')
labels['Name:SGHOUND']='GHOSTHOUND Robot Dog'

# ---- Phase 3 ----
# BASTION: original drone-carrier model; deploys in place (game's own simple
# deployer, as the Siege Chopper) into a mast-up form that keeps ten drones
# airborne (runtime sg_capacity). Crew speaks Ukrainian over a field radio.
r['SGTANK'].update(Image='SGBASTION',IsSimpleDeployer='yes',UnloadingClass='SGTANKD',Deployer='no',DeployFire='no',
 VoiceSelect='SGBastionSelect',VoiceMove='SGBastionMove',VoiceAttack='SGBastionAttack',VoiceSpecialAttack='SGBastionAttack',VoiceFeedback='')
add('SGTANKD','SGTANK','VehicleTypes',None,Image='SGBASTIOND',TechLevel=-1,AllowedToStartInMultiplayer='no',AIBuildThis='no')
r['SGTANKD'].pop('UnloadingClass',None);r['SGTANKD'].pop('IsSimpleDeployer',None)
for key,img in [('SGBASTION','sgbastion'),('SGBASTIOND','sgbastiond')]:
 a[key]={'Image':img,'Voxel':'yes','Remapable':'yes','Cameo':'sgtankico','AltCameo':'sgtankico','PrimaryFireFLH':'0,0,180'}
labels['Name:SGTANK']='BASTION Drone Carrier'
_voice=_json.loads((ROOT/'outputs/singularity-build/v09/voice/manifest.json').read_text())
for group,names in _voice.items():
 if group not in sounds['SoundList'].values():
  sounds['SoundList'][str(max(int(k) for k in sounds['SoundList'] if k.isdigit())+1)]=group
 sounds[group]={'Sounds':' '.join(names),'Control':'random','Volume':'90','Priority':'critical','Type':'global'}

# AREA 51: grey facility with its own antenna mast and sky dish (area51_v09.py).
for old,new in [('GAAIRC_A','GASGA_A'),('GAAIRC_AD','GASGA_AD'),('GAAIRC_B','GASGA_B'),('GAAIRC_BD','GASGA_BD'),('GAAIRC_C','GASGA_C'),('GAAIRC_CD','GASGA_CD')]:
 a[new]=dict(a[old])
 if 'Image' in a[new]:a[new]['Image']=new[:7]
 if new not in r['Animations'].values():r['Animations'][str(max(int(k) for k in r['Animations'] if k.isdigit())+1)]=new
for k,v in list(a['GASGA'].items()):
 if v.startswith('GAAIRC_'):a['GASGA'][k]=v.replace('GAAIRC_','GASGA_')
a['GASGA']['BibShape']='GASGABB'
a['GASGA_C'].update(LoopEnd='8',Rate='160');a['GASGA_CD'].update(LoopStart='8',LoopEnd='16',Rate='160')
labels['Name:SGAREA']='AREA 51'
# Area 51's two aircraft: AURORA and the FURY loyal wingman (RAVEN airframe until its own model).
labels['Name:SGRAVEN']='FURY Loyal Wingman'

# ---- Phase 3 tweaks ----
# B-2: cloaked (enemy air defence cannot see or target it) and a large blast
# (the Kirov's biggest explosion). Aircraft-drop bomb projectiles were tried; the
# B-2 would not release them, so the strike stays instant.
r['SGBOMB'].update(Report='KirovAttack')
r['SGB2BOMBWH'].update(AnimList='TWLT070',Rocker='yes')
r['SGB2'].update(Cloakable='yes',CloakingSpeed='3',RadarInvisible='yes')

# ---- Singularity effects & AI (v09, round 4) ----
# Grid-outage countdown: a timer-only card shown while a house's grid is hacked.
r['SuperWeaponTypes'][str(max(int(k) for k in r['SuperWeaponTypes'] if k.isdigit())+1)]='SGOUTAGE'
r['SGOUTAGE']={'UIName':'Name:SGOUTAGE','Name':'Grid Outage','IsPowered':'false','RechargeTime':'2','Type':'PsychicReveal',
 'Action':'PsychicReveal','SidebarImage':'sgsingico','ShowTimer':'yes','DisableableFromShell':'no','AIDefendAgainst':'no','Range':'0'}
labels['Name:SGOUTAGE']='Grid Outage'
# Area 51 strike fighter (FURY / RAVEN airframe): three rockets per sortie.
r['SGRAVEN']['Ammo']='3'
# DRONE SWARM: one selectable unit that is a cloud of small attack drones.
add('SGSWARMGUN','SGDRONEGUN',None,None,Damage=16,ROF=24,Range=5,Burst=4,OmniFire='yes',Report='BlackOpsAttack')
for key,owner,prereq in [('SGSWARM','Americans,British','GAWEAP,SGCOLO'),('SGSWARMC','Russians','NAWEAP,SGNODE5')]:
 add(key,'SGLOCUST','VehicleTypes','Drone Swarm',Image='SGSWARMART',Primary='SGSWARMGUN',ElitePrimary='SGSWARMGUN',Secondary='',EliteSecondary='',
  Cost=1200,Strength=320,Speed=14,JumpjetSpeed=16,Owner=owner,RequiredHouses=owner,ForbiddenHouses='',Prerequisite=prereq,TechLevel=7,
  Sight=8,ImmuneToPsionics='yes',ImmuneToPsionicWeapons='yes',Crewed='no',AIBuildThis='no',AllowedToStartInMultiplayer='no')
a['SGSWARMART']={'Image':'sgswarm','Voxel':'yes','Remapable':'yes','Cameo':'sgswicon','AltCameo':'sgswicon','PrimaryFireFLH':'0,0,0'}
labels['Name:SGSWARM']='Drone Swarm';labels['Name:SGSWARMC']='Drone Swarm'
