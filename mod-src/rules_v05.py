# v05: stock performed dialogue, long-range loitering munitions, Area 51.
for unit,base in [('SGLINK','GHOST'),('SGOP','FLAKT'),('SGSERAPH','ORCA'),('SGHOUND','GHOST'),('SGNEEDLE','ZEP'),('SGLOCUST','FLAKT')]:
 for k in ['VoiceSelect','VoiceMove','VoiceAttack','VoiceFeedback','VoiceSpecialAttack']:
  r[unit][k]=r[base].get(k,r[base].get('VoiceAttack',''))
# Replace a single stock Rhino's 400HP with 320 damage at the epicentre.
r['SGIMPACT'].update(Verses='100%,100%,100%,100%,100%,100%,25%,25%,25%,100%,100%',CellSpread='1.6',PercentAtMax='.35',AnimList='S_BANG34',InfDeath='3')
r['SGNEEDLEHIT'].update(Damage='320',Range='1.2',Suicide='yes',Report='GenSmallExplosion',Anim='',Projectile='InvisibleLow',Warhead='SGIMPACT')
r['SGNEEDLE'].update(Speed='22',JumpjetSpeed='22',Strength='35',Cost='150',Sight='12',GuardRange='16')
add('SGKAMI','SGNDRONE','AircraftTypes','Nest expendable strike drone',Primary='SGNEEDLEHIT',ElitePrimary='SGNEEDLEHIT',Image='sgneedle',Ammo=1,Strength=25,Speed=20,ROT=16,GuardRange=16)
r['SGOP'].update(Spawns='SGKAMI',SpawnsNumber='1',SpawnRegenRate='180',GuardRange='16',Sight='9')
r['SGNLAUNCH'].update(Range='16',MinimumRange='2')
r['SGLINKLAUNCH'].update(Range='16',MinimumRange='0')
r['SGLINK'].update(GuardRange='16',Sight='10')
r['SGSCOUTGUN'].update(Damage='80',Range='5',ROF='12')
r['SGSCOUT'].update(Ammo='2',GuardRange='16',Strength='45')
r['SGLAUNCH'].update(Range='18');r['SGSERAPH'].update(GuardRange='18',Sight='12',SpawnsNumber='4')
labels['Name:SGLINK']='LINK - Long Range Drone Pilot'
labels['Name:SGOP']='NEST - Deploy / Kamikaze Strike'
# Keep the established American dock geometry, four slots, full aircraft production.
add('SGAREA','GAAIRC','BuildingTypes','AREA 51 - Experimental Airfield',Image='GASGA',Prerequisite='GATECH,GACNST',Owner=US,RequiredHouses=US,ForbiddenHouses='',Cost=3000,Power=-250,Strength=1800,BuildLimit=1,AIBuildThis='no',NumberOfDocks=4)
a['GASGA']=dict(a['GAAIRC']);a['GASGA'].update(Cameo='sga51ico',AltCameo='sga51ico')
shutil.copy2(SRC/'sprites/ggairc.shp',MOD/'ggsga.shp')
add('SGBOMBWH','HE',None,None,CellSpread=2,PercentAtMax='.45',Verses='100%,100%,100%,90%,90%,90%,100%,100%,80%,100%,100%',AnimList='S_BANG48',InfDeath=3)
add('SGBOMB','Maverick',None,None,Damage=220,ROF=12,Range=8,Projectile='Invisible',Speed=100,Warhead='SGBOMBWH',Report='BlackEagleAttack',Burst=1)
add('SGAIRAA','Maverick',None,None,Damage=140,ROF=10,Range=12,Projectile='AAHeatSeeker2',Warhead='AP',Burst=1)
add('SGAIRPREC','Maverick',None,None,Damage=400,ROF=20,Range=10,Warhead='AP',Burst=1)
add('SGAIRCLUSTER','SGBOMB',None,None,Damage=100,ROF=6,Range=8,Burst=1)
for key,name,image,weapon,cost,hp,speed,ammo in [
 ('SGB2','B-2 NIGHTFALL Stealth Bomber','sgb2','SGBOMB',4000,1800,22,6),
 ('SGRAVEN','RAVEN Air Dominance','sgraven','SGAIRAA',2200,450,26,6),
 ('SGAURORA','AURORA Precision Strike','sgaurora','SGAIRPREC',2600,500,30,2),
 ('SGTEMPEST','TEMPEST Cluster Bomber','sgtempest','SGAIRCLUSTER',2400,700,20,8)]:
 add(key,'ORCA','AircraftTypes',name,Image=image,Primary=weapon,ElitePrimary=weapon,Prerequisite='SGAREA',Owner=US,RequiredHouses=US,ForbiddenHouses='',Dock='SGAREA',Cost=cost,Strength=hp,Speed=speed,Ammo=ammo,ROT=8,ImmuneToPsionics='yes',ImmuneToPsionicWeapons='yes',GuardRange=20,TechLevel=10)
 a[image]={'Voxel':'yes','Remapable':'yes','Cameo':image+'ico','AltCameo':image+'ico','PrimaryFireFLH':'0,0,0'}
r['SGB2'].update(Cloakable='yes',CloakingSpeed='3',RadarInvisible='yes')
r['General']['PadAircraft']+=',SGB2,SGRAVEN,SGAURORA,SGTEMPEST'
# Free, replenishing factory defence swarms become usable only in ACTIVE state.
add('SGFACTDRONE','SGINT','AircraftTypes','Factory interceptor',Strength=60,Ammo=4,Speed=18)
for fac in ['GAWEAP','SGFACT']:
 r[fac].update(Primary='SGCORELAUNCH',ElitePrimary='SGCORELAUNCH',Spawns='SGFACTDRONE',SpawnsNumber='4',SpawnRegenRate='90',SpawnReloadRate='20',GuardRange='18',CanPassiveAquire='yes')
r['SGCORELAUNCH']['Range']='18'
r['SGCOLO'].update(GuardRange='18',SpawnRegenRate='60',SpawnReloadRate='15',LightVisibility='8',LightIntensity='.2',LightRedTint='.1',LightGreenTint='.7',LightBlueTint='1')
# Own animated core, no inherited tech-centre rooftop animations covering it.
for k in list(a['GASGC']):
 if 'ActiveAnim' in k or 'IdleAnim' in k or 'SuperAnim' in k:a['GASGC'].pop(k)
a['GASGC'].update(ActiveAnim='SGCOREAN',ActiveAnimDamaged='SGCOREAN',ActiveAnimZAdjust='-80')
a['SGCOREAN']={'Image':'sgcorean','Normalized':'yes','LoopStart':'0','LoopEnd':'11','LoopCount':'-1','Rate':'200','Layer':'ground','NewTheater':'no'}

a["sglink"]=dict(a["ENGINEER"]);a["sglink"].update(Cameo="sglnicon",AltCameo="sglnicon")

r["Animations"][str(max(int(k) for k in r["Animations"] if k.isdigit())+1)]="SGCOREAN"
