/* Brutal Plus: the game's own scripted AI (teams/scripts/triggers authored in
 * the profile's aimd.ini) plus this runtime layer for what aimd.ini cannot do:
 *   - one coordinated army: free units gather at a rally point and launch
 *     together once the army is big enough, then retreat as a group;
 *   - base defence: free units engage anything that enters the base;
 *   - Singularity buildings (Colossus, compute nodes, EMP/jammer arrays,
 *     drone factory) and second factories appended to the native base plan;
 *   - income bonus.
 * Brutal Plus is deliberately NOT fair: like the classic Brutal AI it reads
 * the whole map. The Adaptive Commander's vision gates do not apply here.
 * Pool teams in aimd.ini end their script at home; their members (Team==0, or
 * a team type whose ID starts "BPP") are what this coordinator commands. */
#include "adaptive_bp_data.h"
#define SG_BP_TEAM 0x5D4 /* FootClass::Team (YRpp) */
typedef struct {uint32_t house,last,next_cash,next_build,state_since,launch_frame;int state,launch_value,waves,rx,ry,sx,sy;uint32_t target,target_house;uint32_t wave[96];int wn;uint32_t last_reflex;unsigned picks;int built;int r_hits,r_help,r_retreat,r_focus;struct{uint32_t ptr,uid;int hp;uint32_t acted;}mem[384];} SGBPBrain;
enum {SG_BP_GATHER,SG_BP_MARCH,SG_BP_ASSAULT};
static SGBPBrain sg_bp_brains[32];
static int sg_bp_issuing;
static int sg_civilian_role(const char*id);
static void sg_money(uint32_t house,int amount);
static int sg_bp_mode(void){return !strcmp(mods_active(),"Singularity - Brutal Plus")&&MEM32(0xA8B238)==5;}
/* The easiest difficulty slot is "Classic Brutal": the original AI, kept for
 * comparison matches, so it is not a Brutal Plus house. */
int sg_bp_house(uint32_t h){return sg_bp_mode()&&h>=0x10000&&h!=MEM32(0xA83D4C)&&!MEM8(h+0x1EC)&&MEM32(h+0x34)>0x10000&&!MEM8(MEM32(h+0x34)+0x1A6)&&(int32_t)MEM32(h+0x184)!=2;}
static int sg_bp_find(const char*id){int lo=0,hi=sizeof sg_bp_costs/sizeof sg_bp_costs[0];while(lo<hi){int mid=(lo+hi)/2,c=strcmp(id,sg_bp_costs[mid].id);if(!c)return mid;if(c<0)hi=mid;else lo=mid+1;}return -1;}
static int sg_bp_cost(const char*id){int i=sg_bp_find(id);return i<0?500:sg_bp_costs[i].cost;}
static int sg_bp_strength(const char*id){int i=sg_bp_find(id);return i<0||sg_bp_costs[i].strength<=0?300:sg_bp_costs[i].strength;}
static SGBPBrain*sg_bp_brain(uint32_t h){int i=MEM32(h+0x30);if(i<0||i>=32)return NULL;SGBPBrain*b=&sg_bp_brains[i];if(b->house!=h){memset(b,0,sizeof *b);b->house=h;}return b;}
static int sg_bp_is_foot(uint32_t p){uint32_t vt=MEM32(p);return vt==0x7F5C70||vt==0x7EB058;}
/* Units the coordinator may command: ground combat units not in a scripted
 * (scout / raid / defence) team. */
static int sg_bp_free(uint32_t p,uint32_t h){
 if(!sg_alive(p)||MEM32(p+0x21C)!=h||!sg_bp_is_foot(p))return 0;
 const char*id=sg_ai_id(p);if(sg_civilian_role(id)||!strcmp(id,"SMIN")||!strcmp(id,"SGPDRONE")||!strcmp(id,"SGNEEDLE"))return 0;
 uint32_t team=MEM32(p+SG_BP_TEAM);
 if(team>=0x10000){uint32_t tt=MEM32(team+0x24);if(tt<0x10000||strncmp((const char*)hle_ptr(tt+0x24),"BPP",3))return 0;}
 return 1;
}
static int sg_bp_in_wave(SGBPBrain*b,uint32_t p){for(int i=0;i<b->wn;i++)if(b->wave[i]==p)return 1;return 0;}
static void sg_bp_order(uint32_t p,uint32_t target,int mission){sg_bp_issuing++;sg_ai_order(p,target,mission);sg_bp_issuing--;}
static void sg_bp_move(uint32_t p,int x,int y){sg_bp_issuing++;sg_ai_move(p,x,y);sg_bp_issuing--;}
/* The original "hunting wave" in singularity.h sends idle units out a few at
 * a time; for Brutal Plus houses that drip-feed is refused here. */
static int sg_bp_refuse(uint32_t p,uint32_t target,int mission){return mission==15&&!target&&!sg_bp_issuing&&sg_bp_house(MEM32(p+0x21C));}
static int sg_bp_value(SGBPBrain*b,int*count){int v=0,n=0;for(int i=0;i<b->wn;i++){uint32_t p=b->wave[i];if(sg_alive(p)&&MEM32(p+0x21C)==b->house){v+=sg_bp_cost(sg_ai_id(p));n++;}}if(count)*count=n;return v;}
#include "adaptive_ai_llm.h"
/* Target: the enemy house's Singularity core if it has one, otherwise its
 * nearest production or economy building to our rally point. */
