# Independent drones replace automatic carrier-style infantry launches.
for key in ['SGLINK','SGOP']:
 for k in ['Spawns','SpawnsNumber','SpawnRegenRate','SpawnReloadRate']:r[key].pop(k,None)
 r[key].update(Primary='SGSIDEARM',Secondary='',ElitePrimary='SGSIDEARM',EliteSecondary='',Deployer='no',DeployFire='no',GuardRange='4',Trainable='yes')
labels['Name:SGLINK']='LINK Drone Controller'
labels['Name:SGOP']='NEST Drone Controller'
r['SGSERAPH'].update(TechLevel='-1',AllowedToStartInMultiplayer='no')
add('SGDRONEGUN','20mm',None,None,Damage=8,ROF=35,Range=5,Projectile='Invisible3',Warhead='SA',Report='BlackOpsAttack',Anim='VTMUZZLE',OmniFire='yes')
add('SGDRONEBOOM','SGNEEDLEHIT',None,None,Damage=420,Range=1.2,ROF=60,Suicide='no',Warhead='SGDRONEWH',Report='GenLargeExplosion')
add('SGDRONEWH','SGIMPACT',None,None,CellSpread=1.5,PercentAtMax='.3',AffectsAllies='no',Verses='100%,100%,100%,100%,100%,100%,25%,25%,25%,100%,100%',AnimList='S_BANG34')
add('SGPDRONE','SGLOCUST','VehicleTypes','Pilot Drone - D toggles suicide mode',Image='sgscout',Primary='SGDRONEGUN',ElitePrimary='SGDRONEGUN',Secondary='SGDRONEBOOM',EliteSecondary='SGDRONEBOOM',Cost=150,Strength=65,TechLevel=-1,Owner=ALL,RequiredHouses='',Trainable='yes',Speed=16,JumpjetSpeed=22,JumpjetHeight=300,Sight=9,GuardRange=5,Selectable='yes',IsSelectableCombatant='yes',AllowedToStartInMultiplayer='no',Crewed='no',ImmuneToPsionics='yes',ImmuneToPsionicWeapons='yes',VeteranAbilities='',EliteAbilities='')
add('SGTANK','BFRT','VehicleTypes','BASTION Drone Nest - D deploys five drones',Image='sgtank',Cost=2400,Strength=900,Speed=5,Owner=US,RequiredHouses=US,Prerequisite='GAWEAP,GATECH',Passengers=0,OpenTopped='no',PipScale='None',Primary='SGSIDEARM',Secondary='',ElitePrimary='SGSIDEARM',EliteSecondary='',OmniCrusher='no',Crusher='yes',Trainable='yes',AllowedToStartInMultiplayer='no')
a['sgtank']=dict(a['BFRT']);a['sgtank'].update(Cameo='sgtankico',AltCameo='sgtankico')
labels['Name:SGCOLO']='COLOSSUS Compute Campus'
a['GASGC'].update(Foundation='4x4',Height='5')
# Use the owned game's more theatrical performed dialogue, never robotic TTS.
for key,base in [('SGLINK','TANY'),('SGOP','BORIS'),('SGTANK','BFRT'),('SGPDRONE','GHOST')]:
 for field in ['VoiceSelect','VoiceMove','VoiceAttack','VoiceFeedback','VoiceSpecialAttack']:
  r[key][field]=r[base].get(field,r[base].get('VoiceAttack',''))
