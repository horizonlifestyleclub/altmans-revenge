"""v10 native hooks: small (infantry-size) health bar for operator drones;
infantry small arms may target drones but not other aircraft or Rocketeers."""
from pathlib import Path
root=__import__('sgpaths').ROOT/'outputs/redalert2-recomp/src/recomp/gen'
def entry(file,va,label,code):
 p=root/file;s=p.read_text();marker='/* SG v10 '+label+' */'
 if marker in s:return
 old=f'RECOMP_ENTER(0x{va}u);';assert old in s,(file,va)
 s=s.replace(old,old+'\n    '+marker+'\n    '+code);p.write_text(s)
def before(file,anchor,label,code):
 p=root/file;s=p.read_text();marker='/* SG v10 '+label+' */'
 if marker in s:return
 assert s.count(anchor)==1,(file,anchor)
 s=s.replace(anchor,'    '+marker+'\n    '+code+'\n'+anchor);p.write_text(s)
# TechnoClass::DrawHealthBar: WhatAmI()==Infantry (15) picks the short 8-pip bar.
before('recomp_0046.c','    /* cmp eax, 0xFu */ /* 0x006F6843: cmp eax, 0xf */','drone small health bar',
 '{ extern int sg_small_bar(uint32_t); RECOMP_FLAGS_OUT(); RECOMP_REGS_OUT(); int small=sg_small_bar(esi); RECOMP_REGS_IN(); RECOMP_FLAGS_IN(); if(small)eax=0xF; }')
# TechnoClass::SetTarget and InfantryClass::SetTarget.
for file,va in [('recomp_0046.c','006FCDB0'),('recomp_0018.c','0051B1F0')]:
 entry(file,va,'drone-only infantry AA','{ extern int sg_aa_target_allowed(uint32_t,uint32_t); RECOMP_FLAGS_OUT(); RECOMP_REGS_OUT(); int ok=sg_aa_target_allowed(ecx,MEM32(esp+4)); RECOMP_REGS_IN(); RECOMP_FLAGS_IN(); if(!ok)MEM32(esp+4)=0; }')
print('v10 native patches applied')
# TechnoClass "can generate a gap" virtual (0x701190) is always false for
# units; the Cyber Warfare Unit is a mobile gap generator.
entry('recomp_0047.c','00701190','mobile gap unit','{ extern int sg_gap_unit(uint32_t); RECOMP_FLAGS_OUT(); RECOMP_REGS_OUT(); int g=sg_gap_unit(ecx); RECOMP_REGS_IN(); RECOMP_FLAGS_IN(); if(g){SET_LO8(eax,1);RECOMP_FLAGS_OUT();esp+=4;RECOMP_REGS_OUT();return;} }')
print('v10 mobile gap patch applied')