static uint32_t sg_bp_pick_target(SGBPBrain*b){
 static const char*prio[]={"SGCOLO","SGNODE","GAWEAP","NAWEAP","YAWEAP","GAREFN","NAREFN","YAREFN","GACNST","NACNST","YACNST",NULL};
 uint32_t best=0;double bs=1e30;
 for(int i=0;i<sg_vec_count(0xA8EB40);i++){
  uint32_t q=sg_vec_at(0xA8EB40,i);if(!sg_alive(q)||!sg_enemy(b->house,MEM32(q+0x21C)))continue;
  const char*id=sg_ai_id(q);int rank=-1;for(int k=0;prio[k];k++)if(!strncmp(id,prio[k],strlen(prio[k]))){rank=k<2?0:k<5?1:k<8?2:3;break;}
  rank=sg_llm_rank(b->house,id,rank); /* LLM commander's target choice, if any */
  double dx=(int32_t)MEM32(q+0x9C)-b->rx,dy=(int32_t)MEM32(q+0xA0)-b->ry,s=sqrt(dx*dx+dy*dy)/256.+(rank<0?400:rank*25);
  if(s<bs){bs=s;best=q;}
 }
 if(sg_llm_hunt_army(b->house))best=0;
 if(!best)for(int i=0;i<sg_vec_count(0xA8EC78);i++){ /* no buildings yet: hunt its units (e.g. an MCV) */
  uint32_t q=sg_vec_at(0xA8EC78,i);if(!sg_alive(q)||!sg_bp_is_foot(q)||!sg_enemy(b->house,MEM32(q+0x21C)))continue;
  double dx=(int32_t)MEM32(q+0x9C)-b->rx,dy=(int32_t)MEM32(q+0xA0)-b->ry,s=sqrt(dx*dx+dy*dy);if(s<bs){bs=s;best=q;}
 }
 return best;
}
static void sg_bp_cash(SGBPBrain*b,uint32_t f){
 if(!b->next_cash){b->next_cash=f+900;return;}if(f<b->next_cash)return;b->next_cash=f+900;
 /* Only the top "Brutal Plus" slot gets extra money; "Veteran" plays on the
    same economy as you (apart from the game's own AI money setting). */
 if((int32_t)MEM32(b->house+0x184)!=0)return;
 int bonus=300,refineries=0,singularity=0;
 for(int i=0;i<sg_vec_count(0xA8EB40);i++){uint32_t q=sg_vec_at(0xA8EB40,i);if(!sg_alive(q))continue;const char*id=sg_ai_id(q);if(MEM32(q+0x21C)==b->house&&strstr(id,"REFN"))refineries++;}
 for(int i=0;i<32;i++)if(sg_houses[i].ptr&&sg_enemy(b->house,sg_houses[i].ptr)&&sg_houses[i].stage>0)singularity=1;
 if(singularity)bonus=600;
 bonus+=75*(refineries>4?4:refineries); /* ~12% on top of normal ore income */
 sg_money(b->house,bonus);
 fprintf(stderr,"[brutalplus] house=%d income bonus $%d (refineries=%d enemy singularity=%d)\n",MEM32(b->house+0x30),bonus,refineries,singularity);
}
/* Defence: free units near home engage enemies inside the base. Returns the
 * threat value seen in the base. */
static int sg_bp_defend(SGBPBrain*b){
 int threat=0;uint32_t intr[24];int ni=0;
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){
  uint32_t p=sg_vec_at(0xA8EC78,i);uint32_t vt=MEM32(p);if(!sg_alive(p)||!sg_enemy(b->house,MEM32(p+0x21C)))continue;
  if(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E22A4)continue;
  double dx=(int32_t)MEM32(p+0x9C)-b->sx,dy=(int32_t)MEM32(p+0xA0)-b->sy;if(dx*dx+dy*dy>14.*14*256*256)continue;
  threat+=sg_bp_cost(sg_ai_id(p));if(ni<24)intr[ni++]=p;
 }
 if(!ni)return 0;
 int sent=0,budget=threat*2+1000;
 for(int i=0;i<sg_vec_count(0xA8EC78)&&budget>0;i++){
  uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_bp_free(p,b->house)||(b->state!=SG_BP_GATHER&&sg_bp_in_wave(b,p)))continue;
  double dx=(int32_t)MEM32(p+0x9C)-b->sx,dy=(int32_t)MEM32(p+0xA0)-b->sy;if(dx*dx+dy*dy>22.*22*256*256)continue;
  uint32_t best=0;double bd=1e30;for(int k=0;k<ni;k++){double d=sg_dist(p,intr[k]);if(d<bd){bd=d;best=intr[k];}}
  if(best&&MEM32(p+0x2B4)!=best){sg_bp_order(p,best,1);sent++;}budget-=sg_bp_cost(sg_ai_id(p));
 }
 if(sent)fprintf(stderr,"[brutalplus] house=%d base defence: threat $%d, %d units engaging\n",MEM32(b->house+0x30),threat,sent);
 return threat;
}
/* ---- Reflexes (every second): units react to what happens around them. ----
 * Hit -> hit back, and idle neighbours join in. Miner or building hit -> the
 * nearest free units go after the attacker. Badly hurt -> fall back home.
 * In an assault, focus the weakest enemy unit in reach; anti-air goes for
 * aircraft first. */
