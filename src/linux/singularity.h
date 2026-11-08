/* Local single-player Singularity extension for gamemd.exe 1.001 only.
 * Layouts cross-checked with YRpp and the lifted binary. Never runs for other mods.
 * Experimental progression state is session-local; see mod status documentation.
 */
#include <math.h>
#include <time.h>
#include "mods.h"
#ifndef SG_RESEARCH_TICKS
#define SG_RESEARCH_TICKS 5400
#endif
#ifndef SG_INTEGRATION_TICKS
#define SG_INTEGRATION_TICKS 1800
#endif
typedef struct { uint32_t uplink,uplink_uid,next_uplink; uint32_t ptr,progress,lastpulse; int nodes,colossus,batteries,powered,battery_output; double charge,level; float basecost[3]; int captured,research_started,blackout_enabled,cyber,state,stage,held,cost_steps; uint32_t sink,core_uid,outage_until,outage_by,next_outage,next_heist,next_cost,next_focus,next_hunt,next_cash; float cost_base[2],cost_written[2]; } SGHouse;
static SGHouse sg_houses[32];
static uint32_t sg_lastframe,sg_lasttick;
enum {SG_OFFLINE,SG_READY,SG_RESEARCHING,SG_INTEGRATING,SG_ACTIVE,SG_SUSPENDED};
static const char*sg_states[]={"OFFLINE","READY","RESEARCHING","INTEGRATING","ACTIVE","SUSPENDED"};
static int sg_active(SGHouse*s){return s&&s->state==SG_ACTIVE&&s->powered;}
static int sg_enemy(uint32_t a,uint32_t b){if(!a||!b||a==b)return 0;uint32_t type=MEM32(b+0x34);if(type>=0x10000&&MEM8(type+0x1A6))return 0;int ix=MEM32(b+0x30);return ix>=0&&ix<32&&!(MEM32(a+0x5788)&(1u<<ix));}

