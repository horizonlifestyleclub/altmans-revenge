# Combined rosters use two existing country slots; native YR has ten menu slots.
for country,name in [('British','Allies'),('Confederation','Soviets')]:
 r[country]['Name']=name;labels['Name:'+country]=name
# Allow each combined country every same-side exclusive, preserving original countries.
allied=set(r['Sides']['GDI'].split(','));soviet=set(r['Sides']['Nod'].split(','))
for key,d in r.items():
 if not any(key in r[k].values() for k in ['InfantryTypes','VehicleTypes','AircraftTypes','BuildingTypes']):continue
 owners=set(d.get('Owner','').split(','))
 for group,country in [(allied,'British'),(soviet,'Confederation')]:
  permitted=(owners&group)-set(d.get('ForbiddenHouses','').split(','))
  if d.get('RequiredHouses'):permitted &= set(d['RequiredHouses'].split(','))
  if not permitted:continue
  if country not in owners:d['Owner']=d.get('Owner','')+','+country
  required=d.get('RequiredHouses','')
  if required and set(required.split(','))&group and country not in required.split(','):d['RequiredHouses']=required+','+country
  if d.get('ForbiddenHouses'):d['ForbiddenHouses']=','.join(x for x in d['ForbiddenHouses'].split(',') if x!=country)
# Tesla exclusives belong to combined Soviets; China remains the no-Tesla faction.
for key in ['SHK','TTNK','TESLA']:
 r[key]['TechLevel']=readini(SRC/'patch/rulesmd.ini')[key]['TechLevel']
 r[key]['RequiredHouses']='Confederation';r[key]['ForbiddenHouses']='Russians'
# Allied coalition receives the US paradrop through its native radar building.
r['GAAIRC']['ForbiddenHouses']='Americans,British'
r['AMRADR']['RequiredHouses']='Americans,British'
# Guardian missile garrison plus weak sidearm for controllers; drones remain independent.
add('SGGARRISONMISSILE','MissileLauncher',None,None,Damage=35,Range=7,ROF=60)
add('SGGARRISONMISSILEE','SGGARRISONMISSILE',None,None,Damage=45,ROF=50)
r['GGI'].update(Occupier='yes',OccupyWeapon='SGGARRISONMISSILE',EliteOccupyWeapon='SGGARRISONMISSILEE',OccupyPip='PersonBlue')
for key in ['SGLINK','SGOP']:
 r[key].update(Occupier='yes',OccupyWeapon='SGSIDEARM',EliteOccupyWeapon='SGSIDEARM',OccupyPip='PersonBlue',OpenTransportWeapon='0')
