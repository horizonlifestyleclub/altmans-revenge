/* Local single-player Singularity extension for gamemd.exe 1.001 only.
 * Layouts cross-checked with YRpp and the lifted binary. Never runs for other mods.
 * Experimental progression state is session-local; see mod status documentation.
 */
#include <math.h>
#include "mods.h"
#ifndef SG_RESEARCH_TICKS
#define SG_RESEARCH_TICKS 5400
#endif
#ifndef SG_INTEGRATION_TICKS
#define SG_INTEGRATION_TICKS 1800
#endif
typedef struct { uint32_t ptr,progress,lastpulse; int nodes,colossus,batteries,powered,battery_output; double charge,level; float basecost[3]; int captured,research_started,blackout_enabled,cyber,state; uint32_t sink,core_uid; } SGHouse;
static SGHouse sg_houses[32];
static uint32_t sg_lastframe,sg_lasttick;
enum {SG_OFFLINE,SG_READY,SG_RESEARCHING,SG_INTEGRATING,SG_ACTIVE,SG_SUSPENDED};
static const char*sg_states[]={"OFFLINE","READY","RESEARCHING","INTEGRATING","ACTIVE","SUSPENDED"};
static int sg_active(SGHouse*s){return s&&s->state==SG_ACTIVE&&s->powered;}
static int sg_enemy(uint32_t a,uint32_t b){if(!a||!b||a==b)return 0;uint32_t type=MEM32(b+0x34);if(type>=0x10000&&MEM8(type+0x1A6))return 0;int ix=MEM32(b+0x30);return ix>=0&&ix<32&&!(MEM32(a+0x5788)&(1u<<ix));}