typedef struct {uint32_t ptr,uid,until,immune,pulse; double speedfactor; uint32_t spent_frame; uint32_t children[10],child_uid[10],ready[10],controller,controller_uid; float credited_rank; int suicide,nest_active,recalled,lasthp; uint32_t alert_until,shield_next; int32_t cable[3];
 int mode,own,dive,tethered,elite; uint32_t ord_target,ord_dest,last_launch,dkey_seen;
 uint32_t deploy_at,op_target,ring_next; int deploy_to,jj_base;
 uint32_t tent[2],tent_uid[2],uplink,uplink_uid,gap_born; int32_t gap_x,gap_y;} SGUnit; /* v10: drone stance/ownership */
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
    uint32_t vt=MEM32(p),t=MEM32(p+(building?0x520:((vt==0x007F5C70||vt==0x007E22A4)?0x6C4:0x6C0)));
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
static int sg_bastion(const char*id){return !strcmp(id,"SGTANK")||!strcmp(id,"SGTANKD")||!strcmp(id,"SGTANKC")||!strcmp(id,"SGTANKCD");}
static int sg_hive(const char*id){return !strcmp(id,"SGTANKC")||!strcmp(id,"SGTANKCD");}
/* v10: player-controllable drones: operator drones, Bastion fibre drones, factory strike drones. */
static int sg_opdrone(const char*id){return !strcmp(id,"SGPDRONE")||!strcmp(id,"SGFIBRE")||!strcmp(id,"SGFIBREC")||!strcmp(id,"SGFPV")||!strcmp(id,"SGFPVC");}
/* One-way attack drones: removed (with a blast) the frame after they fire. */
static int sg_kamikaze(const char*id){return !strcmp(id,"SGSHAHED")||!strcmp(id,"SGNEEDLE");}
/* EMP-proof: cable-guided or autonomous (no radio link to jam). */
static int sg_emp_proof(const char*id){return !strcmp(id,"SGFIBRE")||!strcmp(id,"SGFIBREC")||!strcmp(id,"SGFPV")||!strcmp(id,"SGFPVC");}
static int sg_drone(const char*id){return sg_opdrone(id)||sg_bastion(id)||!strcmp(id,"SGSCOUT")||!strcmp(id,"SGSERAPH")||!strcmp(id,"SGNEEDLE")||!strcmp(id,"SGLOCUST")||!strcmp(id,"SGINT")||!strcmp(id,"SGNDRONE")||!strcmp(id,"SGWDRONE")||!strcmp(id,"SGHOUND")||!strcmp(id,"SGGUARD")||!strcmp(id,"SGKAMI")||!strcmp(id,"SGFACTDRONE");}
double sg_costfactor(uint32_t h,uint32_t t){
    if(!sg_on())return 1;SGHouse*s=sg_house(h);if(!s||!s->powered||!t)return 1;
    const char*id=(const char*)hle_ptr(t+0x24);
    /* Only mobile types: buildings never inherit unit discounts. */
    uint32_t vt=MEM32(t);if(vt!=0x7F6218&&vt!=0x7EB610&&vt!=0x7E2868)return 1;
    if(s->colossus)return .9-.4*s->level;
    double nodes=fmin(s->nodes,3)/3.;return 1-.15*nodes-(sg_drone(id)?.5:0)*s->level*nodes;
}
double sg_buildfactor(uint32_t obj){
    if(!sg_on()||!obj)return 1;uint32_t vt=MEM32(obj);
    if(vt!=0x007F5C70&&vt!=0x007EB058&&vt!=0x007E22A4)return 1;
    uint32_t t=MEM32(obj+(vt==0x007F5C70?0x6C4:0x6C0)),h=MEM32(obj+0x21C);SGHouse*s=sg_house(h);
    if(!s||!s->powered)return 1;
    double cost=sg_costfactor(h,t),time=s->colossus?.9-.4*s->level:1-.2*fmin(s->nodes,3)/3.-(sg_drone((const char*)hle_ptr(t+0x24))?.4:0)*s->level*fmin(s->nodes,3)/3.;
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
 uint32_t t=sg_find_unit_type(sg_hive(sg_id(parent,0))?"SGFIBREC":sg_bastion(sg_id(parent,0))?"SGFIBRE":"SGPDRONE"),anchor=sg_controller_anchor(parent);if(!t||!anchor)return 0;
 uint32_t saved=g_ecx,owner=MEM32(parent+0x21C);
 g_ecx=t;uint32_t child=hle_call_guest(MEM32(MEM32(t)+0x8C),1,&owner);g_ecx=saved;
 if(!child)return 0;
 int32_t *coord=hle_heap_alloc(12,1);
 if(!coord){g_ecx=child;hle_call_guest(0x5F65F0,0,NULL);g_ecx=saved;return 0;}
 static const int offsets[8][2]={{1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,-1},{1,-1},{-1,1}};
 int fromdeck=sg_bastion(sg_id(parent,0));
 uint32_t args[2]={hle_va(coord),0},ok=0;
 for(int attempt=0;attempt<24&&!ok;attempt++){
  int dir=(slot+attempt)%8,radius=fromdeck&&attempt<4?0:1+attempt/8; /* off the launch deck first */
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
/* v09 drones: the operator is the only selectable unit. Its drones are
   runtime-owned, non-selectable units that attack whatever the operator is
   attacking, or the nearest enemy in range, like a GI firing; then return.
   (The engine's carrier launcher crashed on infantry/land carriers.) */
static void sg_ai_order(uint32_t,uint32_t,int);static void sg_ai_move(uint32_t,int,int);static uint32_t sg_ai_cell(int,int);static void sg_bastion_orders(void);static void sg_message(const char*);
/* v10: LINK keeps one drone (15 s relaunch); NEST two (three veteran);
   Bastion two on the move, five deployed, launched one at a time, 10 s relaunch. */
static int sg_capacity(uint32_t p,SGUnit*u){if(MEM32(p)==0x7E3EBC){const char*b=sg_id(p,1);return MEM32(RA2_GAME_FRAME_VA)<u->alert_until?(!strcmp(b,"SGCOLO")?6:2):0;}
 const char*id=sg_id(p,0);float rank=sg_rank(p);
 if(sg_bastion(id))return MEM8(p+0x6E0)?(sg_hive(id)?10:5):2; /* HIVE: ten light drones */ /* UnitClass::Deployed */if(!strcmp(id,"SGOP"))return rank>=1?3:2;return 1;}
static uint32_t sg_regen(uint32_t p){return sg_bastion(sg_id(p,0))?150:225;}
static int sg_core(const char*id){return !strcmp(id,"SGCOLO")||!strncmp(id,"SGNODE",6);}
/* LINK/NEST link range: half the map. MapClass::VisibleRect (+0xFC: X,Y,W,H;
   measured against the map's LocalSize) width+height spans the diamond in cells. */
static double sg_half_map(void){
 static int logged;int w=(int32_t)MEM32(0x87F7E8+0x104),h=(int32_t)MEM32(0x87F7E8+0x108);
 double half=(w>0&&h>0&&w<512&&h<512)?(w+h)/2.:40;if(half<15)half=15;if(half>120)half=120;
 if(!logged){logged=1;fprintf(stderr,"[drones] map rect %dx%d: link range %.0f cells\n",w,h,half);}
 return half;
}
static double sg_drone_range(const char*id){return sg_core(id)?10:sg_bastion(id)?14:sg_half_map();}
/* Drones guard their operator within this radius when not parked elsewhere. */
#define SG_GUARD_RADIUS 6.0
static int sg_targetable(uint32_t t){if(t<0x10000||t>0xEFFFFFFF)return 0;uint32_t vt=MEM32(t);return (vt==0x7F5C70||vt==0x7EB058||vt==0x7E22A4||vt==0x7E3EBC)&&sg_alive(t);}
static uint32_t sg_drone_target(uint32_t p,uint32_t anchor,double range){
 uint32_t house=MEM32(p+0x21C),t=MEM32(p+0x2B4);
 if(sg_targetable(t)&&sg_enemy(house,MEM32(t+0x21C))&&sg_dist(anchor,t)<=range+1)return t;
 return 0;
}
/* v10: nearest enemy unit around `at` (drones engage what is near them, not
   everything inside the long link range). Gun stance hunts infantry only;
   suicide stance hunts vehicles only; -1 = anything (data-centre defence). */
static uint32_t sg_near_enemy(uint32_t house,uint32_t at,double radius,int vehicles_only){
 uint32_t best=0;double bd=radius;
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){
  uint32_t q=sg_vec_at(0xA8EC78,i),vt=MEM32(q);if((vt!=0x7F5C70&&vt!=0x7EB058)||!sg_alive(q)||!sg_enemy(house,MEM32(q+0x21C)))continue;
  if(vehicles_only==1&&vt!=0x7F5C70)continue;
  if(vehicles_only==0&&vt!=0x7EB058)continue;
  if(sg_opdrone(sg_id(q,0)))continue;double d=sg_dist(at,q);if(d<=bd){bd=d;best=q;}
 }
 return best;
}
static int sg_ctrl_down;static uint32_t sg_dkey_frame;
static void sg_drone_cmd(uint32_t d,SGUnit*du,uint32_t target,int mission){sg_ai_order(d,target,mission);du->ord_target=target?target:MEM32(d+0x2B4);du->ord_dest=MEM32(d+0x5A4);} /* the engine may apply the target a frame later */
static void sg_drone_goto(uint32_t d,SGUnit*du,int x,int y){sg_ai_move(d,x,y);du->ord_target=MEM32(d+0x2B4);du->ord_dest=MEM32(d+0x5A4);}
static void sg_bastion_ring(uint32_t f);
static uint32_t sg_spawn(const char*,uint32_t,uint32_t,int,int,int);static void sg_remove(uint32_t);
/* v10: speed multiplier (TechnoClass +0x580) relative to the type's own speed. */
#define SG_DIVE_BOOST 2.5
static void sg_speed(uint32_t p,SGUnit*u,double factor){
 double old;memcpy(&old,hle_ptr(p+0x580),8);
 if(old>=0&&old<100){old=(u->speedfactor>0?old/u->speedfactor:1.)*factor;memcpy(hle_ptr(p+0x580),&old,8);u->speedfactor=factor;}
}
/* Native hook (patch_v10_native.py, DrawHealthBar): operator drones get the
   short infantry-size bar instead of the long vehicle bar. */
/* Native hook (patch_v10_native.py): the Cyber Warfare Unit may run a gap. */
int sg_gap_unit(uint32_t p){return sg_on()&&p>=0x10000&&p<0xF0000000&&MEM32(p)==0x7F5C70&&!strcmp(sg_id(p,0),"SGCYBER");}
int sg_small_bar(uint32_t p){
 if(!sg_on()||p<0x10000||p>0xEFFFFFFF||MEM32(p)!=0x7F5C70)return 0;
 return sg_opdrone(sg_id(p,0));
}
/* Native hook (SetTarget): rifle infantry got anti-air projectiles so they can
   shoot drones. They may NOT use that to hit anything else in the air
   (Rocketeers, aircraft); infantry that were anti-air in the original game keep it. */
static int32_t sg_floor(int32_t x,int32_t y){
 static int32_t*c;if(!c)c=hle_heap_alloc(12,1);if(!c)return 0;
 c[0]=x;c[1]=y;c[2]=0;uint32_t cc=hle_va(c),saved=g_ecx;g_ecx=0x87F7E8;int32_t z=(int32_t)hle_call_guest(0x578080,1,&cc);g_ecx=saved;return z;
}
int sg_aa_target_allowed(uint32_t src,uint32_t t){
 if(!sg_on()||src<0x10000||src>0xEFFFFFFF||t<0x10000||t>0xEFFFFFFF||MEM32(src)!=0x7EB058)return 1;
 uint32_t vt=MEM32(t);if(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E22A4)return 1;
 const char*tid=sg_id(t,0);if(vt==0x7F5C70&&(sg_drone(tid)||!strncmp(tid,"SGSWARM",7)))return 1;
 static const char*native_aa[]={"FLAKT","GGI","JUMPJET","CLEG",NULL};const char*sid=sg_id(src,0);
 for(int i=0;native_aa[i];i++)if(!strcmp(sid,native_aa[i]))return 1;
 int32_t up=(int32_t)MEM32(t+0xA4)-sg_floor((int32_t)MEM32(t+0x9C),(int32_t)MEM32(t+0xA0));
 return up<200; /* on the ground (landed aircraft, Rocketeer standing): normal target */
}
/* v10 right-click dive. The game uses right-click to deselect, so the click is
   caught as SDL delivers it (event watch, before the game sees it) and acted
   on next frame with the selection from the frame before the click. */
extern void SDL_AddEventWatch(int(*)(void*,void*),void*);
static volatile int sg_rclick;
static int sg_event_watch(void*ud,void*ev){(void)ud;const uint8_t*e=ev;uint32_t type;memcpy(&type,e,4);
 if(type==0x401&&e[16]==3)sg_rclick=1; /* SDL_MOUSEBUTTONDOWN, SDL_BUTTON_RIGHT */
 return 1;}
/* World position under the mouse: WWMouse (+0x1C/+0x20 = X/Y) through
   TacticalClass::ClientToCoords (thiscall: out CoordStruct*, Point2D*). */
static int sg_cursor_coords(int32_t*out){
 uint32_t mouse=MEM32(0x887640),tac=MEM32(0x887324);if(!mouse||!tac)return 0;
 int32_t*m=hle_heap_alloc(20,1);if(!m)return 0;
 m[0]=(int32_t)MEM32(mouse+0x1C);m[1]=(int32_t)MEM32(mouse+0x20);m[2]=m[3]=m[4]=0;
 uint32_t args[2]={hle_va(m+2),hle_va(m)},saved=g_ecx;g_ecx=tac;hle_call_guest(0x6D2280,2,args);g_ecx=saved;
 out[0]=m[2];out[1]=m[3];out[2]=m[4];int ok=m[2]||m[3];hle_heap_free(m);return ok;
}
static uint32_t sg_enemy_at(uint32_t house,const int32_t*c){
 uint32_t best=0;double bd=1e9;
 for(int v=0;v<2;v++){uint32_t vec=v?0xA8EB40:0xA8EC78;
  for(int i=0;i<sg_vec_count(vec);i++){uint32_t q=sg_vec_at(vec,i);if(!sg_targetable(q)||!sg_enemy(house,MEM32(q+0x21C)))continue;
   double dx=((int32_t)MEM32(q+0x9C)-c[0])/256.,dy=((int32_t)MEM32(q+0xA0)-c[1])/256.,d=sqrt(dx*dx+dy*dy);
   if(d<=(v?2.5:1.5)&&d<bd){bd=d;best=q;}}}
 return best;
}
static void sg_dive(uint32_t d,SGUnit*du,uint32_t t){
 du->own=1;du->dive=1;du->suicide=1;sg_drone_cmd(d,du,t,1);sg_speed(d,du,SG_DIVE_BOOST);
}
/* v10: an attack order given to a selected operator (LINK, NEST, Bastion) is
   handed straight to all of its drones. The gun cannot hurt vehicles or
   buildings, so against those (or with Ctrl held) the drones dive. */
static void sg_send_drones(uint32_t d,SGUnit*du,uint32_t dt,const char*id){
 uint32_t player=MEM32(0xA83D4C);
 if(!sg_targetable(dt)||!sg_enemy(player,MEM32(dt+0x21C))||dt==du->op_target)return;
 du->op_target=dt;int n=0;
 int hard=sg_ctrl_down||MEM32(dt)==0x7F5C70||MEM32(dt)==0x7E3EBC;
 for(int j=0;j<10;j++){uint32_t c=du->children[j];if(!c||MEM32(c+0x10)!=du->child_uid[j]||!sg_alive(c))continue;SGUnit*cu=sg_state(c);if(!cu)continue;
  if(hard)sg_dive(c,cu,dt);else{cu->own=1;cu->dive=0;sg_drone_cmd(c,cu,dt,1);}n++;}
 fprintf(stderr,"[drones] %s %08X attack order: %d drones on %08X%s, %.1f cells\n",id,d,n,dt,hard?" (dive)":" (gun)",sg_dist(d,dt));
 if(n&&sg_dist(d,dt)>sg_drone_range(id)+1)sg_message("Target is outside drone range.");
 if(sg_bastion(id)&&(MEM8(d+0x6E0)||du->deploy_at))sg_ai_order(d,0,5); /* deployed: it stays put, its drones go */
}
static uint32_t sg_prev_sel[32];static int sg_nprev_sel;
static void sg_drone_input(uint32_t f){
#ifndef SG_STATE_TEST
 static int watching;if(!watching){watching=1;SDL_AddEventWatch(sg_event_watch,NULL);}
 uint32_t player=MEM32(0xA83D4C);
 /* Right-click on an enemy: every drone that was selected (and every drone of
    a selected Bastion) dives on it. */
 if(sg_rclick){sg_rclick=0;int32_t c[3];uint32_t t=0;
  if(sg_cursor_coords(c))t=sg_enemy_at(player,c);
  int n=0;
  for(int k=0;t&&k<sg_nprev_sel;k++){uint32_t q=sg_prev_sel[k];if(!sg_alive(q)||MEM32(q+0x21C)!=player)continue;
   if(sg_opdrone(sg_id(q,0))){SGUnit*du=sg_state(q);if(du){sg_dive(q,du,t);n++;}}
   else {SGUnit*u=sg_state(q);if(!u)continue;for(int j=0;j<10;j++){uint32_t d=u->children[j];if(d&&MEM32(d+0x10)==u->child_uid[j]&&sg_alive(d)){SGUnit*du=sg_state(d);if(du){sg_dive(d,du,t);n++;}}}}
  }
  fprintf(stderr,"[drones] right-click at (%d,%d) target=%08X: %d drones dive\n",c[0]/256,c[1]/256,t,n);
  if(n)sg_message(n>1?"Drones diving.":"Drone diving.");
 }
 int nsel=0;
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){
  uint32_t d=sg_vec_at(0xA8EC78,i);if(MEM32(d)!=0x7F5C70||!sg_alive(d)||MEM32(d+0x21C)!=player)continue;
  const char*id=sg_id(d,0);int drone=sg_opdrone(id),bas=sg_bastion(id);if(!drone&&!bas)continue;
  SGUnit*du=sg_state(d);if(!du)continue;
  if(MEM8(d+0x83)&&nsel<32)sg_prev_sel[nsel++]=d;
  uint32_t dt=MEM32(d+0x2B4),dd=MEM32(d+0x5A4),m=MEM32(d+0xAC);
  if(bas){
   if(MEM8(d+0x83)&&m==1)sg_send_drones(d,du,dt,id);
   if(!sg_targetable(dt))du->op_target=0;
   continue;
  }
  /* A selected drone whose target/destination changed to something the runtime
     did not set was ordered by the player. Ctrl held (force-fire) = dive. */
  if(MEM8(d+0x83)&&dt&&dt!=du->ord_target){ /* new attack target */
   du->own=1;du->dive=sg_ctrl_down&&sg_targetable(dt);du->ord_target=dt;du->ord_dest=dd;
   if(du->dive){sg_dive(d,du,dt);fprintf(stderr,"[drones] drone %08X ordered to DIVE on %08X\n",d,dt);}
  }
  else if(dd&&dd!=du->ord_dest){ /* new destination: the player's move order, or the engine chasing the target */
   if(MEM8(d+0x83)&&m==2&&!dt){du->own=1;du->dive=0;}
   du->ord_dest=dd;
  }
  if(du->dive&&!sg_targetable(MEM32(d+0x2B4)))du->dive=0;
 }
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){
  uint32_t o=sg_vec_at(0xA8EC78,i);if(MEM32(o)!=0x7EB058||!sg_alive(o)||MEM32(o+0x21C)!=player)continue;
  const char*id=sg_id(o,0);if(strcmp(id,"SGLINK")&&strcmp(id,"SGOP"))continue;
  SGUnit*ou=sg_state(o);if(!ou)continue;uint32_t ot=MEM32(o+0x2B4);
  if(MEM8(o+0x83)&&MEM32(o+0xAC)==1)sg_send_drones(o,ou,ot,id);
  if(!sg_targetable(ot))ou->op_target=0;
  if(MEM8(o+0x83)&&nsel<32)sg_prev_sel[nsel++]=o;
 }
 sg_nprev_sel=nsel;
 /* Flight speed: drones fly on the jumpjet locomotor, which ignores the
    general speed multiplier; its own Speed (+0x1C, = JumpjetSpeed) is scaled:
    x2.5 while diving, near zero while EMP-suppressed. */
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){
  uint32_t d=sg_vec_at(0xA8EC78,i);if(MEM32(d)!=0x7F5C70||!sg_alive(d)||!sg_opdrone(sg_id(d,0)))continue;
  uint32_t L=MEM32(d+0x674);if(!L||MEM32(L)!=0x7ECD68)continue;SGUnit*du=sg_state(d);if(!du)continue;
  if(!du->jj_base){int b=(int)MEM32(L+0x1C);if(b<=0||b>200)continue;du->jj_base=b;}
  int diving=du->dive||(du->suicide&&MEM32(d+0xAC)==1&&sg_targetable(MEM32(d+0x2B4)));
  double factor=(f<du->until?.05:1.)*(diving?SG_DIVE_BOOST:1.);int want=(int)(du->jj_base*factor+.5);if(want<1)want=1;
  if((int)MEM32(L+0x1C)!=want)MEM32(L+0x1C)=(uint32_t)want;
 }
 sg_bastion_ring(f);
