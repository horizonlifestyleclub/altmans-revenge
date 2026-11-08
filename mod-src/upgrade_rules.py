from pathlib import Path
p=Path('work/singularity/build_mod.py');s=p.read_text()
pos=s.index('def iniwrite(')
s=s[:pos]+'''# v03: defensive data-centre swarm. The native fire gate enables it only when ACTIVE.
add('SGGUARD','SGINT','AircraftTypes','Colossus Guardian',Strength=60,GuardRange=8,Speed=20,ImmuneToPsionics='yes',ImmuneToPsionicWeapons='yes')
add('SGCORELAUNCH','HornetLauncher',None,None,Range=8,MinimumRange=0,ROF=30)
r['SGCOLO'].update(Primary='SGCORELAUNCH',ElitePrimary='SGCORELAUNCH',Spawns='SGGUARD',SpawnsNumber='6',SpawnRegenRate='120',SpawnReloadRate='30',GuardRange='8',CanPassiveAquire='yes',Powered='no',ThreatPosed='40')
# Custom speech is packed into audio.bag/idx, with original samples retained.
sounds=readini(SRC/'patch/soundmd.ini')
manifest=json.loads((SRC/'voices/manifest.json').read_text())
for group,samples in manifest.items():
    registry=sounds['SoundList'];registry[str(max(int(k) for k in registry if k.isdigit())+1)]=group
    sounds[group]={'Sounds':' '.join(samples),'Control':'random','Volume':'90','Priority':'critical','Type':'global'}
for unit,prefix in [('SGLINK','SGLink'),('SGSERAPH','SGSeraph'),('SGHOUND','SGHound'),('SGOP','SGChina'),('SGNEEDLE','SGChina'),('SGLOCUST','SGChina')]:
    r[unit].update(VoiceSelect=prefix+'Select',VoiceMove=prefix+'Move',VoiceAttack=prefix+'Attack',VoiceFeedback='',VoiceSpecialAttack=prefix+'Attack')
r['SGNEEDLE']['VoiceAttack']='SGNeedleAttack'
''' + s[pos:]
s=s.replace("iniwrite(MOD/'rulesmd.ini',r);iniwrite(MOD/'artmd.ini',a)","iniwrite(MOD/'rulesmd.ini',r);iniwrite(MOD/'artmd.ini',a);iniwrite(MOD/'soundmd.ini',sounds)")
p.write_text(s)