static int sg_bp_is_aa(const char*id){return !strcmp(id,"HTK")||!strcmp(id,"FV")||!strcmp(id,"FLAKT")||!strcmp(id,"GGI")||!strcmp(id,"SGAEGIS");}
static uint32_t sg_bp_nearest_enemy(uint32_t h,uint32_t at,double range,int want_air,int units_only){
 uint32_t best=0;double bd=range;
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t q=sg_vec_at(0xA8EC78,i);uint32_t vt=MEM32(q);if(!sg_alive(q)||!sg_enemy(h,MEM32(q+0x21C)))continue;
  int air=vt==0x7E22A4||MEM8(q+0x134);if(want_air>=0&&air!=want_air)continue;if(units_only&&vt==0x7E3EBC)continue;
  if(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E22A4&&vt!=0x7E3EBC)continue;double d=sg_dist(at,q);if(d<bd){bd=d;best=q;}}
 return best;
}
static int sg_bp_engaged(uint32_t h,uint32_t p){uint32_t t=MEM32(p+0x2B4);return t>=0x10000&&sg_alive(t)&&sg_enemy(h,MEM32(t+0x21C));}
static int sg_bp_call_help(SGBPBrain*b,uint32_t victim,uint32_t attacker,double radius,int count){
 int sent=0;
 for(int i=0;i<sg_vec_count(0xA8EC78)&&sent<count;i++){uint32_t p=sg_vec_at(0xA8EC78,i);
  if(!sg_bp_free(p,b->house)||sg_bp_engaged(b->house,p)||sg_dist(p,victim)>radius)continue;
  if(b->state==SG_BP_MARCH&&sg_bp_in_wave(b,p))continue; /* a marching wave keeps marching */
  if(MEM32(p+0x6C)*10<sg_bp_strength(sg_ai_id(p))*3)continue;
  sg_bp_order(p,attacker,1);sent++;}
 return sent;
}
static void sg_bp_reflex(SGBPBrain*b,uint32_t f){
 if(f-b->last_reflex<15)return;b->last_reflex=f;uint32_t h=b->house;
 int hits=0,helpers=0,retreats=0,focus=0;
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){
  uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p)||MEM32(p+0x21C)!=h)continue;uint32_t vt=MEM32(p);
  if(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E3EBC)continue;
  uint32_t uid=MEM32(p+0x10);unsigned slot=(uid*2654435761u)%384;
  int hp=(int32_t)MEM32(p+0x6C),was=hp;
  if(b->mem[slot].ptr==p&&b->mem[slot].uid==uid)was=b->mem[slot].hp;else{b->mem[slot].ptr=p;b->mem[slot].uid=uid;b->mem[slot].acted=0;}
  b->mem[slot].hp=hp;
  const char*id=sg_ai_id(p);int maxhp=sg_bp_strength(id);int hurt=hp<was;
  if(vt==0x7E3EBC){ /* building under fire */
   if(hurt&&f-b->mem[slot].acted>75){uint32_t a=sg_bp_nearest_enemy(h,p,12,-1,1);if(a){helpers+=sg_bp_call_help(b,p,a,25,6);b->mem[slot].acted=f;hits++;}}
   continue;}
  int miner=!strcmp(id,"HARV")||!strcmp(id,"CMIN")||!strcmp(id,"SMIN");
  if(miner){if(hurt&&f-b->mem[slot].acted>75){uint32_t a=sg_bp_nearest_enemy(h,p,9,-1,1);if(a){helpers+=sg_bp_call_help(b,p,a,30,5);b->mem[slot].acted=f;hits++;}}continue;}
  if(!sg_bp_free(p,h))continue;
  /* Badly hurt: pull out of the fight and go home. */
  if(hp*10<maxhp*3&&(sg_bp_in_wave(b,p)||sg_bp_engaged(h,p))){
   for(int k=0;k<b->wn;k++)if(b->wave[k]==p)b->wave[k]=0;
   if(f-b->mem[slot].acted>150){sg_bp_move(p,b->sx,b->sy);b->mem[slot].acted=f;retreats++;}continue;}
  if(hurt&&!sg_bp_engaged(h,p)){uint32_t a=sg_bp_nearest_enemy(h,p,9,-1,0);if(a){sg_bp_order(p,a,1);hits++;helpers+=sg_bp_call_help(b,p,a,8,4);}continue;}
  if(b->state==SG_BP_ASSAULT&&sg_bp_in_wave(b,p)&&f-b->mem[slot].acted>45){
   uint32_t t=MEM32(p+0x2B4),best=0;
   if(sg_bp_is_aa(id))best=sg_bp_nearest_enemy(h,p,8,1,1);
   if(!best&&(!t||MEM32(t)==0x7E3EBC)){ /* weakest enemy unit within reach */
    double bs=1e30;for(int k=0;k<sg_vec_count(0xA8EC78);k++){uint32_t q=sg_vec_at(0xA8EC78,k);uint32_t qv=MEM32(q);if((qv!=0x7F5C70&&qv!=0x7EB058)||!sg_alive(q)||!sg_enemy(h,MEM32(q+0x21C))||MEM8(q+0x134))continue;
     double d=sg_dist(p,q);if(d>6)continue;double sc=(double)MEM32(q+0x6C)/sg_bp_strength(sg_ai_id(q))+d*.05;if(sc<bs){bs=sc;best=q;}}}
   if(best&&best!=t){sg_bp_order(p,best,1);b->mem[slot].acted=f;focus++;}
  }
 }
 b->r_hits+=hits;b->r_help+=helpers;b->r_retreat+=retreats;b->r_focus+=focus;
 if(f%450<15&&(b->r_hits||b->r_retreat||b->r_focus)){fprintf(stderr,"[brutalplus] house=%d reflexes (30 s): %d hit-backs, %d helpers sent, %d retreats, %d focus-fire\n",MEM32(h+0x30),b->r_hits,b->r_help,b->r_retreat,b->r_focus);b->r_hits=b->r_help=b->r_retreat=b->r_focus=0;}
}
static void sg_bp_army(SGBPBrain*b,uint32_t f,int difficulty){
 uint32_t h=b->house;int minutes=(int)(f/900);
 /* Rally point: a third of the way from home toward the current target. */
 if(!b->target||!sg_alive(b->target)||!sg_enemy(h,MEM32(b->target+0x21C))){b->rx=b->sx;b->ry=b->sy;b->target=sg_bp_pick_target(b);}
 if(!b->target)return;
 int tx=(int32_t)MEM32(b->target+0x9C),ty=(int32_t)MEM32(b->target+0xA0);
 b->rx=b->sx+(tx-b->sx)/3;b->ry=b->sy+(ty-b->sy)/3;
 if(b->state==SG_BP_GATHER){
  int value=0,n=0,moved=0;
  for(int i=0;i<sg_vec_count(0xA8EC78);i++){
   uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_bp_free(p,h))continue;
   value+=sg_bp_cost(sg_ai_id(p));n++;
   double dx=(int32_t)MEM32(p+0x9C)-b->rx,dy=(int32_t)MEM32(p+0xA0)-b->ry;uint32_t m=MEM32(p+0xAC);
   if(dx*dx+dy*dy>5.*5*256*256&&(m==0||m==5||m==11||m==13)&&moved<12){int j=((int)(MEM32(p+0x10)%5)-2)*256;sg_bp_move(p,b->rx+j,b->ry-j);moved++;}
  }
  /* Army size needed grows with time; easier settings launch smaller waves. */
  int need=(4000+600*minutes)*(4-difficulty)/4;if(need>20000)need=20000;
  need=need*sg_llm_need_pct(h)/100; /* attack/harass launch sooner, defend/expand hold back */
  int waited=f-b->state_since>3600&&value>=need/2&&n>=6;
  if((value>=need&&n>=8)||n>=40||waited){
   b->wn=0;for(int i=0;i<sg_vec_count(0xA8EC78)&&b->wn<96;i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(sg_bp_free(p,h)&&MEM32(p+0x6C)*10>=sg_bp_strength(sg_ai_id(p))*3)b->wave[b->wn++]=p;}
   /* Stage six cells short of the target, then assault together. */
   double dx=b->sx-tx,dy=b->sy-ty,len=sqrt(dx*dx+dy*dy);if(len<1)len=1;
   b->rx=tx+(int)(dx/len*6*256);b->ry=ty+(int)(dy/len*6*256);
   for(int i=0;i<b->wn;i++){int j=((int)(MEM32(b->wave[i]+0x10)%5)-2)*256;if(b->wave[i])sg_bp_move(b->wave[i],b->rx+j,b->ry+j);}
   sg_llm_wave(h,value,0,0);b->launch_value=value;b->launch_frame=f;b->state=SG_BP_MARCH;b->state_since=f;b->waves++;
   fprintf(stderr,"[brutalplus] house=%d WAVE %d launched: %d units worth $%d (needed $%d) target=%s\n",MEM32(h+0x30),b->waves,b->wn,value,need,sg_ai_id(b->target));
  }
  return;
 }
 int alive=0,value=sg_bp_value(b,&alive);
 if(b->state==SG_BP_MARCH){
  double sx=b->sx-tx,sy=b->sy-ty,len=sqrt(sx*sx+sy*sy);if(len<1)len=1;int px=tx+(int)(sx/len*6*256),py=ty+(int)(sy/len*6*256);
  int near=0;for(int i=0;i<b->wn;i++){uint32_t p=b->wave[i];if(!sg_alive(p))continue;double dx=(int32_t)MEM32(p+0x9C)-px,dy=(int32_t)MEM32(p+0xA0)-py;if(dx*dx+dy*dy<8.*8*256*256)near++;}
  if(near*10>=alive*6||f-b->state_since>1350){
   for(int i=0;i<b->wn;i++){uint32_t p=b->wave[i];if(!sg_alive(p))continue;
    if(!strcmp(sg_ai_id(p),"SGTANK")){SGUnit*u=sg_state(p);if(u)u->nest_active=1;}
    sg_bp_order(p,0,15);}
   b->state=SG_BP_ASSAULT;b->state_since=f;
   fprintf(stderr,"[brutalplus] house=%d wave %d ASSAULT with %d units ($%d)\n",MEM32(h+0x30),b->waves,alive,value);
  }
 }
 /* Pull back as a group instead of feeding units in one at a time. */
 if(value*10<b->launch_value*3||f-b->state_since>6000||!alive){
  for(int i=0;i<b->wn;i++){uint32_t p=b->wave[i];if(!sg_alive(p))continue;if(!strcmp(sg_ai_id(p),"SGTANK")){SGUnit*u=sg_state(p);if(u)u->nest_active=0;}sg_bp_move(p,b->sx,b->sy);}
  sg_llm_wave(h,0,alive,value);
  fprintf(stderr,"[brutalplus] house=%d wave %d over: %d units ($%d of $%d) regrouping\n",MEM32(h+0x30),b->waves,alive,value,b->launch_value);
  b->wn=0;b->state=SG_BP_GATHER;b->state_since=f;b->target=0;
 }
}
/* Singularity and EMP buildings: AIBuildThis alone never makes the native
 * planner pick them, so append one base node at a time. */