#else
 (void)f;
#endif
}
/* One-off explosion animation at an object's position (AnimClass, 0x1C8 bytes). */
static void sg_explode(uint32_t at,const char*anim){
#ifndef SG_STATE_TEST
 uint32_t type=0;for(int i=0;i<sg_vec_count(0x8B4150);i++){uint32_t t=sg_vec_at(0x8B4150,i);if(!strcasecmp((const char*)hle_ptr(t+0x24),anim)){type=t;break;}}
 if(!type)return;
 uint32_t size=0x1C8,mem=hle_call_guest(0x7C8E17,1,&size);if(!mem)return;
 int32_t*c=hle_heap_alloc(12,1);if(!c)return;c[0]=(int32_t)MEM32(at+0x9C);c[1]=(int32_t)MEM32(at+0xA0);c[2]=(int32_t)MEM32(at+0xA4);
 uint32_t args[7]={type,hle_va(c),0,1,0x600,0,0},saved=g_ecx;g_ecx=mem;hle_call_guest(0x421EA0,7,args);g_ecx=saved;hle_heap_free(c);
#else
 (void)at;(void)anim;
#endif
}
/* Fibre-optic cable: each Bastion drone pays out a thin grey line behind it,
   drawn with the engine's laser lines (LaserDrawClass, 0x5C bytes) that fade
   over about six seconds, leaving a short-lived web over the battlefield. */
static void sg_cable(int32_t*from,uint32_t d){
#ifndef SG_STATE_TEST
 int32_t x=(int32_t)MEM32(d+0x9C),y=(int32_t)MEM32(d+0xA0),z=(int32_t)MEM32(d+0xA4)-40;
 double dx=x-from[0],dy=y-from[1];if(dx*dx+dy*dy<96.*96.)return;
 uint32_t size=0x5C,mem=hle_call_guest(0x7C8E17,1,&size);if(!mem)return;
 float one=1.f,zero=0.f;uint32_t fo,fz;memcpy(&fo,&one,4);memcpy(&fz,&zero,4);
 uint32_t args[16]={(uint32_t)from[0],(uint32_t)from[1],(uint32_t)from[2],(uint32_t)x,(uint32_t)y,(uint32_t)z,0,1,
  0xC8C4BEu,0x202020u,0,90,0,1,fo,fz};
 uint32_t saved=g_ecx;g_ecx=mem;hle_call_guest(0x54FE60,16,args);g_ecx=saved;
 from[0]=x;from[1]=y;from[2]=z;
#else
 (void)from;(void)d;
#endif
}
/* v10: range ring for a selected Bastion: a faint dotted circle at drone range
   (14 cells), like the psychic radar's circle but thinner and dimmer. Drawn
   with short engine laser lines that are refreshed while it stays selected. */
static void sg_bastion_ring(uint32_t f){
#ifndef SG_STATE_TEST
 uint32_t player=MEM32(0xA83D4C);
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){
  uint32_t p=sg_vec_at(0xA8EC78,i);if(MEM32(p)!=0x7F5C70||!sg_alive(p)||MEM32(p+0x21C)!=player||!MEM8(p+0x83)||!sg_bastion(sg_id(p,0)))continue;
  SGUnit*u=sg_state(p);if(!u||f<u->ring_next)continue;u->ring_next=f+28;
  double r=sg_drone_range(sg_id(p,0))*256.;int32_t cx=(int32_t)MEM32(p+0x9C),cy=(int32_t)MEM32(p+0xA0);
  const int N=48;float one=1.f,zero=0.f;uint32_t fo,fz;memcpy(&fo,&one,4);memcpy(&fz,&zero,4);
  for(int k=0;k<N;k+=2){ /* every other segment: dotted, less intrusive */
   double a0=2*M_PI*k/N,a1=2*M_PI*(k+1)/N;
   int32_t x0=cx+(int32_t)(r*cos(a0)),y0=cy+(int32_t)(r*sin(a0)),x1=cx+(int32_t)(r*cos(a1)),y1=cy+(int32_t)(r*sin(a1));
   int32_t z0=sg_floor(x0,y0)+16,z1=sg_floor(x1,y1)+16;
   uint32_t size=0x5C,mem=hle_call_guest(0x7C8E17,1,&size);if(!mem)return;
   uint32_t args[16]={(uint32_t)x0,(uint32_t)y0,(uint32_t)z0,(uint32_t)x1,(uint32_t)y1,(uint32_t)z1,0,1,
    0x909050u,0x000000u,0,30,0,0,fo,fo};
   uint32_t saved=g_ecx;g_ecx=mem;hle_call_guest(0x54FE60,16,args);g_ecx=saved;
  }
 }
#else
 (void)f;
