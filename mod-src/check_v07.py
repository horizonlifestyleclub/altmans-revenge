from pathlib import Path
import configparser
root=Path.cwd();base=root/'outputs/redalert2-recomp/mods/yr'
def ini(path):
 c=configparser.ConfigParser(interpolation=None,strict=False,delimiters=('=',));c.optionxform=str;c.read(path,encoding='cp1252');return c
r=ini(base/'Singularity/rulesmd.ini')
def permitted(id,country):
 d=r[id];owners=d.get('Owner','').split(',');required=d.get('RequiredHouses','').split(',');forbidden=d.get('ForbiddenHouses','').split(',')
 return country in owners and country not in forbidden and (required==[''] or country in required) and int(d.get('TechLevel','1'))>=0
for u in ['SNIPE','TNKD','BEAG','GTGCAN','AMRADR','SGCOLO','SGLINK','SGTANK']:
 assert permitted(u,'British'),('Allies missing',u)
 assert not permitted(u,'Confederation'),('cross-side access',u)
for u in ['DESO','TERROR','DTRUCK','TTNK','SHK','TESLA','SGOP','SGNODE1','SGFACT']:
 assert permitted(u,'Confederation'),('Soviets missing',u)
 assert not permitted(u,'British'),('cross-side access',u)
for u in ['TESLA','SHK','TTNK']:assert not permitted(u,'Russians'),('China Tesla leak',u)
for u in ['GGI','SGOP','SGLINK']:
 assert r[u]['Occupier']=='yes' and r[u]['OccupyWeapon'] in r
for transport in ['LCRF','SAPC','YHVR']:
 for mcv in ['AMCV','SMCV','PCV']:
  assert float(r[transport]['SizeLimit'])>=float(r[mcv]['Size'])
  assert int(r[transport]['Passengers'])>=float(r[mcv]['Size'])
a=ini(base/'Singularity - Adaptive/rulesmd.ini')
assert a['General']['MultiplayerAICM']=='0,0,0' and a['General']['AIVirtualPurifiers']=='0,0,0'
for level in ['Easy','Normal','Difficult']:
 for bonus in ['Groundspeed','Airspeed','BuildTime','Armor','ROF','Cost']:assert a[level][bonus]=='1'
assert all(not v for v in ini(base/'Singularity - Adaptive/aimd.ini').values())
print('PASS: complete coalition rosters, cross-side restrictions, China no-Tesla, garrison weapons, MCV transport capacity, neutral AI economy/difficulty multipliers and empty legacy attack triggers.')