static int sg_bp_eligible(uint32_t h,const char*id){
 const char*country=(char*)hle_ptr(MEM32(h+0x34)+0x24);
 for(unsigned i=0;i<sizeof sg_bp_tech/sizeof sg_bp_tech[0];i++)if(!strcmp(id,sg_bp_tech[i].id)){
  if(!sg_ai_in_list(sg_bp_tech[i].owners,country)||(*sg_bp_tech[i].required&&!sg_ai_in_list(sg_bp_tech[i].required,country))||sg_ai_in_list(sg_bp_tech[i].forbidden,country))return 0;
  for(int j=0;j<8&&sg_bp_tech[i].prereqs[j];j++)if(!sg_ai_owns_any(h,sg_bp_tech[i].prereqs[j]))return 0;
  return 1;
 }return 0;
}
static const char*sg_bp_next_building(uint32_t h,uint32_t f){
 int minutes=(int)(f/900),credits=(int32_t)MEM32(h+0x30C),drones=0;
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(sg_alive(p)&&sg_enemy(h,MEM32(p+0x21C))){const char*id=sg_ai_id(p);if(!strcmp(id,"SGPDRONE")||!strcmp(id,"SGLOCUST")||!strcmp(id,"SGNEEDLE")||!strcmp(id,"DRON"))drones++;}}
 static const char*weap[]={"GAWEAP","NAWEAP","YAWEAP"},*barr[]={"GAPILE","NAHAND","YABRCK"};
 /* Second factories so the bonus money turns into units faster. */
 for(int k=0;k<3;k++){if(minutes>=4&&credits>=3000&&sg_bp_eligible(h,weap[k])&&sg_ai_count(h,weap[k])==1)return weap[k];
  if(minutes>=6&&credits>=3000&&sg_bp_eligible(h,barr[k])&&sg_ai_count(h,barr[k])==1)return barr[k];}
 if(sg_bp_eligible(h,"SGFACT")&&!sg_ai_count(h,"SGFACT")&&minutes>=3)return "SGFACT";
 if(sg_bp_eligible(h,"SGJAM")&&sg_ai_count(h,"SGJAM")<(drones>3?3:1)&&minutes>=5)return "SGJAM";
 if(sg_bp_eligible(h,"SGAEGIS")&&sg_ai_count(h,"SGAEGIS")<(drones>3?3:1)&&minutes>=6)return "SGAEGIS";
 if(minutes>=8&&credits>=1500){
  static const char*nodes[]={"SGNODE1","SGNODE2","SGNODE3","SGNODE4","SGNODE5"};
  for(int k=0;k<5;k++)if(!sg_ai_count(h,nodes[k])){if(sg_bp_eligible(h,nodes[k]))return nodes[k];break;}
  if(!sg_ai_count(h,"SGCOLO")&&sg_bp_eligible(h,"SGCOLO")&&credits>=4000)return "SGCOLO";
 }
 return NULL;
}
/* Append one node at a time to the native base plan; the native planner then
 * builds it in turn, with its normal placement, cost and build time. */