#endif
}
static void sg_control_tick(uint32_t f){
#ifndef SG_STATE_TEST
 /* Spent kamikaze drones, and drones whose operator is gone, are removed a frame later. */
 for(int i=sg_vec_count(0xA8EC78)-1;i>=0;i--){
  uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p)||MEM32(p)!=0x7F5C70||(!sg_opdrone(sg_id(p,0))&&!sg_kamikaze(sg_id(p,0))))continue;
  SGUnit*u=sg_state(p);if(!u)continue;
  if(!u->spent_frame&&u->controller&&(!sg_controller_anchor(u->controller)||MEM32(u->controller+0x10)!=u->controller_uid))u->spent_frame=f+1;
  if(u->spent_frame&&f>=u->spent_frame){if(u->suicide&&!u->recalled)sg_explode(p,"S_BANG34");uint32_t saved=g_ecx;g_ecx=p;hle_call_guest(0x5F65F0,0,NULL);g_ecx=saved;}
 }
#endif
 int n=sg_vec_count(0xA8EC78),nb=sg_vec_count(0xA8EB40);
 for(int i=0;i<n+nb;i++){
  uint32_t p=i<n?sg_vec_at(0xA8EC78,i):sg_vec_at(0xA8EB40,i-n),anchor=sg_controller_anchor(p);if(!anchor)continue;
  uint32_t vt=MEM32(p);int building=vt==0x7E3EBC;if(i<n&&vt!=0x7F5C70&&vt!=0x7EB058)continue;
  const char*id=sg_id(p,building);if(building?!sg_core(id):(strcmp(id,"SGLINK")&&strcmp(id,"SGOP")&&!sg_bastion(id)))continue;
  SGUnit*u=sg_state(p);if(!u||f<u->until)continue;
  if(building){ /* data centre under attack: force shield (20 s, 90 s cooldown) and defence drones for 30 s */
   int hp=(int32_t)MEM32(p+0x6C);
   if(u->lasthp&&hp<u->lasthp){u->alert_until=f+450;
    if(f>=u->shield_next){uint32_t args[3]={300,MEM32(p+0x21C),1},saved=g_ecx;g_ecx=p;hle_call_guest(MEM32(MEM32(p)+0x154),3,args);g_ecx=saved;u->shield_next=f+1350;
     fprintf(stderr,"[singularity] %s under attack: shield up, drones out\n",id);if(MEM32(p+0x21C)==MEM32(0xA83D4C))sg_message("Data centre under attack: shield raised, defence drones launched.");}}
   u->lasthp=hp;
  }
  int capacity=sg_capacity(p,u);uint32_t regen=building?150:sg_regen(p);
  double range=sg_drone_range(id);uint32_t target=building?0:sg_drone_target(p,anchor,range);int tank=sg_bastion(id);
  /* v10: a new move order to the operator calls its un-parked drones along. */
  uint32_t opm=MEM32(p+0xAC);int opmoving=!building&&(opm==2||opm==3);
  uint32_t player=MEM32(0xA83D4C);
  for(int j=0;j<10;j++){
   uint32_t d=u->children[j];int alive=d&&MEM32(d+0x10)==u->child_uid[j]&&sg_alive(d);
   if(alive){
    SGUnit*du=sg_state(d);if(!du)continue;
    float rank=sg_rank(d);if(rank>du->credited_rank){float parentrank=sg_rank(p)+(rank-du->credited_rank)*150.f/700.f;memcpy(hle_ptr(p+0x150),&parentrank,4);du->credited_rank=rank;}
    if(building)du->mode=1; /* data-centre defence drones always dive */
    du->tethered=tank;
    if(j>=capacity){du->spent_frame=f+1;du->recalled=1;continue;} /* packed up: extra drones return to the racks */
    if(tank){if(!du->cable[0]&&!du->cable[1]){du->cable[0]=(int32_t)MEM32(anchor+0x9C);du->cable[1]=(int32_t)MEM32(anchor+0xA0);du->cable[2]=(int32_t)MEM32(anchor+0xA4)+60;}sg_cable(du->cable,d);}
    if(du->dive&&!sg_targetable(MEM32(d+0x2B4)))du->dive=0;
    du->suicide=du->mode||du->dive;
    double fromop=sg_dist(d,anchor);
    if(fromop>range+2){ /* out of link range: fly back */
     if(du->own||fromop>range+3){du->own=0;du->dive=0;}
     int x=(int32_t)MEM32(anchor+0x9C)+256*((j%3)-1),y=(int32_t)MEM32(anchor+0xA0)+256*((j/3%3)-1); /* leptons, as sg_ai_cell expects */sg_drone_goto(d,du,x,y);continue;
    }
    if(du->own){
     /* Parked/loitering where the player sent it: engage what comes near. */
     uint32_t m=MEM32(d+0xAC);int idle=m==5||m==0||m==11||m==13;
     if(idle&&!sg_targetable(MEM32(d+0x2B4))){uint32_t near=sg_near_enemy(MEM32(d+0x21C),d,5,du->mode);if(near)sg_drone_cmd(d,du,near,1);}
     continue;
    }
    uint32_t want=target;
    if(!want){uint32_t cur=MEM32(d+0x2B4);if(sg_targetable(cur)&&sg_enemy(MEM32(d+0x21C),MEM32(cur+0x21C))&&sg_dist(anchor,cur)<=SG_GUARD_RADIUS+1)want=cur;}
    if(!want&&!opmoving)want=sg_near_enemy(MEM32(d+0x21C),anchor,building?10:SG_GUARD_RADIUS,building?-1:du->mode);
    if(want){if(MEM32(d+0x2B4)!=want)sg_drone_cmd(d,du,want,1);}
    else if(fromop>2.5||(opmoving&&fromop>1.5)){int x=(int32_t)MEM32(anchor+0x9C)+256*((j%3)-1),y=(int32_t)MEM32(anchor+0xA0)+256*((j/3%3)-1); /* leptons, as sg_ai_cell expects */
     uint32_t cell=sg_ai_cell(x,y);if(cell&&MEM32(d+0x5A4)!=cell)sg_drone_goto(d,du,x,y);}
    continue;
   }
   if(d){u->children[j]=0;u->ready[j]=f+regen;if(MEM32(p+0x21C)==player&&!strcmp(id,"SGLINK"))fprintf(stderr,"[drones] LINK drone lost: relaunch in %u s\n",regen/15);}
   if(j>=capacity)continue;
   if(!u->ready[j])u->ready[j]=f+15;
   if(f>=u->ready[j]){uint32_t child=sg_make_drone(p,j);if(child){u->children[j]=child;u->child_uid[j]=MEM32(child+0x10);u->ready[j]=0;u->last_launch=f;
     SGUnit*cu=sg_state(child);if(cu){cu->ord_target=MEM32(child+0x2B4);cu->ord_dest=MEM32(child+0x5A4);cu->mode=building?1:0;cu->tethered=tank;}
     fprintf(stderr,"[drones] %s slot %d launched drone %08X\n",id,j+1,child);}else u->ready[j]=f+30;}
  }
 }
}
void sg_drone_fired(uint32_t p,int weapon){
 if(!sg_on()||!p)return;
 /* B-2 release: a large blast on the target, guaranteed visible even when the
    warhead's own animation is culled. */
 if(MEM32(p)==0x7E22A4&&!strcmp(sg_id(p,0),"SGB2")){uint32_t t=MEM32(p+0x2B4);if(sg_targetable(t)){sg_explode(t,"TWLT070");sg_explode(t,"EXPLOLRG");fprintf(stderr,"[b2] bomb released on %s\n",sg_id(t,MEM32(t)==0x7E3EBC));}return;}
 if(MEM32(p)==0x7F5C70&&sg_kamikaze(sg_id(p,0))){SGUnit*k=sg_state(p);if(k&&!k->spent_frame){k->spent_frame=MEM32(RA2_GAME_FRAME_VA)+1;k->suicide=1;}return;}
 if(weapon!=1||MEM32(p)!=0x7F5C70||!sg_opdrone(sg_id(p,0)))return;
 SGUnit*u=sg_state(p);if(u&&!u->spent_frame)u->spent_frame=MEM32(RA2_GAME_FRAME_VA)+1;
}
uint32_t sg_credit_source(uint32_t killer){
 if(!sg_on()||!killer||MEM32(killer)!=0x7F5C70||!sg_opdrone(sg_id(killer,0)))return killer;
 SGUnit*u=sg_state(killer);if(u&&u->controller&&sg_controller_anchor(u->controller)&&MEM32(u->controller+0x10)==u->controller_uid&&MEM32(u->controller+0x21C)==MEM32(killer+0x21C))return u->controller;
 return killer;
}
int sg_weapon_mode(uint32_t p){
 if(!sg_on()||!p||MEM32(p)!=0x7F5C70||!sg_opdrone(sg_id(p,0)))return -1;
 SGUnit*u=sg_state(p);if(!u)return 0;
 /* Ctrl held over a target with a selected drone: show/use the dive weapon. */
 if(sg_ctrl_down&&MEM8(p+0x83)&&MEM32(p+0x21C)==MEM32(0xA83D4C))return 1;
 return (u->mode||u->dive)?1:0;
}

/* v09: Singularity research is two native superweapon cards (shield tab) with
   the nuke-style on-screen countdown. The runtime grants and advances them; they
   are never fired by clicking. SuperClass layout per YRpp:
   +0x28 Type, +0x2C Owner, +0x30 timer start, +0x38 time left, +0x6D present, +0x6F ready. */