typedef struct {uint32_t ptr,uid,until,immune,pulse; double speedfactor; uint32_t spent_frame; uint32_t children[5],child_uid[5],ready[5],controller,controller_uid; float credited_rank; int suicide,nest_active;} SGUnit;
static SGUnit sg_units[8192];
static SGUnit *sg_state(uint32_t p){
    uint32_t uid=MEM32(p+0x10);unsigned slot=(p>>4)%8192;
    for(unsigned n=0;n<8192;n++,slot=(slot+1)%8192){SGUnit*s=&sg_units[slot];if(!s->ptr||s->ptr==p){if(s->uid!=uid||s->ptr!=p){memset(s,0,sizeof *s);s->ptr=p;s->uid=uid;s->speedfactor=1;}return s;}}
    return NULL;
}
static int sg_alive(uint32_t p) { return p>=0x10000 && p<0xF0000000 && MEM8(p+0x90) && !MEM8(p+0x81) && (int32_t)MEM32(p+0x6C)>0; }
static int sg_vec_count(uint32_t v) { int n=(int)MEM32(v+0x10); return n>=0 && n<8192?n:0; }
static uint32_t sg_vec_at(uint32_t v,int i) { return MEM32(MEM32(v+4)+4*i); }
static const char *sg_id(uint32_t p,int building) {
    uint32_t vt=MEM32(p),t=MEM32(p+(building?0x520:(vt==0x007F5C70?0x6C4:0x6C0)));
    if(t<0x10000 || t>0xEFFFFFFF)return "";
    return (const char*)hle_ptr(t+0x24);
}
static SGHouse *sg_house(uint32_t p) { for(int i=0;i<32;i++)if(sg_houses[i].ptr==p)return &sg_houses[i];return NULL; }
static int sg_on(void){return (!strcmp(mods_active(),"Singularity")||!strcmp(mods_active(),"Singularity - Adaptive")) && MEM32(0xA8B238)==5;}
int sg_power(uint32_t b,int base){if(!sg_on())return base;SGHouse*s=sg_house(MEM32(b+0x21C));if(!s||strcmp(sg_id(b,1),"SGBATT")||!s->batteries)return base;return base+s->battery_output/s->batteries;}
int sg_drain(uint32_t b,int base){if(!sg_on())return base;SGHouse*s=sg_house(MEM32(b+0x21C));return base+(s&&s->level>0&&!strcmp(sg_id(b,1),"SGCOLO")?400:0)+(s&&s->cyber&&s->sink==b?100000:0);}
static double sg_dist(uint32_t a,uint32_t b){double dx=(int32_t)MEM32(a+0x9C)-(int32_t)MEM32(b+0x9C),dy=(int32_t)MEM32(a+0xA0)-(int32_t)MEM32(b+0xA0);return sqrt(dx*dx+dy*dy)/256.;}
int sg_ai_target_allowed(uint32_t,uint32_t);
int sg_blocked(uint32_t p){
    if(!sg_on()||!p)return 0;
    if(!sg_ai_target_allowed(p,MEM32(p+0x2B4)))return 1;SGUnit*u=sg_state(p);uint32_t f=MEM32(RA2_GAME_FRAME_VA);
    if(u && f<u->until)return 1;
    SGHouse*h=sg_house(MEM32(p+0x21C));uint32_t vt=MEM32(p);
    if(vt==0x7E3EBC){const char*id=sg_id(p,1);if(!strcmp(id,"SGCOLO")||!strncmp(id,"SGNODE",6)||!strcmp(id,"GAWEAP")||!strcmp(id,"SGFACT"))return !sg_active(h);}
    /* Deterministic fire-control failures: seven blocked windows in ten.
       Never consume the engine RNG or modify damage/health directly. */
    if(h&&h->cyber&&(vt==0x7F5C70||vt==0x7EB058||vt==0x7E22A4))return ((f/15+MEM32(p+0x10))%10)<7;
    return 0;
}
static int sg_drone(const char*id){return !strcmp(id,"SGPDRONE")||!strcmp(id,"SGTANK")||!strcmp(id,"SGSCOUT")||!strcmp(id,"SGSERAPH")||!strcmp(id,"SGNEEDLE")||!strcmp(id,"SGLOCUST")||!strcmp(id,"SGINT")||!strcmp(id,"SGNDRONE")||!strcmp(id,"SGWDRONE")||!strcmp(id,"SGHOUND")||!strcmp(id,"SGGUARD")||!strcmp(id,"SGKAMI")||!strcmp(id,"SGFACTDRONE");}
double sg_costfactor(uint32_t h,uint32_t t){
    if(!sg_on())return 1;SGHouse*s=sg_house(h);if(!s||!s->powered||!t)return 1;
    const char*id=(const char*)hle_ptr(t+0x24);
    /* Only mobile types: buildings never inherit unit discounts. */
    uint32_t vt=MEM32(t);if(vt!=0x7F6218&&vt!=0x7EB610&&vt!=0x7E2868)return 1;
    if(s->colossus)return .9-.4*s->level;
    double nodes=fmin(s->nodes,5)/5.;return 1-.15*nodes-(sg_drone(id)?.5:0)*s->level*nodes;
}
double sg_buildfactor(uint32_t obj){
    if(!sg_on()||!obj)return 1;uint32_t vt=MEM32(obj);
    if(vt!=0x007F5C70&&vt!=0x007EB058&&vt!=0x007E22A4)return 1;
    uint32_t t=MEM32(obj+(vt==0x007F5C70?0x6C4:0x6C0)),h=MEM32(obj+0x21C);SGHouse*s=sg_house(h);
    if(!s||!s->powered)return 1;
    double cost=sg_costfactor(h,t),time=s->colossus?.9-.4*s->level:1-.2*fmin(s->nodes,5)/5.-(sg_drone((const char*)hle_ptr(t+0x24))?.4:0)*s->level*fmin(s->nodes,5)/5.;
    return time/cost;
}
double sg_rof(uint32_t obj){
    if(!sg_on()||!obj)return 1;SGHouse*s=sg_house(MEM32(obj+0x21C));
    if(!s||!s->powered||!s->colossus)return 1;
    uint32_t vt=MEM32(obj);int building=vt!=0x007F5C70&&vt!=0x007EB058&&vt!=0x007E22A4;
    return 1./(1.+s->level*(sg_drone(sg_id(obj,building))?.25:.15));
}
/* Garrisoned infantry are in limbo but still own their drone slots. Only actual
   friendly building occupants qualify; passengers in transports do not. */
