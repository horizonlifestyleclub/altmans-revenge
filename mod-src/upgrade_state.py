from pathlib import Path
p=Path('outputs/redalert2-recomp/src/linux/singularity.h');s=p.read_text()
s=s.replace('int captured;', 'int captured,research_started,blackout_enabled,cyber,state; uint32_t sink,core_uid;')
s=s.replace('static uint32_t sg_lastframe,sg_lasttick;', '''static uint32_t sg_lastframe,sg_lasttick;
enum {SG_OFFLINE,SG_READY,SG_RESEARCHING,SG_INTEGRATING,SG_ACTIVE,SG_SUSPENDED};
static const char*sg_states[]={"OFFLINE","READY","RESEARCHING","INTEGRATING","ACTIVE","SUSPENDED"};
static int sg_active(SGHouse*s){return s&&s->state==SG_ACTIVE&&s->powered;}
static int sg_enemy(uint32_t a,uint32_t b){if(!a||!b||a==b)return 0;int ix=MEM32(b+0x30);return ix>=0&&ix<32&&!(MEM32(a+0x5788)&(1u<<ix));}
''')
s=s.replace('return base+(s&&s->level>0&&!strcmp(sg_id(b,1),"SGCOLO")?400:0);', 'return base+(s&&s->level>0&&!strcmp(sg_id(b,1),"SGCOLO")?400:0)+(s&&s->cyber&&s->sink==b?100000:0);')
s=s.replace('return u && f<u->until;', '''if(u && f<u->until)return 1;
    SGHouse*h=sg_house(MEM32(p+0x21C));uint32_t vt=MEM32(p);
    if(vt==0x7E3EBC && !strcmp(sg_id(p,1),"SGCOLO"))return !sg_active(h);
    /* Deterministic fire-control failures: seven blocked windows in ten.
       Never consume the engine RNG or modify damage/health directly. */
    if(h&&h->cyber&&(vt==0x7F5C70||vt==0x7EB058||vt==0x7E22A4))return ((f/15+MEM32(p+0x10))%10)<7;
    return 0;''')
s=s.replace('||!strcmp(id,"SGHOUND");','||!strcmp(id,"SGHOUND")||!strcmp(id,"SGGUARD");')
s=s.replace('s->nodes=s->colossus=s->batteries=0;', 's->nodes=s->colossus=s->batteries=0;s->sink=0;')
s=s.replace('const char*id=sg_id(b,1);\n        if(!strcmp(id,"SGCOLO"))s->colossus++;','''const char*id=sg_id(b,1);if(!s->sink)s->sink=b;
        if(!strcmp(id,"SGCOLO")){s->colossus++;uint32_t uid=MEM32(b+0x10);if(s->core_uid!=uid){s->progress=0;s->level=0;s->research_started=0;s->core_uid=uid;}}''')
s=s.replace('int baseoutput=output-s->battery_output,deficit=drain-baseoutput;', '''/* The core has a hardened local power feed: enemy cyber pressure affects
           the ordinary grid, but cannot make two active cores oscillate on/off. */
        int physicaldrain=drain-(s->cyber?100000:0);if(physicaldrain<0)physicaldrain=0;
        int baseoutput=output-s->battery_output,deficit=physicaldrain-baseoutput;''')