static uint32_t sg_super(uint32_t house,const char*id){
 for(int i=0;i<sg_vec_count(0xA83CB8);i++){
  uint32_t s=sg_vec_at(0xA83CB8,i),t=MEM32(s+0x28);
  if(MEM32(s+0x2C)==house&&t>0x10000&&!strcmp((const char*)hle_ptr(t+0x24),id))return s;
 }
 return 0;
}
/* Position in the SuperWeaponTypes list: the sidebar identifies cards by it. */
static uint32_t sg_sw_index(uint32_t type){for(int i=0;i<sg_vec_count(0xA8E330);i++)if(sg_vec_at(0xA8E330,i)==type)return (uint32_t)i;return 0;}
static int sg_super_left(uint32_t s){
 int start=(int)MEM32(s+0x30),left=(int)MEM32(s+0x38);
 if(start!=-1)left-=(int)(MEM32(RA2_GAME_FRAME_VA)-(uint32_t)start);
 return left>0?left:0;
}
static int sg_super_done(uint32_t s){return s&&MEM8(s+0x6D)&&(MEM8(s+0x6F)||sg_super_left(s)<=15);}
#ifndef SG_STATE_TEST
static void sg_super_grant(uint32_t s,uint32_t house){
 if(!s||MEM8(s+0x6D))return;
 uint32_t saved=g_ecx,args[3]={1,1,0};g_ecx=s;uint32_t ok=hle_call_guest(0x6CB560,3,args)&0xFF;
 /* A one-time grant arrives fully charged; restart its countdown from zero. */
 const char*fast=getenv("SG_FAST_RESEARCH"); /* lab only: frames per card */
 if(ok){g_ecx=s;hle_call_guest(0x6CE0B0,0,NULL);}
 if(ok&&fast&&atoi(fast)>0){MEM32(s+0x30)=MEM32(RA2_GAME_FRAME_VA);MEM32(s+0x38)=(uint32_t)atoi(fast);}
 if(getenv("SG_DEBUG_SUPER")){fprintf(stderr,"[sgdebug] grant %s ok=%u frame=%u:",(char*)hle_ptr(MEM32(s+0x28)+0x24),ok,MEM32(RA2_GAME_FRAME_VA));for(int o=0x24;o<0x80;o+=4)fprintf(stderr," %02X=%08X",o,MEM32(s+o));fprintf(stderr,"\n");}
 if(ok&&house==MEM32(0xA83D4C)){uint32_t cam[2]={31,sg_sw_index(MEM32(s+0x28))};g_ecx=0x87F7E8;hle_call_guest(0x6A6300,2,cam);}
 g_ecx=saved;
}
/* Adds the card to the shield tab; the sidebar ignores a card it already shows. */
static void sg_super_show(uint32_t s){if(!s||!MEM8(s+0x6D))return;uint32_t cam[2]={31,sg_sw_index(MEM32(s+0x28))},saved=g_ecx;g_ecx=0x87F7E8;hle_call_guest(0x6A6300,2,cam);g_ecx=saved;}
static void sg_super_lose(uint32_t s){if(!s||!MEM8(s+0x6D))return;uint32_t saved=g_ecx;g_ecx=s;hle_call_guest(0x6CB7B0,0,NULL);g_ecx=saved;}
static void sg_super_hold(uint32_t s,int hold){if(!s||!MEM8(s+0x6D))return;uint32_t saved=g_ecx,arg=hold?1:0;g_ecx=s;hle_call_guest(0x6CB4D0,1,&arg);g_ecx=saved;}
static void sg_message(const char*text){
 size_t n=strlen(text);uint16_t*w=hle_heap_alloc((n+1)*2,1);if(!w)return;
 for(size_t i=0;i<=n;i++)w[i]=(uint8_t)text[i];
 uint32_t saved=g_ecx,args[7]={0,0,hle_va(w),(uint32_t)-1,0x4046,450,0};g_ecx=0xA8BC60;hle_call_guest(0x5D3BA0,7,args);g_ecx=saved;
 /* The message list keeps a reference for its lifetime: deliberately not freed. */
}
#else
static void sg_super_grant(uint32_t s,uint32_t house){(void)house;if(s)MEM8(s+0x6D)=1;}
static void sg_super_lose(uint32_t s){if(s)MEM8(s+0x6D)=0;}
static void sg_super_show(uint32_t s){(void)s;}
static void sg_super_hold(uint32_t s,int hold){(void)s;(void)hold;}
static void sg_message(const char*text){(void)text;}
#endif
/* Card pictures: the engine leaves SidebarImage files in the mod folder
   unloaded, so the runtime loads them (FileSystem::LoadFile as SHP) and sets
   SuperWeaponType +0xB8 SidebarImage; +0xCC holds the file name (measured). */
static void sg_card_images(void){
#ifndef SG_STATE_TEST
 static uint32_t fixed[2];
 static const char*ids[2]={"SGRESEARCH","SGSINGULAR"};
 for(int k=0;k<2;k++){
  uint32_t sw=0;
  for(int i=0;i<sg_vec_count(0xA8E330);i++){uint32_t t=sg_vec_at(0xA8E330,i);if(!strcmp((const char*)hle_ptr(t+0x24),ids[k]))sw=t;}
  if(!sw||fixed[k]==sw)continue;
  char name[40];snprintf(name,sizeof name,"%.24s.shp",(const char*)hle_ptr(sw+0xCC));
  char*g=hle_heap_alloc(sizeof name,1);if(!g)continue;memcpy(g,name,sizeof name);
  uint32_t sc=g_ecx,sd=g_edx;g_ecx=hle_va(g);g_edx=1;uint32_t shp=hle_call_guest(0x5B40B0,0,NULL);g_ecx=sc;g_edx=sd;
  if(shp>0x10000){MEM32(sw+0xB8)=shp;fixed[k]=sw;fprintf(stderr,"[singularity] card %s picture %s\n",ids[k],name);}
 }
#endif
}
/* Singularity achieved: every unit of that house gains a rank; one in three
   goes straight to elite. Deterministic per unit, never the engine RNG. */
static int sg_promote(uint32_t house){
 int promoted=0;
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){
  uint32_t p=sg_vec_at(0xA8EC78,i),vt=MEM32(p);if(!sg_alive(p)||MEM32(p+0x21C)!=house)continue;
  if(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E22A4)continue;
  float v=sg_rank(p),nv=1.f; /* v10: one star (veteran) */
  if(nv>v){memcpy(hle_ptr(p+0x150),&nv,4);promoted++;}
 }
 return promoted;
}
/* B-2 bomb: Construction Yards and War Factories survive badly damaged. */
void sg_building_damage(uint32_t b,uint32_t pdamage,uint32_t wh){
 if(!sg_on()||!b||!pdamage||wh<0x10000||strcmp((const char*)hle_ptr(wh+0x24),"SGB2BOMBWH"))return;
 const char*id=sg_id(b,1);if(!strstr(id,"CNST")&&!strstr(id,"WEAP")&&strcmp(id,"SGFACT"))return;
 int hp=(int32_t)MEM32(b+0x6C),allow=hp-250,dmg=(int32_t)MEM32(pdamage);if(allow<0)allow=0;
 if(dmg>allow){MEM32(pdamage)=(uint32_t)allow;fprintf(stderr,"[b2] %s survives: damage %d clamped to %d\n",id,dmg,allow);}
}

#include "adaptive_ai.h"
#ifdef SG_V07_LAB
#include "@WORKSPACE@/work/singularity/lab_v07.h"
#endif
#ifdef SG_DRONE_LAB
#include "@WORKSPACE@/work/singularity/drone_lab_v06.h"
#endif
/* v09 Singularity effects (owner ACTIVE): random enemy grid outages of 10 s to
   2 min with an on-screen countdown, vehicle/aircraft prices down 10% every
   3 min (to 40%), and cyber heists of up to $1,000 roughly every 1.5 min.
   Original-AI houses get a cash bonus, periodic hunting waves, and after an
   enemy Singularity they converge on its data centre. 15 frames = 1 s. */
