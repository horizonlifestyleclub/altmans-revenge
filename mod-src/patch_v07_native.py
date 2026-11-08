from pathlib import Path
root=__import__('sgpaths').ROOT/'outputs/redalert2-recomp/src/recomp/gen'
def entry(file,va,label,code):
 p=root/file;s=p.read_text();marker='/* SG v07 '+label+' */'
 if marker in s:return
 old=f'RECOMP_ENTER(0x{va}u);';assert old in s,(file,va)
 s=s.replace(old,old+'\n    '+marker+'\n    '+code);p.write_text(s)
for va,cat in [('004FE3E0',0),('004FEA60',1),('004FEEE0',2),('004FF210',3)]:
 entry('recomp_0017.c',va,'adaptive production '+str(cat),'{ extern int sg_ai_production(uint32_t,int); RECOMP_FLAGS_OUT(); RECOMP_REGS_OUT(); int handled=sg_ai_production(ecx,'+str(cat)+'); RECOMP_REGS_IN(); RECOMP_FLAGS_IN(); if(handled){eax=15;RECOMP_FLAGS_OUT();esp+=4;RECOMP_REGS_OUT();return;} }')
entry('recomp_0017.c','005098F0','adaptive no blind superweapons','{ extern int sg_ai_house(uint32_t); if(sg_ai_house(ecx)){RECOMP_FLAGS_OUT();esp+=4;RECOMP_REGS_OUT();return;} }')
for file,va in [('recomp_0046.c','006FCDB0'),('recomp_0018.c','0051B1F0'),('recomp_0005.c','00443B90')]:
 entry(file,va,'fair target filter','{ extern int sg_ai_target_allowed(uint32_t,uint32_t);if(!sg_ai_target_allowed(ecx,MEM32(esp+4)))MEM32(esp+4)=0; }')
for file,va in [('recomp_0050.c','00741970'),('recomp_0018.c','0051AA40'),('recomp_0001.c','0041AA80')]:
 # Locate the actual generated file when a unit class spans a partition.
 matches=[p for p in root.glob('recomp_*.c') if f'RECOMP_ENTER(0x{va}u);' in p.read_text()]
 assert len(matches)==1,(va,matches)
 entry(matches[0].name,va,'fair pursuit filter','{ extern int sg_ai_destination_allowed(uint32_t,uint32_t);if(!sg_ai_destination_allowed(ecx,MEM32(esp+4)))MEM32(esp+4)=0; }')

# The original threat grid otherwise samples every enemy unit on the map.
p=root/'recomp_0017.c';s=p.read_text();marker='/* SG v07 fair threat grid */'
if marker not in s:
 s=s.replace('L_0050944D:', 'L_0050944D:\n    '+marker+'\n    { extern int sg_ai_observable_for(uint32_t,uint32_t);if(!sg_ai_observable_for(ebx,esi))goto L_005095B6; }')
 p.write_text(s)
