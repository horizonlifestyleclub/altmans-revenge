from pathlib import Path
import json
root=__import__('sgpaths').ROOT
code=(root/'work/singularity/build_mod.py').read_text();ns={};exec(code[code.index('def readini'):code.index('r=readini')],ns);readini=ns['readini']
base=root/'outputs/redalert2-recomp/mods/yr/Singularity';out=base.parent/'Singularity - Adaptive';out.mkdir(exist_ok=True)
for src in base.iterdir():
 if not src.is_file() or src.name.lower() in ['rulesmd.ini','ra2md.ini','aimd.ini','readme.txt','ra2md.csf']:continue
 dst=out/src.name
 if not dst.exists():dst.symlink_to(src)
r=readini(base/'rulesmd.ini')
r['General'].update(MultiplayerAICM='0,0,0',AIVirtualPurifiers='0,0,0',AITriggerSuccessWeightDelta='0',AITriggerFailureWeightDelta='0',MinimumAIDefensiveTeams='0,0,0',MaximumAIDefensiveTeams='0,0,0')
for k in ['Easy','Normal','Difficult']:
 r[k].update(Groundspeed='1',Airspeed='1',BuildTime='1',Armor='1',ROF='1',Cost='1',ContentScan='no')
def write(p,data):p.write_text('; Local Adaptive Commander - experimental fair profile\n'+''.join('\n['+k+']\n'+''.join(a+'='+str(b)+'\n' for a,b in v.items()) for k,v in data.items()),encoding='cp1252')
write(out/'rulesmd.ini',r)
# No original task forces or triggers: the native commander chooses units and orders.
(out/'aimd.ini').write_text('[TaskForces]\n\n[ScriptTypes]\n\n[TeamTypes]\n\n[AITriggerTypes]\n',encoding='cp1252')
config=readini(base/'RA2MD.INI');config['Skirmish'].update(Slot01='4,8,-2',GameSpeed='2');write(out/'RA2MD.INI',config)
(out/'README.txt').write_text('Adaptive Commander experimental local AI. No API or language model. Supports Allied/Soviet sides. Normal costs, no AI income multipliers. Vision-filtered strategy and targets; original superweapon automation disabled. See outputs/singularity-build/v07/STATUS.md for verification and limitations.\n')
# Generated observations use exact installed sight values capped at five cells.
items=sorted({id:max(0,min(5,int(r[id].get('Sight','0')))) for kind in ['InfantryTypes','VehicleTypes','AircraftTypes','BuildingTypes'] for id in r[kind].values() if id in r}.items())
text='static const struct {const char*id;int sight;} sg_ai_sights[]={\n'+''.join('{"%s",%d},\n'%kv for kv in items)+'};\nstatic int sg_ai_sight(const char*id){int lo=0,hi=sizeof sg_ai_sights/sizeof sg_ai_sights[0];while(lo<hi){int mid=(lo+hi)/2,c=strcmp(id,sg_ai_sights[mid].id);if(!c)return sg_ai_sights[mid].sight;if(c<0)hi=mid;else lo=mid+1;}return 0;}\n'
(root/'outputs/redalert2-recomp/src/linux/adaptive_sight.h').write_text(text)
launcher=root/'outputs/redalert2-recomp/Adaptive Commander (experimental).command'
launcher.write_text('#!/bin/sh\ncd "$(dirname "$0")" || exit 1\nexec ./build-macos/ra2-singularity --run --mod "Singularity - Adaptive" "$@"\n');launcher.chmod(0o755)
print('Built local Adaptive Commander profile, shared assets, separate launcher, exact sight table.')

# Separate localized opponent names; the normal mod retains its original AI menu.
import struct
b=(base/'ra2md.csf').read_bytes();magic,version,nlabels,nstrings,unused,lang=struct.unpack_from('<4s5I',b);pos=24;entries=[]
labels={'GUI:AIEasy':'Adaptive Rookie','GUI:AINormal':'Adaptive Veteran','GUI:AIHard':'Adaptive Commander'}
for _ in range(nlabels):
 count,n=struct.unpack_from('<II',b,pos+4);pos+=12;label=b[pos:pos+n].decode('ascii');pos+=n;vals=[]
 for j in range(count):
  tag=b[pos:pos+4];ln=struct.unpack_from('<I',b,pos+4)[0];pos+=8;value=bytes(x^255 for x in b[pos:pos+ln*2]).decode('utf-16le');pos+=ln*2;extra=b''
  if tag==b'WRTS':nn=struct.unpack_from('<I',b,pos)[0];extra=b[pos:pos+4+nn];pos+=4+nn
  vals.append((tag,value,extra))
 if label in labels:vals=[(b' RTS',labels[label],b'')]
 entries.append((label,vals))
data=bytearray(struct.pack('<4s5I',magic,version,len(entries),sum(len(v) for _,v in entries),unused,lang))
for label,vals in entries:
 raw=label.encode('ascii');data+=b' LBL'+struct.pack('<II',len(vals),len(raw))+raw
 for tag,value,extra in vals:
  raw=value.encode('utf-16le');data+=tag+struct.pack('<I',len(raw)//2)+bytes(x^255 for x in raw)+extra
p=out/'ra2md.csf'
if p.is_symlink():p.unlink()
p.write_bytes(data)

# Original House::CanBuild relaxes AI prerequisites. Enforce installed rules
# explicitly before asking a normal factory to manufacture anything.
choices='GAPOWR NAPOWR GAREFN NAREFN GAPILE NAHAND GAWEAP NAWEAP SGFACT GAAIRC AMRADR NARADR GATECH NATECH NASAM NAFLAK GAPILL NALASR HARV CMIN HTK FV HTNK MTNK V3 SREF SGLOCUST SGTANK E1 E2 SGLINK SGOP SENGINEER ENGINEER FLAKT GGI'.split()
groups={k[len('Prerequisite'):].upper():v for k,v in r['General'].items() if k.startswith('Prerequisite')}
rows=[]
for id in choices:
 d=r[id];clauses=[]
 for req in d.get('Prerequisite','').split(','):
  req=req.strip()
  if req:clauses.append(groups.get(req.upper(),req))
 rows.append('{'+','.join(json.dumps(v) for v in [id,d.get('Owner',''),d.get('RequiredHouses',''),d.get('ForbiddenHouses','')])+',{'+','.join(json.dumps(v) for v in clauses+[None]*(8-len(clauses))).replace('null','NULL')+'}}')
(root/'outputs/redalert2-recomp/src/linux/adaptive_tech.h').write_text('static const struct {const char*id,*owners,*required,*forbidden;const char*prereqs[8];} sg_ai_tech[]={\n'+',\n'.join(rows)+'\n};\n')