static void sg_bp_plan(SGBPBrain*b,uint32_t f){
 uint32_t h=b->house;if(f<b->next_build)return;b->next_build=f+450;
 const char*id=sg_bp_next_building(h,f);if(!id)return;
 int idx;uint32_t t=sg_ai_find_type(0xA83C68,id,&idx);if(!t)return;
 uint32_t v=h+0x5704,data=MEM32(v+4);int n=sg_vec_count(v);
 for(int i=0;i<n;i++)if((int32_t)MEM32(data+i*16)==idx&&!MEM8(data+i*16+8))return; /* already planned */
 /* DynamicVectorClass<BaseNodeClass> at House+0x5704: Items +4, Capacity +8,
    Count +0x10, CapacityIncrement +0x14; SetCapacity is vtable slot 2.
    Insert as the first unplaced node so it is the next thing built. */
 int cap=(int32_t)MEM32(v+8);
 if(n>=cap){int inc=(int32_t)MEM32(v+0x14);uint32_t args[2]={(uint32_t)(cap+(inc>0?inc:10)),0};sg_ai_call(v,MEM32(MEM32(v)+8),2,args);data=MEM32(v+4);
  if((int32_t)MEM32(v+8)<=n){fprintf(stderr,"[brutalplus] base plan full, %s skipped\n",id);return;}}
 int at=0;while(at<n&&MEM8(data+at*16+8))at++;
 memmove(hle_ptr(data+(at+1)*16),hle_ptr(data+at*16),(size_t)(n-at)*16);
 MEM32(data+at*16)=idx;MEM32(data+at*16+4)=0;MEM32(data+at*16+8)=0;MEM32(data+at*16+12)=0;MEM32(v+0x10)=n+1;
 fprintf(stderr,"[brutalplus] house=%d base plan + %s at node %d of %d\n",MEM32(h+0x30),id,at,n+1);
}
/* Idle factories: whenever the native AI has nothing queued in a factory and
 * the money is there, build a unit for the army pool from a mixed list that
 * leans toward counters to what the enemy fields (it sees the whole map). */
