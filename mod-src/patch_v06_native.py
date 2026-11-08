from pathlib import Path
root=__import__('sgpaths').ROOT/'outputs/redalert2-recomp/src/recomp/gen'
p=root/'recomp_0046.c';s=p.read_text()
if 'SG v06 manual drone weapon mode' not in s:
 s=s.replace('RECOMP_ENTER(0x006F3330u);','''RECOMP_ENTER(0x006F3330u);
    /* SG v06 manual drone weapon mode */
    { extern int sg_weapon_mode(uint32_t); int mode=sg_weapon_mode(ecx);
      if(mode>=0){eax=(uint32_t)mode; RECOMP_FLAGS_OUT(); esp+=8; RECOMP_REGS_OUT(); return;} }
''');p.write_text(s)
p=root/'recomp_0047.c';s=p.read_text()
if 'SG v06 controller kill credit' not in s:
 s=s.replace('RECOMP_ENTER(0x00702D40u);','''RECOMP_ENTER(0x00702D40u);
    /* SG v06 controller kill credit */
    { extern uint32_t sg_credit_source(uint32_t); MEM32(esp+4)=sg_credit_source(MEM32(esp+4)); }
''');p.write_text(s)

p=root/'recomp_0024.c';s=p.read_text()
line='    ecx = 0x0087F5D8u; /* 0x0055B4E1: mov ecx, 0x87f5d8 */'
if 'SG v06 safe simulation boundary' not in s:
 assert line in s
 hook='    /* SG v06 safe simulation boundary */\n    { extern void sg_tick(void); RECOMP_FLAGS_OUT(); RECOMP_REGS_OUT(); sg_tick(); RECOMP_REGS_IN(); RECOMP_FLAGS_IN(); }\n'
 s=s.replace(line,hook+line);p.write_text(s)

p=root/'recomp_0046.c';s=p.read_text()
if 'SG v06 consume drone after payload' not in s:
 s=s.replace('RECOMP_ENTER(0x006FDD50u);','RECOMP_ENTER(0x006FDD50u);\n    /* SG v06 consume drone after payload */\n    { extern void sg_drone_fired(uint32_t,int); if(MEM32(esp+4))sg_drone_fired(ecx,(int)MEM32(esp+8)); }')
 p.write_text(s)