static uint32_t sg_controller_anchor(uint32_t p){
 if(sg_alive(p))return p;
 if(p<0x10000||MEM32(p)!=0x7EB058||!MEM8(p+0x90)||(int32_t)MEM32(p+0x6C)<=0)return 0;
 for(int i=0;i<sg_vec_count(0xA8EB40);i++){
  uint32_t b=sg_vec_at(0xA8EB40,i);if(!sg_alive(b)||MEM32(b+0x21C)!=MEM32(p+0x21C))continue;
  for(int j=0;j<sg_vec_count(b+0x684);j++)if(sg_vec_at(b+0x684,j)==p)return b;
 }
 return 0;
}
/* Individually controllable drones. Uses the engine's UnitType spawn routine;
   each owned slot has an independent replacement deadline. */
static uint32_t sg_find_unit_type(const char *id){
 for(int i=0;i<sg_vec_count(0xA83CE0);i++){uint32_t t=sg_vec_at(0xA83CE0,i);if(t>0x10000&&!strcmp((const char*)hle_ptr(t+0x24),id))return t;}return 0;
}
#ifndef SG_STATE_TEST
static void sg_launch_drone(uint32_t p){
 /* Start the native jumpjet locomotor so freshly created drones take off. */
 uint32_t loco=MEM32(p+0x674);if(!loco||MEM32(loco)!=0x7ECD68)return;
 uint32_t args[4]={loco,MEM32(p+0x9C),MEM32(p+0xA0),MEM32(p+0xA4)};
 hle_call_guest(MEM32(MEM32(loco)+17*4),4,args);
}
static uint32_t sg_make_drone(uint32_t parent,int slot){
 uint32_t t=sg_find_unit_type("SGPDRONE"),anchor=sg_controller_anchor(parent);if(!t||!anchor)return 0;
 uint32_t saved=g_ecx,owner=MEM32(parent+0x21C);
 g_ecx=t;uint32_t child=hle_call_guest(MEM32(MEM32(t)+0x8C),1,&owner);g_ecx=saved;
 if(!child)return 0;
 int32_t *coord=hle_heap_alloc(12,1);
 if(!coord){g_ecx=child;hle_call_guest(0x5F65F0,0,NULL);g_ecx=saved;return 0;}
 static const int offsets[8][2]={{1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,-1},{1,-1},{-1,1}};
 uint32_t args[2]={hle_va(coord),0},ok=0;
 for(int attempt=0;attempt<24&&!ok;attempt++){
  int dir=(slot+attempt)%8,radius=1+attempt/8;
  coord[0]=(int32_t)MEM32(anchor+0x9C)+256*radius*offsets[dir][0];
  coord[1]=(int32_t)MEM32(anchor+0xA0)+256*radius*offsets[dir][1];
  coord[2]=(int32_t)MEM32(anchor+0xA4);
  g_ecx=child;ok=hle_call_guest(MEM32(MEM32(child)+0xD8),2,args);g_ecx=saved;
 }
 hle_heap_free(coord);
 if(!ok){g_ecx=child;hle_call_guest(0x5F65F0,0,NULL);g_ecx=saved;return 0;}
 sg_launch_drone(child);
 SGUnit*u=sg_state(child);if(u){u->controller=parent;u->controller_uid=MEM32(parent+0x10);}
 return child;
}
#else
static uint32_t sg_make_drone(uint32_t parent,int slot){(void)parent;(void)slot;return 0;}
#endif
static float sg_rank(uint32_t p){float v;memcpy(&v,hle_ptr(p+0x150),4);return isfinite(v)&&v>=0&&v<100?v:0;}
static int sg_capacity(uint32_t p,SGUnit*u){const char*id=sg_id(p,0);if(!strcmp(id,"SGTANK"))return u->nest_active?5:0;float rank=sg_rank(p);return rank>=2?3:rank>=1?2:1;}
static uint32_t sg_regen(uint32_t p){float rank=sg_rank(p);return rank>=2?225:rank>=1?330:450;}
static void sg_control_tick(uint32_t f){
#ifndef SG_STATE_TEST
 for(int i=sg_vec_count(0xA8EC78)-1;i>=0;i--){
  uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p)||MEM32(p)!=0x7F5C70||strcmp(sg_id(p,0),"SGPDRONE"))continue;
  SGUnit*u=sg_state(p);if(u&&u->spent_frame&&f>=u->spent_frame){uint32_t saved=g_ecx;g_ecx=p;hle_call_guest(0x5F65F0,0,NULL);g_ecx=saved;}
 }