static uint32_t sg_seed=0x9E3779B9u;static int sg_seeded;
static uint32_t sg_rand(uint32_t n){sg_seed^=sg_seed<<13;sg_seed^=sg_seed>>17;sg_seed^=sg_seed<<5;return n?sg_seed%n:0;}
static void sg_money(uint32_t house,int amount){uint32_t saved=g_ecx,a=(uint32_t)(amount<0?-amount:amount);g_ecx=house;hle_call_guest(amount<0?0x4F9790:0x4F9950,1,&a);g_ecx=saved;}
static SGHouse*sg_pick_enemy(SGHouse*a,int hn){
 SGHouse*c[32];int k=0;for(int j=0;j<hn;j++){SGHouse*t=&sg_houses[j];uint32_t ty=MEM32(t->ptr+0x34);if(t!=a&&sg_enemy(a->ptr,t->ptr)&&ty>0x10000&&strcmp((const char*)hle_ptr(ty+0x24),"Neutral")&&strcmp((const char*)hle_ptr(ty+0x24),"Special"))c[k++]=t;}
 return k?c[sg_rand(k)]:NULL;
}
static int sg_ai_controlled(uint32_t h){uint32_t ty=MEM32(h+0x34);return h!=MEM32(0xA83D4C)&&ty>0x10000&&!MEM8(ty+0x1A6)&&strcmp((const char*)hle_ptr(ty+0x24),"Neutral")&&strcmp((const char*)hle_ptr(ty+0x24),"Special")&&!sg_ai_mode();}
static int sg_civilian_role(const char*id){if(sg_opdrone(id))return 1;static const char*skip[]={"HARV","CMIN","SMIN","AMCV","SMCV","PCV","ENGINEER","SENGINEER","YENGINEER","SPY","SLAV","SGPDRONE","DRON","LCRF","SAPC","ZEP",NULL};for(int i=0;skip[i];i++)if(!strcmp(id,skip[i]))return 1;return 0;}
static void sg_outage_timer(uint32_t house,int frames,int on){
 uint32_t sw=sg_super(house,"SGOUTAGE");if(!sw)return;
 if(!on){sg_super_lose(sw);return;}
#ifndef SG_STATE_TEST
 uint32_t saved=g_ecx,args[3]={1,0,0};if(!MEM8(sw+0x6D)){g_ecx=sw;hle_call_guest(0x6CB560,3,args);g_ecx=saved;}
 MEM8(sw+0x6F)=0;MEM32(sw+0x30)=MEM32(RA2_GAME_FRAME_VA);MEM32(sw+0x38)=(uint32_t)frames;
#endif
}
static void sg_effects(uint32_t f,int hn){
 if(!sg_seeded){sg_seed=((uint32_t)time(NULL)*2654435761u)|1u;sg_seeded=1;} /* different events every match */
 uint32_t player=MEM32(0xA83D4C);char msg[160];
 for(int i=0;i<hn;i++){
  SGHouse*a=&sg_houses[i];
  if(sg_active(a)){
   if(!a->next_outage)a->next_outage=f+300+sg_rand(600);
   if(f>=a->next_outage){SGHouse*t=sg_pick_enemy(a,hn);uint32_t dur=150+sg_rand(1651);a->next_outage=f+dur+600+sg_rand(900);
    if(t&&f>=t->outage_until){t->outage_until=f+dur;t->outage_by=a->ptr;sg_outage_timer(t->ptr,(int)dur,1);
     fprintf(stderr,"[singularity] outage house=%08X for %u s\n",t->ptr,dur/15);
     if(a->ptr==player){snprintf(msg,sizeof msg,"Enemy power grid hacked: offline for %u seconds.",dur/15);sg_message(msg);}
     if(t->ptr==player){snprintf(msg,sizeof msg,"WARNING: our power grid is hacked - offline for %u seconds.",dur/15);sg_message(msg);}}}
   if(!a->next_heist)a->next_heist=f+900+sg_rand(900);
   if(f>=a->next_heist){SGHouse*t=sg_pick_enemy(a,hn);a->next_heist=f+900+sg_rand(900);
    if(t){int bal=(int32_t)MEM32(t->ptr+0x30C),amt=200+(int)sg_rand(801);if(amt>bal)amt=bal;
     if(amt>0){sg_money(t->ptr,-amt);sg_money(a->ptr,amt);fprintf(stderr,"[singularity] heist $%d from %08X\n",amt,t->ptr);
      if(a->ptr==player){snprintf(msg,sizeof msg,"Cyber heist: $%d transferred from enemy accounts.",amt);sg_message(msg);}
      if(t->ptr==player){snprintf(msg,sizeof msg,"WARNING: $%d stolen by enemy hackers.",amt);sg_message(msg);}}}}
   if(!a->next_cost)a->next_cost=f+2700;
   if(f>=a->next_cost&&a->cost_steps<6){a->cost_steps++;a->next_cost=f+2700;
    fprintf(stderr,"[singularity] house=%08X vehicles/aircraft now %d%% cost\n",a->ptr,100-10*a->cost_steps);
    if(a->ptr==player){snprintf(msg,sizeof msg,"Automated factories: vehicles and aircraft now cost %d%%.",100-10*a->cost_steps);sg_message(msg);}}
  }
  /* Price multipliers (CostUnitsMult +0x5394, CostAircraftMult +0x5398). The
     engine recomputes them when buildings change; detect that and re-apply. */
  for(int k=0;k<2;k++){
   float cur;memcpy(&cur,hle_ptr(a->ptr+0x5394+4*k),4);
   if(!a->cost_written[k]||fabsf(cur-a->cost_written[k])>1e-4f)a->cost_base[k]=cur;
   float want=a->cost_base[k]*(1.f-0.1f*(float)a->cost_steps);
   if(fabsf(cur-want)>1e-4f){memcpy(hle_ptr(a->ptr+0x5394+4*k),&want,4);}
   a->cost_written[k]=want;
  }
  /* Grid outage ends. */
  int cyber=f<a->outage_until;
  if(!cyber&&a->outage_until){a->outage_until=0;sg_outage_timer(a->ptr,0,0);if(a->ptr==player)sg_message("Power grid restored.");}
  if(a->cyber!=cyber){a->cyber=cyber;MEM8(a->ptr+0x5778)=1;fprintf(stderr,"[singularity] cyber house=%08X %s\n",a->ptr,cyber?"OUTAGE + FIRE-CONTROL FAULTS":"RESTORED");}
 }
 /* Original AI: income bonus, hunting waves, Singularity focus. */
 for(int i=0;i<hn;i++){
  SGHouse*h=&sg_houses[i];if(!sg_ai_controlled(h->ptr))continue;
  SGHouse*core_owner=NULL;for(int j=0;j<hn;j++)if(sg_active(&sg_houses[j])&&sg_enemy(h->ptr,sg_houses[j].ptr))core_owner=&sg_houses[j];
  if(!h->next_cash)h->next_cash=f+900;
  if(f>=h->next_cash){h->next_cash=f+900;sg_money(h->ptr,core_owner?600:300);}
  uint32_t core=0;
  if(core_owner)for(int b=0;b<sg_vec_count(0xA8EB40);b++){uint32_t q=sg_vec_at(0xA8EB40,b);if(sg_alive(q)&&MEM32(q+0x21C)==core_owner->ptr&&sg_core(sg_id(q,1))){core=q;if(!strcmp(sg_id(q,1),"SGCOLO"))break;}}
  int focus=core&&f>=h->next_focus,hunt=!h->next_hunt?(h->next_hunt=f+1800+sg_rand(900),0):f>=h->next_hunt;
  if(!focus&&!hunt)continue;
  if(focus)h->next_focus=f+300;
  if(hunt)h->next_hunt=f+1800+sg_rand(900);
  int sent=0;
  for(int k=0;k<sg_vec_count(0xA8EC78);k++){
   uint32_t q=sg_vec_at(0xA8EC78,k),vt=MEM32(q);if((vt!=0x7F5C70&&vt!=0x7EB058)||!sg_alive(q)||MEM32(q+0x21C)!=h->ptr||sg_civilian_role(sg_id(q,0)))continue;
   if(focus){if(MEM32(q+0x2B4)!=core){sg_ai_order(q,core,1);sent++;}}
   else{uint32_t m=MEM32(q+0xAC);if((m==5||m==11||m==0||m==13)&&sg_rand(10)<7){sg_ai_order(q,0,15);sent++;}}
  }
  if(sent)fprintf(stderr,"[singularity] AI house=%08X %s: %d units\n",h->ptr,focus?"converging on data centre":"hunting wave",sent);
 }
}
void sg_tick(void) {
#ifdef RA2_TARGET_GAMEMD
    if(!sg_on()){hle_screen_status("");return;}
    uint32_t f=MEM32(RA2_GAME_FRAME_VA);
    if(f==sg_lastframe || !f)return;
    if(f<sg_lastframe){memset(sg_houses,0,sizeof sg_houses);memset(sg_units,0,sizeof sg_units);memset(sg_ai_brains,0,sizeof sg_ai_brains);sg_lasttick=0;sg_seeded=0;}
    sg_lastframe=f;
    if(f<60)sg_card_images();
    sg_ai_sanitize();
    sg_bastion_orders();
    sg_drone_input(f);
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
        if(!strcmp(id,"SGCOLO")){s->colossus++;uint32_t uid=MEM32(b+0x10);s->core_uid=uid;}
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
        /* Card 1 "Singularity Research" (6 min) -> card 2 "Singularity" (2 min)
           -> ACTIVE. Low power pauses the cards natively; losing the
           prerequisite puts them on hold; losing every core resets. */
        int eligible=s->colossus||s->nodes>=3; /* v10: China needs three $3,000 nodes */
        uint32_t rs=sg_super(h,"SGRESEARCH"),ss=sg_super(h,"SGSINGULAR"),card=s->stage==1?rs:s->stage==2?ss:0;
        if(!s->colossus&&!s->nodes){sg_super_lose(rs);sg_super_lose(ss);s->stage=0;s->held=0;s->level=0;s->blackout_enabled=0;}
        else if(!eligible){if(card&&!s->held){sg_super_hold(card,1);s->held=1;}}
        else {
            if(s->held){sg_super_hold(card,0);s->held=0;}
            if(s->stage==0&&rs){sg_super_grant(rs,h);s->stage=1;}
            else if(s->stage==1&&!MEM8(rs+0x6D))sg_super_grant(rs,h);
            else if(s->stage==1&&sg_super_done(rs)){sg_super_lose(rs);sg_super_grant(ss,h);s->stage=2;}
            else if(s->stage==2&&!MEM8(ss+0x6D))sg_super_grant(ss,h);
            else if(s->stage==2&&sg_super_done(ss)){
                sg_super_lose(ss);s->stage=3;s->level=1;s->blackout_enabled=1;MEM8(h+0x5778)=1;
                int promoted=sg_promote(h);
                fprintf(stderr,"[singularity] ACHIEVED house=%08X promoted=%d\n",h,promoted);
                sg_message(h==MEM32(0xA83D4C)?"SINGULARITY ACHIEVED - all units veteran, strike drones unlocked. Enemy grid and targeting disrupted.":"WARNING: Enemy Singularity achieved.");
            }
        }
        if(h==MEM32(0xA83D4C)&&(s->stage==1||s->stage==2)){ /* keep the card on the sidebar */
            sg_super_show(s->stage==1?rs:ss);
        }
        s->research_started=s->stage>0;s->progress=s->stage>=2?SG_RESEARCH_TICKS:0;
        int previous=s->state;
        if(!s->colossus&&!s->nodes)s->state=SG_OFFLINE;
        else if(!powered||!eligible)s->state=SG_SUSPENDED;
        else if(s->stage<=1)s->state=s->stage?SG_RESEARCHING:SG_READY;
        else if(s->stage==2)s->state=SG_INTEGRATING;
        else s->state=SG_ACTIVE;
        if(previous!=s->state)fprintf(stderr,"[singularity] state house=%08X %s -> %s\n",h,sg_states[previous],sg_states[s->state]);
        if(f%450==0 && (s->colossus || s->nodes))fprintf(stderr,"[singularity] house=%08X nodes=%d colossus=%d powered=%d research=%u ticks\n",h,s->nodes,s->colossus,powered,s->progress);
    }
    sg_effects(f,hn);
    /* v10: while a house's Singularity is active, all its units are veteran (1 star). */
    for(int i=0;i<sg_vec_count(0xA8EC78);i++){
        uint32_t p=sg_vec_at(0xA8EC78,i),vt=MEM32(p);if(!sg_alive(p)||(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E22A4))continue;
        SGHouse*h=sg_house(MEM32(p+0x21C));if(!sg_active(h))continue;
        if(sg_rank(p)<1.f){float e=1.f;memcpy(hle_ptr(p+0x150),&e,4);} /* one star */
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
    /* v10: AEGIS EMP Tank - automatic pulse when enemy radio drones or vehicles
       are within 6 cells: drones stall 6 s, vehicles 3 s; 30 s recharge.
       Fibre-optic and strike drones are EMP-proof. */
    for(int i=0;i<sg_vec_count(0xA8EC78);i++){
        uint32_t e=sg_vec_at(0xA8EC78,i);if(MEM32(e)!=0x7F5C70||!sg_alive(e)||strcmp(sg_id(e,0),"SGEMPT"))continue;
        SGUnit*em=sg_state(e);if(!em||f<em->pulse)continue;uint32_t eh=MEM32(e+0x21C);
        int drones=0,vehicles=0,mine=0;
        for(int j=0;j<sg_vec_count(0xA8EC78);j++){
            uint32_t q=sg_vec_at(0xA8EC78,j),vt=MEM32(q);if(!sg_alive(q)||(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E22A4)||!sg_enemy(eh,MEM32(q+0x21C)))continue;
            if(sg_dist(e,q)>6)continue;const char*qid=sg_id(q,0);SGUnit*qu=sg_state(q);if(!qu)continue;
            if(sg_drone(qid)&&!sg_bastion(qid)){if(sg_emp_proof(qid)||qu->tethered||f<qu->immune)continue;qu->until=f+90;qu->immune=f+240;drones++;}
            else if(vt==0x7F5C70){qu->until=f+45;vehicles++;}
            else continue;
            if(MEM32(q+0x21C)==MEM32(0xA83D4C))mine++;
        }
        if(drones||vehicles){em->pulse=f+450;sg_explode(e,"EMP_FX01");
            fprintf(stderr,"[singularity] EMP tank %08X pulse: %d drones, %d vehicles\n",e,drones,vehicles);
            if(eh==MEM32(0xA83D4C))sg_message("EMP pulse: enemy drones and vehicles stalled.");else if(mine)sg_message("Enemy EMP pulse: your drones and vehicles are stalled.");}
    }
    /* v10: Starlink (building) and Cyber Warfare Unit (vehicle) use the
       engine's gap generator (TechnoClass +0x269 gap on, +0x26C radius;
       CreateGap 0x6FB170, DestroyGap 0x6FB470). The radius grows with time,
       so the enemy map darkens slowly; the Cyber unit's gap follows it. */
    for(int v=0;v<2;v++){uint32_t vec=v?0xA8EC78:0xA8EB40;
        for(int i=0;i<sg_vec_count(vec);i++){
            uint32_t g=sg_vec_at(vec,i);if(!sg_alive(g))continue;
            int cyber=!v?0:(MEM32(g)==0x7F5C70&&!strcmp(sg_id(g,0),"SGCYBER"));
            if(!v&&strcmp(sg_id(g,1),"SGSTARLINK"))continue;if(v&&!cyber)continue;
            SGUnit*gu=sg_state(g);if(!gu)continue;if(!gu->gap_born)gu->gap_born=f;
            uint32_t age=f-gu->gap_born;
            int want=cyber?5+(int)(age/300)*2:8+(int)(age/600)*3;if(want>(cyber?20:36))want=cyber?20:36;
            int32_t x=(int32_t)MEM32(g+0x9C),y=(int32_t)MEM32(g+0xA0);int on=MEM8(g+0x269);
            int moved=cyber&&on&&(abs(x-gu->gap_x)>256||abs(y-gu->gap_y)>256);
            if(!cyber&&!on)continue; /* building unpowered: the engine keeps it off */
            if(on&&(int)MEM32(g+0x26C)==want&&!moved)continue;
            uint32_t saved=g_ecx;
            if(on){if(cyber){MEM32(g+0x9C)=(uint32_t)gu->gap_x;MEM32(g+0xA0)=(uint32_t)gu->gap_y;}
                g_ecx=g;hle_call_guest(0x6FB470,0,NULL);MEM32(g+0x9C)=(uint32_t)x;MEM32(g+0xA0)=(uint32_t)y;}
            MEM32(g+0x26C)=(uint32_t)want;g_ecx=g;hle_call_guest(0x6FB170,0,NULL);g_ecx=saved;
            gu->gap_x=x;gu->gap_y=y;
            if(!moved)fprintf(stderr,"[singularity] %s %08X shroud radius %d cells (gap %s)\n",cyber?"Cyber unit":"Starlink",g,want,MEM8(g+0x269)?"on":"OFF");
        }
    }
    /* v10: B-2 limit (2 per side). The engine lets queued aircraft exceed
       BuildLimit; an extra bomber that comes out is scrapped and refunded. */
    for(int i=0;i<hn;i++){
        uint32_t h=sg_vec_at(0xA80228,i),newest=0;int n=0;
        for(int j=0;j<sg_vec_count(0xA8EC78);j++){uint32_t q=sg_vec_at(0xA8EC78,j);if(MEM32(q)!=0x7E22A4||!sg_alive(q)||MEM32(q+0x21C)!=h||strcmp(sg_id(q,0),"SGB2"))continue;n++;if(!newest||MEM32(q+0x10)>MEM32(newest+0x10))newest=q;}
        if(n>2&&newest){sg_remove(newest);sg_money(h,2500);fprintf(stderr,"[b2] house %08X had %d B-2s: extra one refunded\n",h,n);if(h==MEM32(0xA83D4C))sg_message("B-2 limit is 2: the extra bomber was refunded.");}
    }
    /* v10: Singularity uplink - hidden prerequisite building that unlocks the
       factory strike drones while the Singularity is active. */
    for(int i=0;i<hn;i++){
        SGHouse*s=&sg_houses[i];uint32_t h=s->ptr;int on=s->state==SG_ACTIVE;
        int have=s->uplink&&MEM32(s->uplink+0x10)==s->uplink_uid&&sg_alive(s->uplink);
        if(on&&!have){uint32_t core=0;for(int j=0;j<bn&&!core;j++){uint32_t b=sg_vec_at(0xA8EB40,j);if(sg_alive(b)&&MEM32(b+0x21C)==h&&sg_core(sg_id(b,1)))core=b;}
            if(core&&f>=s->next_uplink){uint32_t u=sg_spawn("SGUPLINK",0xA83C68,h,(int32_t)MEM32(core+0x9C)/256+3,(int32_t)MEM32(core+0xA0)/256+3,21);s->uplink=u;s->uplink_uid=u?MEM32(u+0x10):0;s->next_uplink=f+150;
            fprintf(stderr,"[singularity] house %08X uplink %s: strike drones %s\n",h,u?"placed":"FAILED",u?"unlocked":"locked");}}
        if(!on&&have){sg_remove(s->uplink);s->uplink=0;fprintf(stderr,"[singularity] house %08X uplink removed: strike drones locked\n",h);}
    }
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
            if(sg_dist(b,p)>(us?6:3))continue;SGUnit*u=sg_state(p);if(!u||f<u->immune||u->tethered||sg_emp_proof(pid))continue; /* fibre-optic / autonomous: no radio link to jam */
            u->until=f+90;u->immune=f+240;hit++;
        }
        if(hit){em->pulse=f+(us?900:675);fprintf(stderr,"[singularity] %s EMP suppressed %d targets for 90 simulation frames\n",id,hit);}
    }
    for(int i=0;i<tn;i++){
        uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p))continue;uint32_t vt=MEM32(p);
        if(vt!=0x007F5C70&&vt!=0x007EB058&&vt!=0x007E22A4)continue;
        SGUnit*u=sg_state(p);SGHouse*h=sg_house(MEM32(p+0x21C));if(!u)continue;
        double factor=(f<u->until)?.01:1.;
        if(u->dive||(u->suicide&&MEM32(p+0xAC)==1&&sg_targetable(MEM32(p+0x2B4))))factor*=SG_DIVE_BOOST; /* v10: dives accelerate */
        if(h&&h->powered&&h->level>0&&(h->colossus||sg_drone(sg_id(p,0))))factor*=1.+.1*h->level*(h->colossus?1:fmin(h->nodes,3)/3.);
        sg_speed(p,u,factor);
    }
    /* v09: no custom status text; Singularity uses sidebar cards and the
       native countdown. Only the opt-in Adaptive Commander profile notes itself. */
    char hud[96]="";
    if(sg_ai_mode())snprintf(hud,sizeof hud,"ADAPTIVE COMMANDER | LOCAL / SCOUTING");
    hle_screen_status(hud);
    (void)elapsed;
