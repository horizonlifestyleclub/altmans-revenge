from pathlib import Path
import re
root=__import__('sgpaths').ROOT/'outputs/redalert2-recomp'
gen=root/'src/recomp/gen'
def patch_function(path,name,transform):
    s=path.read_text();pattern=r'(void '+name+r'\(void\) \{.*?)(?=\nvoid |\Z)'
    m=re.search(pattern,s,re.S);assert m,name
    if '/* SG PATCH */' in m[1]:return
    new=transform(m[1]);path.write_text(s[:m.start()]+new+s[m.end():])
def cost(s):
    s=s.replace('RECOMP_ENTER(0x0050C0A0u);','RECOMP_ENTER(0x0050C0A0u);\n    /* SG PATCH */ extern double sg_costfactor(uint32_t,uint32_t);\n    uint32_t _sg_h=ecx,_sg_t=MEM32(esp+4);')
    return s.replace('RECOMP_FLAGS_OUT(); esp += 8;', '_st[0] *= sg_costfactor(_sg_h,_sg_t); RECOMP_FLAGS_OUT(); esp += 8;')
patch_function(gen/'recomp_0017.c','sub_0050C0A0',cost)
p=gen/'recomp_0046.c';s=p.read_text();line='fp_push((double)(int32_t)MEM32(edi + 0xB0)); /* 0x006FD0B5: fild dword ptr [edi + 0xb0] */'
if line in s:s=s.replace(line,'{ extern double sg_rof(uint32_t); fp_push((double)(int32_t)MEM32(edi + 0xB0) * sg_rof(esi)); } /* SG PATCH: rearm */');p.write_text(s)
def time(s):
    s=s.replace('RECOMP_ENTER(0x006F47A0u);','RECOMP_ENTER(0x006F47A0u);\n    /* SG PATCH */ extern double sg_buildfactor(uint32_t); uint32_t _sg_obj=ecx;')
    return re.sub(r'(RECOMP_FLAGS_OUT\(\); esp \+= 4;)',r'eax=(uint32_t)((double)eax*sg_buildfactor(_sg_obj)); \1',s)
patch_function(gen/'recomp_0046.c','sub_006F47A0',time)
def powerhook(name,va):
    def fix(s):
        s=s.replace('RECOMP_ENTER(0x'+va+'u);','RECOMP_ENTER(0x'+va+'u);\n    /* SG PATCH */ extern int '+name+'(uint32_t,int); uint32_t _sg_b=ecx;')
        return s.replace('RECOMP_FLAGS_OUT(); esp += 4;','eax='+name+'(_sg_b,(int)eax); RECOMP_FLAGS_OUT(); esp += 4;')
    return fix
patch_function(gen/'recomp_0005.c','sub_0044E7B0',powerhook('sg_power','0044E7B0'))
patch_function(gen/'recomp_0005.c','sub_0044E880',powerhook('sg_drain','0044E880'))
p=gen/'recomp_0046.c';s=p.read_text();marker='L_006FC0C5:\n'
if '/* SG fire gate */' not in s:
    s=s.replace(marker,marker+'    { extern int sg_blocked(uint32_t); if(sg_blocked(esi)) goto L_006FC0DF; } /* SG fire gate */\n');p.write_text(s)
print('Applied repeatable local native hooks. Reapply after regenerating lifted C.')