#endif
 int n=sg_vec_count(0xA8EC78);
 for(int i=0;i<n;i++){
  uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_controller_anchor(p))continue;
  uint32_t vt=MEM32(p);if(vt!=0x7F5C70&&vt!=0x7EB058)continue;
  const char*id=sg_id(p,0);if(strcmp(id,"SGLINK")&&strcmp(id,"SGOP")&&strcmp(id,"SGTANK"))continue;
  SGUnit*u=sg_state(p);if(!u||f<u->until)continue;int capacity=sg_capacity(p,u);uint32_t regen=sg_regen(p);
  for(int j=0;j<5;j++){
   uint32_t d=u->children[j];int alive=d&&MEM32(d+0x10)==u->child_uid[j]&&sg_alive(d);
   if(alive){
    SGUnit*du=sg_state(d);if(du){float rank=sg_rank(d);if(rank>du->credited_rank){float parentrank=sg_rank(p)+(rank-du->credited_rank)*150.f/700.f;memcpy(hle_ptr(p+0x150),&parentrank,4);du->credited_rank=rank;}}
    continue;
   }
   if(d){u->children[j]=0;u->ready[j]=f+regen;fprintf(stderr,"[drones] slot %d replacement in %u ticks\n",j+1,regen);}
   if(j>=capacity)continue;
   if(!u->ready[j])u->ready[j]=f+15;
   if(f>=u->ready[j]){uint32_t child=sg_make_drone(p,j);if(child){u->children[j]=child;u->child_uid[j]=MEM32(child+0x10);u->ready[j]=0;fprintf(stderr,"[drones] %s slot %d launched controllable drone %08X\n",id,j+1,child);}else u->ready[j]=f+30;}
  }
 }
}
void sg_drone_fired(uint32_t p,int weapon){
 if(!sg_on()||weapon!=1||!p||MEM32(p)!=0x7F5C70||strcmp(sg_id(p,0),"SGPDRONE"))return;
 SGUnit*u=sg_state(p);if(u&&!u->spent_frame)u->spent_frame=MEM32(RA2_GAME_FRAME_VA)+1;
}
uint32_t sg_credit_source(uint32_t killer){
 if(!sg_on()||!killer||MEM32(killer)!=0x7F5C70||strcmp(sg_id(killer,0),"SGPDRONE"))return killer;
 SGUnit*u=sg_state(killer);if(u&&u->controller&&sg_controller_anchor(u->controller)&&MEM32(u->controller+0x10)==u->controller_uid&&MEM32(u->controller+0x21C)==MEM32(killer+0x21C))return u->controller;
 return killer;
}
int sg_weapon_mode(uint32_t p){
 if(!sg_on()||!p||MEM32(p)!=0x7F5C70||strcmp(sg_id(p,0),"SGPDRONE"))return -1;
 SGUnit*u=sg_state(p);return u&&u->suicide?1:0;
}
static int sg_drone_command(int scan,int down){
 if((scan!=7&&scan!=66)||!sg_on())return 0; /* D or F9 */
 int handled=0;uint32_t player=MEM32(0xA83D4C);
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){
  uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p)||!MEM8(p+0x83)||MEM32(p+0x21C)!=player||MEM32(p)!=0x7F5C70)continue;
  const char*id=sg_id(p,0);SGUnit*u=sg_state(p);if(!u)continue;
  if(!strcmp(id,"SGPDRONE")){handled=1;if(down){u->suicide=!u->suicide;fprintf(stderr,"[drones] selected drone mode %s\n",u->suicide?"SUICIDE":"NORMAL");}}
  if(!strcmp(id,"SGTANK")){handled=1;if(down){u->nest_active=!u->nest_active;fprintf(stderr,"[drones] nest tank %s\n",u->nest_active?"DEPLOYED":"MOBILE");}}
 }
 return handled;
}