#endif
}

/* v09: F8 menu retired; keys pass through to the game. Bastion's drone nest
   follows the game's own deploy order (D key or deploy cursor): that order sets
   CurrentMission (+0xAC) to Unload (16); the runtime turns it into the
   simple-deploy state (UnitClass::Deployed, +0x6E0, mast-up model) instead of
   unloading the passengers, who stay aboard and keep firing. */
/* Place a new object of a rules type next to cell (x,y) (first free spot of a
   small spiral), owned by house. vector: 0xA83C68 buildings, 0xA83CE0 units. */
static uint32_t sg_spawn(const char*id,uint32_t vector,uint32_t house,int x,int y,int tries){
 uint32_t type=0;for(int i=0;i<sg_vec_count(vector);i++){uint32_t t=sg_vec_at(vector,i);if(t>0x10000&&!strcmp((char*)hle_ptr(t+0x24),id)){type=t;break;}}
 if(!type)return 0;
 uint32_t saved=g_ecx;g_ecx=type;uint32_t p=hle_call_guest(MEM32(MEM32(type)+0x8C),1,&house);g_ecx=saved;if(!p)return 0;
 static const int sp[][2]={{0,0},{1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1},{2,0},{0,2},{-2,0},{0,-2},{2,1},{-2,1},{2,-1},{-2,-1},{1,2},{-1,2},{1,-2},{-1,-2}};
 int32_t*c=hle_heap_alloc(12,1);if(!c){g_ecx=p;hle_call_guest(0x5F65F0,0,NULL);g_ecx=saved;return 0;}
 uint32_t args[2]={hle_va(c),0},ok=0;
 for(int k=0;k<tries&&k<(int)(sizeof sp/sizeof sp[0])&&!ok;k++){
  c[0]=(x+sp[k][0])*256+128;c[1]=(y+sp[k][1])*256+128;c[2]=0;
  uint32_t cc=hle_va(c);g_ecx=0x87F7E8;c[2]=(int32_t)hle_call_guest(0x578080,1,&cc);
  g_ecx=p;ok=hle_call_guest(MEM32(MEM32(p)+0xD8),2,args);g_ecx=saved;
 }
 hle_heap_free(c);
 if(!ok){g_ecx=p;hle_call_guest(0x5F65F0,0,NULL);g_ecx=saved;return 0;}
 return p;
}
static void sg_remove(uint32_t p){uint32_t saved=g_ecx;g_ecx=p;hle_call_guest(0x5F65F0,0,NULL);g_ecx=saved;}
/* Drone camp: two garrisonable Army Tents (3 infantry each) beside a deployed
   Bastion. On pack-up empty tents fold away; occupied ones stay until emptied. */