s=s.replace('int powered=baseoutput+supply>=drain;s->powered=powered;','int powered=baseoutput+supply>=physicaldrain;s->powered=powered;')
s=s.replace('if(powered && (s->colossus || s->nodes>=5) && s->progress<5400)', 'if(s->research_started && powered && (s->colossus || s->nodes>=5) && s->progress<5400)')
s=s.replace('if(s->progress>=5400 && powered && (s->colossus||s->nodes))','if(s->research_started && s->progress>=5400 && powered && (s->colossus||s->nodes>=5))')
needle='        if(f%450==0 && (s->colossus || s->nodes))'
pos=s.index(needle)
s=s[:pos]+'''        int previous=s->state;
        if(!s->colossus&&!s->nodes){s->state=SG_OFFLINE;s->progress=0;s->level=0;s->research_started=0;s->core_uid=0;}
        else if(!powered || (!s->colossus&&s->nodes<5))s->state=SG_SUSPENDED;
        else if(!s->research_started)s->state=SG_READY;
        else if(s->progress<5400)s->state=SG_RESEARCHING;
        else if(s->level<1)s->state=SG_INTEGRATING;
        else s->state=SG_ACTIVE;
        if(previous!=s->state)fprintf(stderr,"[singularity] state house=%08X %s -> %s\\n",h,sg_states[previous],sg_states[s->state]);
''' +s[pos:]
needle='    int tn=sg_vec_count(0xA8EC78);'
pos=s.index(needle)
s=s[:pos]+'''    /* Compute all sources first, then apply opponents' grid faults. */
    for(int i=0;i<hn;i++){
        SGHouse*t=&sg_houses[i];int cyber=0;
        for(int j=0;j<hn;j++){SGHouse*a=&sg_houses[j];if(sg_active(a)&&a->blackout_enabled&&sg_enemy(a->ptr,t->ptr)){cyber=1;break;}}
        if(t->cyber!=cyber){t->cyber=cyber;MEM8(t->ptr+0x5778)=1;fprintf(stderr,"[singularity] cyber house=%08X %s\\n",t->ptr,cyber?"BLACKOUT + FIRE-CONTROL FAULTS":"RESTORED");}
    }
''' +s[pos:]
# Cyber faults must not also freeze movement; movement suppression is EMP only.
s=s.replace('double factor=sg_blocked(p)?.01:1.;','double factor=(f<u->until)?.01:1.;')
s+='''
/* A native technology menu, separate from the construction sidebar. */
static hle_menu_item_t sg_menu[12];
static char sg_status_label[128],sg_progress_label[128],sg_requirement_label[128];
static void sg_menu_chosen(int id){
    if(!sg_on())return;SGHouse*s=sg_house(MEM32(0xA83D4C));if(!s)return;
    if(id==8101 && s->state==SG_READY){s->research_started=1;s->blackout_enabled=1;fprintf(stderr,"[singularity] research requested by player\\n");}
    if(id==8102 && sg_active(s)){s->blackout_enabled=!s->blackout_enabled;fprintf(stderr,"[singularity] player toggled enemy grid disruption %d\\n",s->blackout_enabled);}
}
int sg_tech_key(int scan,int down){
    if(scan!=65+1 || !sg_on())return 0; /* SDL_SCANCODE_F8 = 65 (corrected below) */
    if(!down)return 1;
    SGHouse*s=sg_house(MEM32(0xA83D4C));int n=0;
    sg_menu[n++]=(hle_menu_item_t){"SINGULARITY | TECHNOLOGY",NULL,0,0,NULL,0};
    if(!s){sg_menu[n++]=(hle_menu_item_t){"Start a fresh skirmish to research technology",NULL,0,0,NULL,0};}
    else {
        snprintf(sg_status_label,sizeof sg_status_label,"State: %s",sg_states[s->state]);
        snprintf(sg_progress_label,sizeof sg_progress_label,"Research: %u%% | Integration: %.0f%%",s->progress*100/5400,s->level*100);
        snprintf(sg_requirement_label,sizeof sg_requirement_label,"Prerequisite: %s | Power: %s",s->colossus?"Colossus online":s->nodes>=5?"5 compute nodes":"Colossus OR 5 compute nodes",s->powered?"available":"insufficient");
        sg_menu[n++]=(hle_menu_item_t){sg_status_label,NULL,0,0,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){sg_requirement_label,NULL,0,0,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){sg_progress_label,NULL,0,0,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){NULL,NULL,0,0,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){"Research Singularity", "6 min + 2 min integration",s->state==SG_READY?8101:0,s->research_started,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){"Disrupt enemy power + targeting", "Requires ACTIVE",sg_active(s)?8102:0,s->blackout_enabled,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){"Colossus defence: 6 interceptors", "Launch against nearby threats",0,0,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){"Destroy core / cut its power to stop effects",NULL,0,0,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){"Reopen F8 to refresh | fresh skirmishes only",NULL,0,0,NULL,0};
    }
    hle_screen_menu(sg_menu,n,sg_menu_chosen);return 1;
}
'''
s=s.replace('scan!=65+1', 'scan!=65').replace(' /* SDL_SCANCODE_F8 = 65 (corrected below) */',' /* SDL_SCANCODE_F8 */')
p.write_text(s)
p=Path('outputs/redalert2-recomp/src/linux/menu.c');s=p.read_text().replace('static int on_key(int scancode, int down) {','extern int sg_tech_key(int scancode,int down);\nstatic int on_key(int scancode, int down) {\n    if(sg_tech_key(scancode,down))return 1;');p.write_text(s)