#include "adaptive_ai.h"
#ifdef SG_V07_LAB
#include "@WORKSPACE@/work/singularity/lab_v07.h"
#endif
#ifdef SG_DRONE_LAB
#include "@WORKSPACE@/work/singularity/drone_lab_v06.h"
#endif
void sg_tick(void) {
#ifdef RA2_TARGET_GAMEMD
    if(!sg_on()){hle_screen_status("");return;}
    uint32_t f=MEM32(RA2_GAME_FRAME_VA);
    if(f==sg_lastframe || !f)return;
    if(f<sg_lastframe){memset(sg_houses,0,sizeof sg_houses);memset(sg_units,0,sizeof sg_units);memset(sg_ai_brains,0,sizeof sg_ai_brains);sg_lasttick=0;}
    sg_lastframe=f;
    sg_ai_sanitize();
    uint32_t elapsed=f-sg_lasttick;if(elapsed<15)return;sg_lasttick=f;
    double seconds=elapsed/15.;
    int hn=sg_vec_count(0xA80228);if(hn>32)return;
    for(int i=0;i<hn;i++) {
        uint32_t h=sg_vec_at(0xA80228,i);SGHouse *s=&sg_houses[i];
        if(s->ptr!=h){memset(s,0,sizeof *s);s->ptr=h;}
        s->nodes=s->colossus=s->batteries=0;s->sink=0;
    }
    int bn=sg_vec_count(0xA8EB40);
    for(int i=0;i<bn;i++){
        uint32_t b=sg_vec_at(0xA8EB40,i);if(!sg_alive(b))continue;
        SGHouse*s=sg_house(MEM32(b+0x21C));if(!s)continue;
        const char*id=sg_id(b,1);if(!s->sink)s->sink=b;
        if(!strcmp(id,"SGCOLO")){s->colossus++;uint32_t uid=MEM32(b+0x10);if(s->core_uid!=uid){s->progress=0;s->level=0;s->research_started=0;s->core_uid=uid;}}
        if(!strncmp(id,"SGNODE",6))s->nodes++;
        if(!strcmp(id,"SGBATT"))s->batteries++;
    }
    for(int i=0;i<hn;i++){
        SGHouse*s=&sg_houses[i];uint32_t h=s->ptr;
        int output=(int)MEM32(h+0x53A4),drain=(int)MEM32(h+0x53A8);
        /* The core has a hardened local power feed: enemy cyber pressure affects
           the ordinary grid, but cannot make two active cores oscillate on/off. */
        int physicaldrain=drain-(s->cyber?100000:0);if(physicaldrain<0)physicaldrain=0;
        int baseoutput=output-s->battery_output,deficit=physicaldrain-baseoutput;
        s->charge=fmin(s->charge,s->batteries*6000.);
        int supply=0;
        if(deficit>0&&s->batteries&&s->charge>0){supply=(int)fmin(fmin(deficit,s->batteries*200),s->charge/seconds);supply=(supply/s->batteries)*s->batteries;s->charge-=supply*seconds;}
        else if(deficit<0)s->charge=fmin(s->batteries*6000.,s->charge+fmin(-deficit,s->batteries*100*(1+.25*s->level))*.9*seconds);
        if(supply!=s->battery_output){s->battery_output=supply;MEM8(h+0x5778)=1;}
        int powered=baseoutput+supply>=physicaldrain;s->powered=powered;
        if(s->research_started && powered && (s->colossus || s->nodes>=5) && s->progress<SG_RESEARCH_TICKS)s->progress=(uint32_t)fmin(SG_RESEARCH_TICKS,s->progress+elapsed);
        if(s->research_started && s->progress>=SG_RESEARCH_TICKS && powered && (s->colossus||s->nodes>=5)){if(s->level==0)MEM8(h+0x5778)=1;s->level=fmin(1.,s->level+(double)elapsed/SG_INTEGRATION_TICKS);}
        int previous=s->state;
        if(!s->colossus&&!s->nodes){s->state=SG_OFFLINE;s->progress=0;s->level=0;s->research_started=0;s->core_uid=0;}
        else if(!powered || (!s->colossus&&s->nodes<5))s->state=SG_SUSPENDED;
        else if(!s->research_started)s->state=SG_READY;
        else if(s->progress<SG_RESEARCH_TICKS)s->state=SG_RESEARCHING;
        else if(s->level<1)s->state=SG_INTEGRATING;
        else s->state=SG_ACTIVE;
        if(previous!=s->state)fprintf(stderr,"[singularity] state house=%08X %s -> %s\n",h,sg_states[previous],sg_states[s->state]);
        if(f%450==0 && (s->colossus || s->nodes))fprintf(stderr,"[singularity] house=%08X nodes=%d colossus=%d powered=%d research=%u ticks\n",h,s->nodes,s->colossus,powered,s->progress);
    }
    /* Compute all sources first, then apply opponents' grid faults. */
    for(int i=0;i<hn;i++){
        SGHouse*t=&sg_houses[i];int cyber=0;
        for(int j=0;j<hn;j++){SGHouse*a=&sg_houses[j];if(sg_active(a)&&a->blackout_enabled&&sg_enemy(a->ptr,t->ptr)){cyber=1;break;}}
        if(t->cyber!=cyber){t->cyber=cyber;MEM8(t->ptr+0x5778)=1;fprintf(stderr,"[singularity] cyber house=%08X %s\n",t->ptr,cyber?"BLACKOUT + FIRE-CONTROL FAULTS":"RESTORED");}
    }
    sg_control_tick(f);
    sg_ai_tick(f);
#ifdef SG_V07_LAB
    sg_v07_lab(f);
#endif
#ifdef SG_DRONE_LAB
    sg_drone_lab(f);
#endif
    int tn=sg_vec_count(0xA8EC78);
    /* Autonomous defensive pulses. No health writes and no forced aircraft crash. */
    for(int i=0;i<bn;i++){
        uint32_t b=sg_vec_at(0xA8EB40,i);if(!sg_alive(b))continue;
        const char*id=sg_id(b,1);int us=!strcmp(id,"SGAEGIS"),cn=!strcmp(id,"SGJAM");if(!us&&!cn)continue;
        SGHouse*h=sg_house(MEM32(b+0x21C));if(!h||!h->powered)continue;
        SGUnit*em=sg_state(b);if(!em||f<em->pulse)continue;int hit=0;
        for(int j=0;j<tn;j++){
            uint32_t p=sg_vec_at(0xA8EC78,j);if(!sg_alive(p))continue;
            uint32_t targethouse=MEM32(p+0x21C),sourcehouse=MEM32(b+0x21C);int targetindex=MEM32(targethouse+0x30);
            if(targethouse==sourcehouse || (targetindex>=0&&targetindex<32&&(MEM32(sourcehouse+0x5788)&(1u<<targetindex))))continue;
            uint32_t vt=MEM32(p);if(vt!=0x007F5C70&&vt!=0x007EB058&&vt!=0x007E22A4)continue;
            const char*pid=sg_id(p,0);if(!sg_drone(pid)&&strcmp(pid,"SGLINK")&&strcmp(pid,"SGOP"))continue;
            if(sg_dist(b,p)>(us?6:3))continue;SGUnit*u=sg_state(p);if(!u||f<u->immune)continue;
            u->until=f+90;u->immune=f+240;hit++;
        }
        if(hit){em->pulse=f+(us?900:675);fprintf(stderr,"[singularity] %s EMP suppressed %d targets for 90 simulation frames\n",id,hit);}
    }
    for(int i=0;i<tn;i++){
        uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p))continue;uint32_t vt=MEM32(p);
        if(vt!=0x007F5C70&&vt!=0x007EB058&&vt!=0x007E22A4)continue;
        SGUnit*u=sg_state(p);SGHouse*h=sg_house(MEM32(p+0x21C));if(!u)continue;
        double factor=(f<u->until)?.01:1.;
        if(u->nest_active&&!strcmp(sg_id(p,0),"SGTANK"))factor=0.;
        if(h&&h->powered&&h->level>0&&(h->colossus||sg_drone(sg_id(p,0))))factor*=1.+.1*h->level*(h->colossus?1:fmin(h->nodes,5)/5.);
        double old;memcpy(&old,hle_ptr(p+0x580),8);
        if(old>=0&&old<100){old=(u->speedfactor>0?old/u->speedfactor:1.)*factor;memcpy(hle_ptr(p+0x580),&old,8);u->speedfactor=factor;}
    }
    SGHouse *player=sg_house(MEM32(0xA83D4C));
    char hud[256]="";
    if(player&&(player->colossus||player->nodes)) {
        int remaining=(int)((SG_RESEARCH_TICKS-player->progress+14)/15)+(int)ceil((1-player->level)*SG_INTEGRATION_TICKS/15.);
        if(sg_active(player)) snprintf(hud,sizeof hud,"SINGULARITY ACTIVE | GRID %s | F8",player->blackout_enabled?"OFFLINE":"ONLINE");
        else if(player->research_started)snprintf(hud,sizeof hud,"SINGULARITY %02d:%02d | %s | F8",remaining/60,remaining%60,sg_states[player->state]);
        else if(player->state==SG_READY)snprintf(hud,sizeof hud,"SINGULARITY READY | F8");
        else snprintf(hud,sizeof hud,"SINGULARITY SUSPENDED | F8");
    }
    for(int i=0;i<sg_vec_count(0xA8EC78);i++){
        uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p)||(MEM32(p)!=0x7F5C70&&MEM32(p)!=0x7EB058)||!MEM8(p+0x83)||MEM32(p+0x21C)!=MEM32(0xA83D4C))continue;
        const char*id=sg_id(p,0);SGUnit*u=sg_state(p);if(!u)continue;
        size_t len=strlen(hud);
        if(!strcmp(id,"SGPDRONE")){snprintf(hud+len,sizeof hud-len,"%sDRONE: %s | D toggles mode",len?"\n":"",u->spent_frame?"DETONATING":u->suicide?"SUICIDE":"NORMAL");break;}
        if(!strcmp(id,"SGTANK")||!strcmp(id,"SGLINK")||!strcmp(id,"SGOP")){
            int cap=sg_capacity(p,u),alive=0;for(int j=0;j<5;j++)if(u->children[j]&&sg_alive(u->children[j])&&MEM32(u->children[j]+0x10)==u->child_uid[j])alive++;
            snprintf(hud+len,sizeof hud-len,"%s%s %d/%d DRONES%s",len?"\n":"",!strcmp(id,"SGTANK")?"BASTION":"CONTROLLER",alive,cap,!strcmp(id,"SGTANK")?" | D deploy/pack":"");
            for(int j=0;j<cap;j++)if(!u->children[j]&&u->ready[j]>f){len=strlen(hud);snprintf(hud+len,sizeof hud-len," [%d: %us]",j+1,(u->ready[j]-f+14)/15);}
            break;
        }
    }
    if(sg_ai_mode()){size_t len=strlen(hud);snprintf(hud+len,sizeof hud-len,"%sADAPTIVE COMMANDER | LOCAL / SCOUTING",len?"\n":"");}
    hle_screen_status(hud);
    (void)elapsed;