static void sg_camp(uint32_t p,SGUnit*u,int up){
 int cx=(int32_t)MEM32(p+0x9C)/256,cy=(int32_t)MEM32(p+0xA0)/256;
 static const int off[2][2]={{-5,-1},{3,-1}};
 for(int k=0;k<2;k++){
  uint32_t t=u->tent[k];int alive=t&&MEM32(t+0x10)==u->tent_uid[k]&&sg_alive(t);
  if(up&&!alive){t=sg_spawn("SGTENT",0xA83C68,MEM32(p+0x21C),cx+off[k][0],cy+off[k][1],21);u->tent[k]=t;u->tent_uid[k]=t?MEM32(t+0x10):0;
   fprintf(stderr,"[drones] Bastion %08X camp tent %d %s\n",p,k+1,t?"up":"FAILED (no room)");}
  if(!up&&alive&&!sg_vec_count(t+0x684)){sg_remove(t);u->tent[k]=0;}
  if(!up&&!alive)u->tent[k]=0;
 }
}
/* v10: deploying or packing up takes 10 s (150 frames), immobile; the
   deployed Bastion becomes type SGTANKD (mast-up model, sgbastiond.vxl), so it
   visibly changes, and cannot drive until packed up again. */
static void sg_bastion_toggle(uint32_t p){
 SGUnit*u=sg_state(p);if(!u||u->deploy_at)return;
 uint32_t f=MEM32(RA2_GAME_FRAME_VA);u->deploy_to=!MEM8(p+0x6E0);u->deploy_at=f+150;
 sg_ai_order(p,0,5); /* Guard: stops it, and cancels the unload */
 if(MEM32(p+0x21C)==MEM32(0xA83D4C))sg_message(u->deploy_to?"Bastion deploying: 10 seconds, then five drones.":"Bastion packing up: 10 seconds.");
 fprintf(stderr,"[drones] Bastion %08X %s (10 s)\n",p,u->deploy_to?"DEPLOYING":"PACKING UP");
}
static void sg_bastion_finish(uint32_t p,SGUnit*u){
 int hive=sg_hive(sg_id(p,0));uint32_t t=sg_find_unit_type(u->deploy_to?(hive?"SGTANKCD":"SGTANKD"):(hive?"SGTANKC":"SGTANK"));
 if(t)MEM32(p+0x6C4)=t; /* UnitClass::Type */
 MEM8(p+0x6E0)=(uint8_t)u->deploy_to;MEM8(p+0x6E1)=0;MEM8(p+0x6E2)=0;u->deploy_at=0;
 sg_ai_order(p,0,5);
 sg_camp(p,u,u->deploy_to);
 if(MEM32(p+0x21C)==MEM32(0xA83D4C))sg_message(u->deploy_to?(hive?"HIVE deployed: ten drones launching, drone camp up (2 tents, 3 infantry each).":"Bastion deployed: five drones launching, drone camp up (2 tents, 3 infantry each)."):(hive?"HIVE mobile.":"Bastion mobile."));
 fprintf(stderr,"[drones] Bastion %08X %s type=%s\n",p,u->deploy_to?"DEPLOYED: 5 drones":"MOBILE: 2 drones",sg_id(p,0));
}
static void sg_bastion_orders(void){
 uint32_t f=MEM32(RA2_GAME_FRAME_VA),player=MEM32(0xA83D4C);
 int fallback=sg_dkey_frame&&f>=sg_dkey_frame+4;
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){
  uint32_t p=sg_vec_at(0xA8EC78,i);
  if(MEM32(p)!=0x7F5C70||!sg_alive(p)||!sg_bastion(sg_id(p,0)))continue;
  SGUnit*u=sg_state(p);if(!u)continue;
  if(!MEM8(p+0x6E0)&&!u->deploy_at&&(u->tent[0]||u->tent[1])&&f%15==0)sg_camp(p,u,0);
  if(u->deploy_at){ /* mid-transition: stays put */
   if(f>=u->deploy_at)sg_bastion_finish(p,u);
   else {uint32_t m=MEM32(p+0xAC);if(m==2||m==1||m==16)sg_ai_order(p,0,5);}
   continue;
  }
  if(MEM8(p+0x6E0)&&MEM32(p+0xAC)==2){sg_ai_order(p,0,5);if(MEM32(p+0x21C)==player)sg_message("Bastion is deployed: press D to pack up (10 s).");}
  if(MEM32(p+0xAC)==16){sg_bastion_toggle(p);u->pulse=f;continue;}
  /* D pressed but the game issued no deploy order: toggle selected Bastions directly. */
  if(fallback&&MEM8(p+0x83)&&MEM32(p+0x21C)==player&&u->pulse<sg_dkey_frame){sg_bastion_toggle(p);u->pulse=f;}
 }
 /* v10: D on selected operator drones switches gun / suicide stance. */
 if(fallback){int n=0,mode=0;
  for(int i=0;i<sg_vec_count(0xA8EC78);i++){
   uint32_t p=sg_vec_at(0xA8EC78,i);
   if(MEM32(p)!=0x7F5C70||!sg_alive(p)||!MEM8(p+0x83)||MEM32(p+0x21C)!=player||!sg_opdrone(sg_id(p,0)))continue;
   SGUnit*u=sg_state(p);if(!u||u->dkey_seen==sg_dkey_frame)continue;
   u->dkey_seen=sg_dkey_frame;u->mode=!u->mode;mode=u->mode;n++;
  }
  if(n){sg_message(mode?"Drone stance: SUICIDE - dives on vehicles it finds.":"Drone stance: GUN - machine gun, returns after a dive.");fprintf(stderr,"[drones] %d drones stance %s\n",n,mode?"SUICIDE":"GUN");}
 }
 if(fallback)sg_dkey_frame=0;
}
int sg_tech_key(int scan,int down){
 if(scan==7&&down&&sg_on())sg_dkey_frame=MEM32(RA2_GAME_FRAME_VA); /* D; never swallowed */
 if(scan==224||scan==228)sg_ctrl_down=down; /* left/right Ctrl: force-fire = drone dive */
 return 0;
}
