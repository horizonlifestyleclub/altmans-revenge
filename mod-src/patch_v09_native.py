from pathlib import Path
root=__import__('sgpaths').ROOT/'outputs/redalert2-recomp/src/recomp/gen'
def entry(file,va,label,code):
 p=root/file;s=p.read_text();marker='/* SG v09 '+label+' */'
 if marker in s:return
 old=f'RECOMP_ENTER(0x{va}u);';assert old in s,(file,va)
 s=s.replace(old,old+'\n    '+marker+'\n    '+code);p.write_text(s)
# BuildingClass::ReceiveDamage(int* damage, int distance, WarheadTypeClass* wh, ...)
entry('recomp_0005.c','00442230','B-2 building damage clamp','{ extern void sg_building_damage(uint32_t,uint32_t,uint32_t); RECOMP_FLAGS_OUT(); RECOMP_REGS_OUT(); sg_building_damage(ecx,MEM32(esp+4),MEM32(esp+0xC)); RECOMP_REGS_IN(); RECOMP_FLAGS_IN(); }')
print('v09 native patches applied')
