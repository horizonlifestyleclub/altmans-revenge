# v10: controllable operator drones, LINK with the Engineer's voice, infantry can
# shoot drones down, Bastion with five tethered drones, $7,500 Singularity.
# Runtime counterparts live in src/linux/singularity.h (search "v10").

# ---- Operator drones (SGPDRONE): selectable, own voice, gun + suicide ----
# Gun (left click): light machine gun - kills infantry, scratches light armour,
# nothing against tanks or buildings. Suicide (Ctrl+click, or D to switch the
# drone's stance): dives in, hurts tanks, chips buildings, kills ~2 infantry.
add('SGDRONEGUNWH','SA',None,None,Verses='100%,80%,80%,5%,0%,0%,0%,0%,0%,100%,100%',
    Wall='no',Wood='no',Tiberium='no',AffectsAllies='no')
r['SGDRONEGUN'].update(Damage='20',ROF='25',Range='5',Warhead='SGDRONEGUNWH')
r['SGDRONEWH'].update(CellSpread='.5',PercentAtMax='.25',
    Verses='100%,100%,100%,100%,100%,100%,15%,15%,15%,100%,100%')
r['SGDRONEBOOM'].update(Damage='300',Range='1.2')
r['SGPDRONE'].update(Name='Operator drone',Selectable='yes',IsSelectableCombatant='yes',
    Armor='flak',Strength='70',
    VoiceSelect='SGDroneSelect',VoiceMove='SGDroneMove',VoiceAttack='SGDroneAttack',
    VoiceSpecialAttack='SGDroneDive',VoiceSecondaryWeaponAttack='SGDroneDive',VoiceFeedback='')
labels['Name:SGPDRONE']='Operator drone (D: gun / suicide)'
# Own, larger quadcopter model (drone_v10.py). The engine loads a voxel by the
# art SECTION name (Image=SGPDRONE -> sgpdrone.vxl); an art section with a
# different name plus its own Image= line loads nothing, which made both the
# operator drone and the v09 Drone Swarm invisible.
r['SGPDRONE']['Image']='SGPDRONE'
a['SGPDRONE']={'Voxel':'yes','Remapable':'yes','Cameo':'sglnicon','AltCameo':'sglnicon','PrimaryFireFLH':'0,0,0'}
a.pop('SGSWARMART',None)
r['SGSWARM']['Image']='SGSWARM';r['SGSWARMC']['Image']='SGSWARM'
a['SGSWARM']={'Voxel':'yes','Remapable':'yes','Cameo':'sgswicon','AltCameo':'sgswicon','PrimaryFireFLH':'0,0,0'}

# ---- Infantry can shoot drones (and, weakly, other aircraft) ----
# Small-arms projectiles get an anti-air twin; rifles and pistols use it.
add('InvisibleLowAA','InvisibleLow',None,None,AA='yes')  # projectiles load on first reference
_small_arms={'SA','HollowPoint','HollowPoint2','HollowPoint4','HollowPointNoBuilding','BORISWH'}
_aa_weapons=set()
for _inf in r['InfantryTypes'].values():
    for _f in ['Primary','ElitePrimary','Secondary','EliteSecondary']:
        _w=r.get(_inf,{}).get(_f)
        if _w and _w in r and r[_w].get('Projectile')=='InvisibleLow' and r[_w].get('Warhead') in _small_arms:
            _aa_weapons.add(_w)
for _w in sorted(_aa_weapons):
    r[_w]['Projectile']='InvisibleLowAA'

# ---- LINK operator: its own radio voice (no Engineer clips); long-range link ----
import json as _json, subprocess as _sp, sys as _sys
_vman=ROOT/'outputs/singularity-build/v10/voice/manifest.json'
if not _vman.exists():
    _sp.run([_sys.executable,str(SRC/'voice_v10.py')],check=True)
_v10=_json.loads(_vman.read_text())
for _group,_names in _v10.items():
    if _group not in sounds['SoundList'].values():
        sounds['SoundList'][str(max(int(k) for k in sounds['SoundList'] if k.isdigit())+1)]=_group
    sounds[_group]={'Sounds':' '.join(_names),'Control':'random','Volume':'85','Priority':'critical','Type':'global'}
def _mix(name,radio):
    clips=sounds[radio]['Sounds'].split()
    if name not in sounds['SoundList'].values():
        sounds['SoundList'][str(max(int(k) for k in sounds['SoundList'] if k.isdigit())+1)]=name
    sounds[name]={'Sounds':' '.join(clips),'Control':'random','Volume':'85'}
_mix('SGLinkV10Select','SGLinkRadioSelect')
_mix('SGLinkV10Move','SGLinkRadioMove')
_mix('SGLinkV10Attack','SGLinkRadioAttack')
r['SGLINK'].update(VoiceSelect='SGLinkV10Select',VoiceMove='SGLinkV10Move',VoiceAttack='SGLinkV10Attack',
    VoiceSpecialAttack='SGLinkV10Attack',VoiceFeedback='',VoiceEnter='SGLinkV10Move',
    VoiceCapture='SGLinkV10Attack',DieSound=r['ENGINEER'].get('DieSound',''))
# The operator's own designator reaches further so it can paint targets for the drone.
for _key,_rng in [('SGLINK',10),('SGOP',8),('SGTANK',10)]:
    r['SGDESIGNATE_'+_key]['Range']=str(_rng)
    r[_key]['GuardRange']=str(_rng)
labels['Name:SGLINK']='LINK Drone Operator'

# ---- Bastion: up to five fibre-optic drones (runtime), immune to EMP ----
labels['Name:SGTANK']='BASTION Drone Carrier (D: deploy 10 s, 5 tethered drones)'
labels['Name:SGTANKD']='BASTION Drone Carrier (deployed - D: pack up 10 s)'
r['SGTANKD']['UIName']='Name:SGTANKD'

# ---- Singularity: a game-ending investment ----
r['SGCOLO']['Cost']='7500'
for _i in range(1,6):
    r['SGNODE%d'%_i]['Cost']='1500'