#endif
}

/* A native technology menu, separate from the construction sidebar. */
static hle_menu_item_t sg_menu[16];
static char sg_status_label[128],sg_progress_label[128],sg_requirement_label[128];
static void sg_menu_chosen(int id){
    if(!sg_on())return;SGHouse*s=sg_house(MEM32(0xA83D4C));if(!s)return;
    if(id==8101 && s->state==SG_READY){s->research_started=1;s->blackout_enabled=1;fprintf(stderr,"[singularity] research requested by player\n");}
    if(id==8102 && sg_active(s)){s->blackout_enabled=!s->blackout_enabled;fprintf(stderr,"[singularity] player toggled enemy grid disruption %d\n",s->blackout_enabled);}
}
int sg_tech_key(int scan,int down){
    if(sg_drone_command(scan,down))return 1;
    if(scan!=65 || !sg_on())return 0; /* SDL_SCANCODE_F8 */
    if(!down)return 1;
    SGHouse*s=sg_house(MEM32(0xA83D4C));int n=0;
    sg_menu[n++]=(hle_menu_item_t){"SINGULARITY | TECHNOLOGY",NULL,0,0,NULL,0};
    if(!s){sg_menu[n++]=(hle_menu_item_t){"Start a fresh skirmish to research technology",NULL,0,0,NULL,0};}
    else {
        snprintf(sg_status_label,sizeof sg_status_label,"State: %s",sg_states[s->state]);
        snprintf(sg_progress_label,sizeof sg_progress_label,"Countdown: %02u:%02u | %s",(unsigned)(((SG_RESEARCH_TICKS-s->progress+14)/15)+(unsigned)ceil((1-s->level)*SG_INTEGRATION_TICKS/15.))/60,(unsigned)(((SG_RESEARCH_TICKS-s->progress+14)/15)+(unsigned)ceil((1-s->level)*SG_INTEGRATION_TICKS/15.))%60,sg_states[s->state]);
        snprintf(sg_requirement_label,sizeof sg_requirement_label,"Prerequisite: %s | Power: %s",s->colossus?"Colossus online":s->nodes>=5?"5 compute nodes":"Colossus OR 5 compute nodes",s->powered?"available":"insufficient");
        sg_menu[n++]=(hle_menu_item_t){sg_status_label,NULL,0,0,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){sg_requirement_label,NULL,0,0,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){sg_progress_label,NULL,0,0,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){NULL,NULL,0,0,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){"Research Singularity", "6 min + 2 min integration",s->state==SG_READY?8101:0,s->research_started,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){"Disrupt enemy power + targeting", "Requires ACTIVE",sg_active(s)?8102:0,s->blackout_enabled,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){s->colossus?"Colossus defence: 6 interceptors":"Node defence: 1 interceptor per node", "Launch against nearby threats",0,0,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){"Destroy core / cut its power to stop effects",NULL,0,0,NULL,0};
        sg_menu[n++]=(hle_menu_item_t){"Reopen F8 to refresh | fresh skirmishes only",NULL,0,0,NULL,0};
    }
    if(sg_ai_mode())sg_menu[n++]=(hle_menu_item_t){"Adaptive Commander: local, scouting required", "No model or API connection",0,0,NULL,0};
    hle_screen_menu(sg_menu,n,sg_menu_chosen);return 1;
}