static void sg_bp_enemy_mix(uint32_t h,int*air,int*inf,int*armor,int*defs){
 *air=*inf=*armor=*defs=0;
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t q=sg_vec_at(0xA8EC78,i);if(!sg_alive(q)||!sg_enemy(h,MEM32(q+0x21C)))continue;uint32_t vt=MEM32(q);
  if(vt==0x7E22A4||MEM8(q+0x134))(*air)++;else if(vt==0x7EB058)(*inf)++;else if(vt==0x7F5C70)(*armor)++;}
 for(int i=0;i<sg_vec_count(0xA8EB40);i++){uint32_t q=sg_vec_at(0xA8EB40,i);if(!sg_alive(q)||!sg_enemy(h,MEM32(q+0x21C)))continue;const char*id=sg_ai_id(q);
  if(!strcmp(id,"GAPILL")||!strcmp(id,"NALASR")||!strcmp(id,"TESLA")||!strcmp(id,"ATESLA")||!strcmp(id,"GAPRSM")||!strcmp(id,"NABNKR")||!strcmp(id,"YAGGUN")||!strcmp(id,"GATSEN"))(*defs)++;}
}
static const char*sg_bp_pick_unit(uint32_t h,int infantry,unsigned turn){
 int side=MEM32(h+0x1E8),air,inf,armor,defs;sg_bp_enemy_mix(h,&air,&inf,&armor,&defs);
 const char*c[8];int n=0;
#define BP_TRY(id) do{if(n<8&&sg_bp_eligible(h,id))c[n++]=id;}while(0)
 /* LLM commander's build focus: skip the other category half the time and
    put its preferred units at the front of the list. */
 if(sg_llm_is(h,"build","armour")&&infantry&&turn%2)return NULL;
 if(sg_llm_is(h,"build","infantry")&&!infantry&&turn%2)return NULL;
 if(sg_llm_is(h,"build","antiair")){if(side==1){BP_TRY(infantry?"FLAKT":"HTK");BP_TRY(infantry?"FLAKT":"HTK");}else if(side==2){if(!infantry){BP_TRY("YTNK");BP_TRY("YTNK");}}else{BP_TRY(infantry?"GGI":"FV");BP_TRY(infantry?"GGI":"FV");}}
 if(sg_llm_is(h,"build","artillery")&&!infantry){if(side==1){BP_TRY("V3");BP_TRY("V3");}else if(side!=2){BP_TRY("SREF");BP_TRY("SREF");}}
 if(!infantry){
  if(side==1){BP_TRY("HTNK");BP_TRY("HTNK");if(air>2)BP_TRY("HTK");if(inf>armor+4)BP_TRY("HTK");if(defs>4)BP_TRY("V3");BP_TRY("APOC");BP_TRY("SGLOCUST");}
  else if(side==2){BP_TRY("LTNK");BP_TRY("YTNK");if(air>2)BP_TRY("YTNK");BP_TRY("LTNK");}
  else{BP_TRY("MTNK");BP_TRY("MTNK");if(air>2||inf>armor+4)BP_TRY("FV");BP_TRY("SREF");BP_TRY("SGTANK");if(armor>6)BP_TRY("TNKD");BP_TRY("FV");}
 }else{
  if(side==1){BP_TRY("E2");if(air>2)BP_TRY("FLAKT");BP_TRY("FLAKT");BP_TRY("SGOP");}
  else if(side==2){BP_TRY("INIT");BP_TRY("BRUTE");}
  else{BP_TRY("E1");if(armor>4)BP_TRY("GGI");if(air>2)BP_TRY("GGI");BP_TRY("SGLINK");}
 }
#undef BP_TRY
 return n?c[turn%n]:NULL;
}
static int sg_bp_production(uint32_t h,int category){
 if(category!=1&&category!=2)return 0;
 uint32_t off=category==1?0x5650:0x5654,vec=category==1?0xA83CE0:0xA8E348;
 if((int32_t)MEM32(h+off)>=0||(int32_t)MEM32(h+0x30C)<1500)return 0;
 SGBPBrain*b=sg_bp_brain(h);if(!b)return 0;
 /* Cap the standing army so extra money does not become an endless flood:
    about 12 units plus 2 per game minute, at most 60. */
 int army=0,cap=12+2*(int)(MEM32(0xA8ED84)/900);if(cap>60)cap=60;
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(sg_alive(p)&&MEM32(p+0x21C)==h&&sg_bp_is_foot(p))army++;}
 if(army>=cap)return 0;
 const char*id=sg_bp_pick_unit(h,category==2,b->picks++);if(!id)return 0;
 int idx;uint32_t t=sg_ai_find_type(vec,id,&idx);if(!t)return 0;
 uint32_t args[3]={t,0,0};if(sg_ai_call(h,0x4F7870,3,args)!=1)return 0;
 MEM32(h+off)=idx;b->built++;
 if(b->built%10==1)fprintf(stderr,"[brutalplus] house=%d idle factory -> %s (%d army units queued so far)\n",MEM32(h+0x30),id,b->built);
 return 1;
}
static void sg_bp_tick(uint32_t f){
 if(!sg_bp_mode())return;
 /* Headless playtests only (SG_BP_AUTODEPLOY=1): deploy the idle human's MCV
    so a match runs long enough to watch the AI's full cycle. */
 static int announced;if(!announced){announced=1;uint32_t sc=MEM32(0xA8B230);const char*mapname="?";
  if(sc>=0x10000)for(uint32_t o=0;o<0x3000;o++){const char*q=(const char*)hle_ptr(sc+o);if((q[0]=='.')&&(!strncasecmp(q,".map",4)||!strncasecmp(q,".mmx",4)||!strncasecmp(q,".yrm",4)||!strncasecmp(q,".mpr",4))){uint32_t st=o;while(st>0&&st>o-40&&((const char*)hle_ptr(sc+st-1))[0]>32&&((const char*)hle_ptr(sc+st-1))[0]<127)st--;mapname=(const char*)hle_ptr(sc+st);break;}}
  int houses=0;for(int i=0;i<sg_vec_count(0xA80228);i++){uint32_t h=sg_vec_at(0xA80228,i);if(MEM32(h+0x34)>0x10000&&!MEM8(MEM32(h+0x34)+0x1A6)){houses++;
   fprintf(stderr,"[brutalplus] player house=%d %s difficulty=%d human=%d brutalplus=%d allies=%08X\n",MEM32(h+0x30),(const char*)hle_ptr(MEM32(h+0x34)+0x24),(int32_t)MEM32(h+0x184),h==MEM32(0xA83D4C),sg_bp_house(h),MEM32(h+0x5788));}}
  fprintf(stderr,"[brutalplus] map %s, %d player houses\n",mapname,houses);}
 /* Test switches apply to the first match only: a new game started from the
    menu in the same window is a normal game again. */
 static uint32_t prev_frame;static int matches;if(f<prev_frame)matches++;prev_frame=f;
 static int autodeploy=-1;if(autodeploy<0)autodeploy=getenv("SG_BP_AUTODEPLOY")!=NULL;if(matches)autodeploy=0;
 if(autodeploy&&f%90==0)for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(sg_alive(p)&&MEM32(p+0x21C)==MEM32(0xA83D4C)&&strstr(sg_ai_id(p),"MCV")&&MEM32(p+0xAC)!=16){sg_ai_order(p,0,16);fprintf(stderr,"[brutalplus] test: deploying the player's %s\n",sg_ai_id(p));}}
 /* Spectator playtests only (SG_BP_SPECTATE=1): the human is allied with every
    AI, so the AIs fight each other (Brutal Plus vs Classic Brutal). */
 static int spectate=-1;if(spectate<0)spectate=getenv("SG_BP_SPECTATE")!=NULL;if(matches)spectate=0;
 if(spectate&&f%90==0){uint32_t me=MEM32(0xA83D4C);int mi=MEM32(me+0x30);
  for(int i=0;i<sg_vec_count(0xA80228);i++){uint32_t h=sg_vec_at(0xA80228,i);if(h==me||MEM32(h+0x34)<0x10000||MEM8(MEM32(h+0x34)+0x1A6))continue;int ix=MEM32(h+0x30);if(ix<0||ix>31||mi<0||mi>31)continue;
   for(int j=0;j<sg_vec_count(0xA80228);j++){uint32_t o=sg_vec_at(0xA80228,j);int oi=MEM32(o+0x30);if(o==me||o==h||MEM32(o+0x34)<0x10000||MEM8(MEM32(o+0x34)+0x1A6)||oi<0||oi>31)continue;
    if(MEM32(h+0x5788)&(1u<<oi)){MEM32(h+0x5788)&=~(1u<<oi);fprintf(stderr,"[brutalplus] test: house=%d no longer allied with house=%d\n",ix,oi);}}
   if(!(MEM32(me+0x5788)&(1u<<ix))||!(MEM32(h+0x5788)&(1u<<mi))){MEM32(me+0x5788)|=1u<<ix;MEM32(h+0x5788)|=1u<<mi;fprintf(stderr,"[brutalplus] test: spectator allied with house=%d\n",ix);}}}
 /* Once a minute: a scoreboard of every player (buildings, units, army value). */
 if(f%900==0&&f){char line[512];int n=snprintf(line,sizeof line,"[brutalplus] score min=%u",f/900);
  for(int i=0;i<sg_vec_count(0xA80228);i++){uint32_t h=sg_vec_at(0xA80228,i);if(MEM32(h+0x34)<0x10000||MEM8(MEM32(h+0x34)+0x1A6))continue;int nb=0,nu=0,val=0;
   for(int k=0;k<sg_vec_count(0xA8EC78);k++){uint32_t q=sg_vec_at(0xA8EC78,k);if(!sg_alive(q)||MEM32(q+0x21C)!=h)continue;if(MEM32(q)==0x7E3EBC)nb++;else{nu++;val+=sg_bp_cost(sg_ai_id(q));}}
   const char*who=h==MEM32(0xA83D4C)?"you":sg_bp_house(h)?"BrutalPlus":"Classic";
   if(n<(int)sizeof line)n+=snprintf(line+n,sizeof line-n," | %s#%d %s: %d bldg, %d units ($%d)",who,MEM32(h+0x30),(const char*)hle_ptr(MEM32(h+0x34)+0x24),nb,nu,val);}
  fprintf(stderr,"%s\n",line);
  if(f%9000==0)for(int i=0;i<sg_vec_count(0xA80228);i++){uint32_t h=sg_vec_at(0xA80228,i);if(MEM32(h+0x34)<0x10000||MEM8(MEM32(h+0x34)+0x1A6))continue;char bl[600];int m=snprintf(bl,sizeof bl,"[brutalplus] buildings house=%d power=%d/%d allies=%08X:",MEM32(h+0x30),(int32_t)MEM32(h+0x53A4),(int32_t)MEM32(h+0x53A8),MEM32(h+0x5788));
   for(int k=0;k<sg_vec_count(0xA8EB40)&&m<(int)sizeof bl-12;k++){uint32_t q=sg_vec_at(0xA8EB40,k);if(sg_alive(q)&&MEM32(q+0x21C)==h)m+=snprintf(bl+m,sizeof bl-m," %s",sg_ai_id(q));}
   fprintf(stderr,"%s\n",bl);
   uint32_t v=h+0x5704,data=MEM32(v+4);int n=sg_vec_count(v),shown=0;char nl[400];int m2=snprintf(nl,sizeof nl,"[brutalplus] next plan nodes house=%d (%d):",MEM32(h+0x30),n);
   for(int k=0;k<n&&shown<8;k++){if(MEM8(data+k*16+8))continue;int ti=(int32_t)MEM32(data+k*16);const char*tn=ti>=0&&ti<sg_vec_count(0xA83C68)?(const char*)hle_ptr(sg_vec_at(0xA83C68,ti)+0x24):"?";
    m2+=snprintf(nl+m2,sizeof nl-m2," %s@%d,%d/a%d",tn,(int16_t)MEM16(data+k*16+4),(int16_t)MEM16(data+k*16+6),(int32_t)MEM32(data+k*16+12));shown++;if(m2>=(int)sizeof nl-40)break;}
   fprintf(stderr,"%s\n",nl);}}
 for(int hi=0;hi<sg_vec_count(0xA80228);hi++){
  uint32_t h=sg_vec_at(0xA80228,hi);if(!sg_bp_house(h))continue;SGBPBrain*b=sg_bp_brain(h);if(!b)continue;
  if(f<b->last){memset(b,0,sizeof *b);b->house=h;}
  if(b->sx)sg_bp_reflex(b,f); /* every second */
  if(f-b->last<45)continue;b->last=f; /* every 3 seconds */
  int difficulty=(int32_t)MEM32(h+0x184);if(difficulty<0||difficulty>2)difficulty=0;
  uint32_t anchor=0;for(int i=0;i<sg_vec_count(0xA8EB40);i++){uint32_t q=sg_vec_at(0xA8EB40,i);if(!sg_alive(q)||MEM32(q+0x21C)!=h)continue;const char*id=sg_ai_id(q);if(!anchor||strstr(id,"CNST"))anchor=q;}
  if(!anchor)continue;b->sx=MEM32(anchor+0x9C);b->sy=MEM32(anchor+0xA0);
  if(!b->state_since)b->state_since=f;
  sg_bp_cash(b,f);
  sg_bp_plan(b,f);
  int threat=sg_bp_defend(b);
  /* A big attack on the base recalls a wave that has not engaged yet. */
  if((b->state==SG_BP_MARCH&&threat>3000)||(b->state==SG_BP_ASSAULT&&threat>6000)){for(int i=0;i<b->wn;i++)if(sg_alive(b->wave[i]))sg_bp_move(b->wave[i],b->sx,b->sy);b->wn=0;b->state=SG_BP_GATHER;b->state_since=f;fprintf(stderr,"[brutalplus] house=%d wave recalled to defend ($%d threat)\n",MEM32(h+0x30),threat);}
  if(sg_llm_is(h,"stance","defend")&&b->state==SG_BP_MARCH){for(int i=0;i<b->wn;i++)if(sg_alive(b->wave[i]))sg_bp_move(b->wave[i],b->sx,b->sy);b->wn=0;b->state=SG_BP_GATHER;b->state_since=f;fprintf(stderr,"[llm] wave called home (defend stance)\n");}
  sg_bp_army(b,f,difficulty);
  sg_llm_tick(b,f,threat);
  if(f%900<45){int n=0,v=0,teamed=0;char plan[32]="-",enemies[64]="";int pi=(int32_t)MEM32(h+0x564C);if(pi>=0&&pi<sg_vec_count(0xA83C68))snprintf(plan,sizeof plan,"%s",(const char*)hle_ptr(sg_vec_at(0xA83C68,pi)+0x24));
   for(int i=0;i<sg_vec_count(0xA80228);i++){uint32_t o=sg_vec_at(0xA80228,i);if(sg_enemy(h,o)&&MEM32(o+0x34)>0x10000&&!MEM8(MEM32(o+0x34)+0x1A6)){size_t l=strlen(enemies);snprintf(enemies+l,sizeof enemies-l,"%d,",MEM32(o+0x30));}}for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p)||MEM32(p+0x21C)!=h||!sg_bp_is_foot(p))continue;if(sg_bp_free(p,h)){n++;v+=sg_bp_cost(sg_ai_id(p));}else if(MEM32(p+SG_BP_TEAM)>=0x10000)teamed++;}
   fprintf(stderr,"[brutalplus] house=%d status min=%u state=%d free=%d ($%d) in-teams=%d wave=%d credits=%d target=%s plan=%s enemies=%s\n",MEM32(h+0x30),f/900,b->state,n,v,teamed,b->wn,MEM32(h+0x30C),b->target?sg_ai_id(b->target):"-",plan,enemies);}
 }
}
