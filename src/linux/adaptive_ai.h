/* Experimental local commander: no network/model calls. All strategic enemy
 * inputs are copied into bounded observations behind the sensor filter. */
typedef struct {uint32_t ptr,uid,seen;int x,y,kind;char id[24];} SGAIContact;
typedef struct {uint32_t house,last,turn;int air,armor,infantry,staticdef,ownarmy,scout_phase;int bx,by;char report[96];SGAIContact contacts[256];} SGAIBrain;
static SGAIBrain sg_ai_brains[32];
static int sg_ai_issuing;
static int sg_ai_mode(void){return !strcmp(mods_active(),"Singularity - Adaptive")&&MEM32(0xA8B238)==5;}
int sg_ai_house(uint32_t h){return sg_ai_mode()&&h>=0x10000&&!MEM8(h+0x1EC)&&!MEM8(MEM32(h+0x34)+0x1A6);}
static uint32_t sg_ai_type(uint32_t p){uint32_t vt=MEM32(p);return MEM32(p+(vt==0x7E3EBC?0x520:vt==0x7F5C70?0x6C4:0x6C0));}
static int sg_ai_kind(uint32_t p){uint32_t v=MEM32(p);if(v==0x7E3EBC)return 3;if(v==0x7E22A4||MEM8(p+0x134))return 0;return v==0x7EB058?2:1;}
static const char*sg_ai_id(uint32_t p){return (const char*)hle_ptr(sg_ai_type(p)+0x24);}
/* Conservative five-cell vision, capped to a unit's actual INI sight by the
 * generated table. Cloaked targets are never exposed to this controller. */
#include "adaptive_sight.h"
static int sg_ai_visible(uint32_t h,uint32_t target){
 if(!sg_alive(target)||MEM32(target+0x220)!=0||MEM8(target+0x1D8))return 0;
 int tx=(int32_t)MEM32(target+0x9C),ty=(int32_t)MEM32(target+0xA0),tz=(int32_t)MEM32(target+0xA4);
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){
  uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p)||MEM32(p+0x21C)!=h)continue;
  uint32_t vt=MEM32(p);if(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E3EBC&&vt!=0x7E22A4)continue;
  int sight=sg_ai_sight(sg_ai_id(p));double dx=tx-(int32_t)MEM32(p+0x9C),dy=ty-(int32_t)MEM32(p+0xA0);
  /* Do not spot a ground target above our observer's terrain from below. */
  if(tz>(int32_t)MEM32(p+0xA4)+200&&!MEM8(target+0x134))continue;
  if(dx*dx+dy*dy<=(double)sight*sight*256*256)return 1;
 }
 return 0;
}
int sg_ai_observable_for(uint32_t house,uint32_t target){return !sg_ai_house(house)||MEM32(target+0x21C)==house||sg_ai_visible(house,target);}
int sg_ai_target_allowed(uint32_t source,uint32_t target){
 if(!source||!target)return 1;uint32_t h=MEM32(source+0x21C);if(!sg_ai_house(h))return 1;
 uint32_t vt=MEM32(target);if(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E3EBC&&vt!=0x7E22A4)return 0;
 if(MEM32(target+0x21C)==h)return 1;
 return sg_ai_visible(h,target);
}
int sg_ai_destination_allowed(uint32_t source,uint32_t target){
 if(!target)return 1;uint32_t vt=MEM32(target);
 if(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E3EBC&&vt!=0x7E22A4){
  uint32_t h=MEM32(source+0x21C);if(!sg_ai_house(h)||sg_ai_issuing)return 1;
  const char*id=sg_ai_id(source);if(!strcmp(id,"HARV")||!strcmp(id,"CMIN")||strstr(id,"MCV")||!strcmp(id,"PCV"))return 1;
  /* Strategic cell orders must originate in this commander. The native
     engine may retain an existing destination, but may not invent another. */
  return target==MEM32(source+0x5A4);
 }
 return sg_ai_target_allowed(source,target);
}
static SGAIBrain*sg_ai_brain(uint32_t h){int i=MEM32(h+0x30);if(i<0||i>=32)return NULL;SGAIBrain*b=&sg_ai_brains[i];if(b->house!=h){memset(b,0,sizeof *b);b->house=h;}return b;}
static void sg_ai_observe(SGAIBrain*b,uint32_t f){
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){
  uint32_t p=sg_vec_at(0xA8EC78,i);uint32_t vt=MEM32(p);if(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E3EBC&&vt!=0x7E22A4)continue;
  if(!sg_enemy(b->house,MEM32(p+0x21C))||!sg_ai_visible(b->house,p))continue;
  uint32_t uid=MEM32(p+0x10);SGAIContact*c=NULL,*old=&b->contacts[0];
  for(int j=0;j<256;j++){SGAIContact*q=&b->contacts[j];if(q->uid==uid&&q->ptr==p){c=q;break;}if(q->seen<old->seen)old=q;}
  if(!c)c=old;c->ptr=p;c->uid=uid;c->seen=f;c->x=MEM32(p+0x9C);c->y=MEM32(p+0xA0);c->kind=sg_ai_kind(p);snprintf(c->id,sizeof c->id,"%s",sg_ai_id(p));
 }
 b->air=b->armor=b->infantry=b->staticdef=0;
 for(int j=0;j<256;j++){SGAIContact*c=&b->contacts[j];if(!c->seen||f-c->seen>1800)continue;switch(c->kind){case 0:b->air++;break;case 1:b->armor++;break;case 2:b->infantry++;break;default:b->staticdef++;}}
}
static int sg_ai_count(uint32_t h,const char*id){int n=0;for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(sg_alive(p)&&MEM32(p+0x21C)==h&&!strcmp(sg_ai_id(p),id))n++;}return n;}
#include "adaptive_tech.h"
static int sg_ai_in_list(const char*list,const char*id){size_t n=strlen(id);while(*list){const char*end=strchr(list,',');size_t len=end?(size_t)(end-list):strlen(list);if(len==n&&!strncmp(list,id,n))return 1;if(!end)break;list=end+1;}return 0;}
static int sg_ai_owns_any(uint32_t h,const char*list){for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(sg_alive(p)&&MEM32(p+0x21C)==h&&sg_ai_in_list(list,sg_ai_id(p)))return 1;}return 0;}
static int sg_ai_eligible(uint32_t h,const char*id){
 const char*country=(char*)hle_ptr(MEM32(h+0x34)+0x24);
 for(unsigned i=0;i<sizeof sg_ai_tech/sizeof sg_ai_tech[0];i++)if(!strcmp(id,sg_ai_tech[i].id)){
  if(!sg_ai_in_list(sg_ai_tech[i].owners,country)||(*sg_ai_tech[i].required&&!sg_ai_in_list(sg_ai_tech[i].required,country))||sg_ai_in_list(sg_ai_tech[i].forbidden,country))return 0;
  for(int j=0;j<8&&sg_ai_tech[i].prereqs[j];j++)if(!sg_ai_owns_any(h,sg_ai_tech[i].prereqs[j]))return 0;
  return 1;
 }return 0;
}
#ifndef SG_STATE_TEST
static uint32_t sg_ai_call(uint32_t self,uint32_t address,int n,uint32_t*args){uint32_t saved=g_ecx;g_ecx=self;uint32_t out=hle_call_guest(address,n,args);g_ecx=saved;return out;}
static uint32_t sg_ai_find_type(uint32_t vector,const char*id,int*idx){for(int i=0;i<sg_vec_count(vector);i++){uint32_t t=sg_vec_at(vector,i);if(!strcmp((char*)hle_ptr(t+0x24),id)){*idx=i;return t;}}return 0;}
static void sg_ai_choose(uint32_t h,uint32_t vector,uint32_t offset,const char*id){
 if((int32_t)MEM32(h+offset)>=0||!sg_ai_eligible(h,id))return;int idx;uint32_t t=sg_ai_find_type(vector,id,&idx);if(!t)return;
 uint32_t args[3]={t,0,0};if(sg_ai_call(h,0x4F7870,3,args)!=1)return;
 MEM32(h+offset)=idx;fprintf(stderr,"[adaptive] house=%d production=%s via normal factory\n",MEM32(h+0x30),id);
}
static int sg_bp_refuse(uint32_t p,uint32_t target,int mission);
static void sg_ai_order(uint32_t p,uint32_t target,int mission){
 if(sg_bp_refuse(p,target,mission))return;
 sg_ai_issuing++;uint32_t args[2]={(uint32_t)mission,1};sg_ai_call(p,0x5B35E0,2,args);
 if(target){args[0]=target;args[1]=1;sg_ai_call(p,MEM32(MEM32(p)+0x480),2,args);if(mission==1||mission==8)sg_ai_call(p,MEM32(MEM32(p)+0x3C8),1,&target);}
 sg_ai_issuing--;
}
static uint32_t sg_ai_cell(int x,int y){int32_t*c=hle_heap_alloc(12,1);if(!c)return 0;c[0]=x;c[1]=y;c[2]=0;uint32_t arg=hle_va(c);uint32_t p=(sg_ai_call(0x87F7E8,0x5785F0,1,&arg)&255)?sg_ai_call(0x87F7E8,0x565730,1,&arg):0;hle_heap_free(c);return p;}
static void sg_ai_move(uint32_t p,int x,int y){uint32_t cell=sg_ai_cell(x,y);if(cell)sg_ai_order(p,cell,2);}
/* Existing object targets must not turn into a live tracking feed when
 * sight is lost. This runs each simulation frame, independent of plan cadence. */
static void sg_ai_sanitize(void){
 if(!sg_ai_mode())return;
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){
  uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p))continue;uint32_t h=MEM32(p+0x21C);if(!sg_ai_house(h))continue;
  uint32_t target=MEM32(p+0x2B4),zero=0;
  if(target&&!sg_ai_target_allowed(p,target))sg_ai_call(p,MEM32(MEM32(p)+0x3C8),1,&zero);
  uint32_t vt=MEM32(p);if(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E22A4)continue;
  uint32_t dest=MEM32(p+0x5A4);if(!dest)continue;uint32_t dv=MEM32(dest);
  if((dv==0x7F5C70||dv==0x7EB058||dv==0x7E3EBC||dv==0x7E22A4)&&!sg_ai_target_allowed(p,dest)){
   uint32_t args[2]={0,1};sg_ai_call(p,MEM32(vt+0x480),2,args);
   SGAIBrain*b=sg_ai_brain(h);if(b)for(int j=0;j<256;j++){SGAIContact*c=&b->contacts[j];if(c->ptr==dest&&c->uid==MEM32(dest+0x10)&&c->seen&&MEM32(0xA8ED84)-c->seen<900){sg_ai_move(p,c->x,c->y);break;}}
  }
 }
}
#include "adaptive_ai_bp.h"
/* Legacy strategic unit selection is bypassed. Existing factory machinery still
 * charges credits, obeys prerequisites and takes normal time to finish units. */
int sg_ai_production(uint32_t h,int category){
 if(sg_bp_house(h))return sg_bp_production(h,category);
 if(!sg_ai_house(h))return 0;SGAIBrain*b=sg_ai_brain(h);if(!b)return 1;
 int sov=MEM32(h+0x1E8)==1;
 if(category==0){
  if(!sg_ai_count(h,sov?"NACNST":"GACNST"))return 0; /* Native MCV deployment. */
  if((int32_t)MEM32(h+0x564C)>=0)return 1;
  const char*id=NULL;
  if((int32_t)MEM32(h+0x53A4)-(int32_t)MEM32(h+0x53A8)<100)id=sov?"NAPOWR":"GAPOWR";
  else if(!sg_ai_count(h,sov?"NAREFN":"GAREFN"))id=sov?"NAREFN":"GAREFN";
  else if(!sg_ai_count(h,sov?"NAHAND":"GAPILE"))id=sov?"NAHAND":"GAPILE";
  else if(!sg_ai_count(h,sov?"NAWEAP":"GAWEAP")&&!sg_ai_count(h,"SGFACT"))id=sov?"NAWEAP":"GAWEAP";
  else if(sg_ai_count(h,sov?"NAREFN":"GAREFN")<2)id=sov?"NAREFN":"GAREFN";
  else if(!sg_ai_count(h,sov?"NARADR":"GAAIRC")&&!sg_ai_count(h,"AMRADR"))id=sov?"NARADR":"GAAIRC";
  else if(sov&&!sg_ai_count(h,"SGFACT")&&sg_ai_eligible(h,"SGFACT"))id="SGFACT";
  else if(!sg_ai_count(h,sov?"NATECH":"GATECH"))id=sov?"NATECH":"GATECH";
  else if(b->air&&sg_ai_count(h,sov?"NAFLAK":"NASAM")<3)id=sov?"NAFLAK":"NASAM";
  else if(b->armor+b->infantry&&sg_ai_count(h,sov?"NALASR":"GAPILL")<2)id=sov?"NALASR":"GAPILL";
  if(!id)return 1;
  sg_ai_choose(h,0xA83C68,0x564C,id);
  if((int32_t)MEM32(h+0x564C)<0&&!strcmp(id,"NAWEAP"))sg_ai_choose(h,0xA83C68,0x564C,"SGFACT");
  if((int32_t)MEM32(h+0x564C)<0&&!strcmp(id,"GAAIRC"))sg_ai_choose(h,0xA83C68,0x564C,"AMRADR");
  int idx=(int32_t)MEM32(h+0x564C);if(idx<0)return 1;
  /* Replace pending stock plans with one node chosen from our economy and
     observed threats. Placed nodes remain for the engine's base geometry. */
  uint32_t v=h+0x5704,data=MEM32(v+4);int n=sg_vec_count(v),kept=0;
  for(int i=0;i<n;i++){uint32_t node=data+i*16;if(MEM8(node+8)){if(i!=kept)memcpy(hle_ptr(data+kept*16),hle_ptr(node),16);kept++;}}
  MEM32(v+0x10)=kept;uint32_t*node=hle_heap_alloc(16,1);node[0]=idx;node[1]=node[2]=node[3]=0;uint32_t arg=hle_va(node);sg_ai_call(v,MEM32(MEM32(v)+0x10),1,&arg);hle_heap_free(node);
 }else if(category==1){
  int miners=sg_ai_count(h,sov?"HARV":"CMIN");const char*id;
  if(miners<2)id=sov?"HARV":"CMIN";
  else if((int32_t)MEM32(h+0x30C)<1000)return 1;
  else if(b->air>2)id=sov?"HTK":"FV";
  else if(b->infantry>b->armor+3)id=sov?"HTK":"FV";
  else if(b->staticdef>5&&b->turn%3==0)id=sov?"V3":"SREF";
  else if(b->turn%7==0)id=sov?"SGLOCUST":"SGTANK";
  else id=sov?"HTNK":"MTNK";
  sg_ai_choose(h,0xA83CE0,0x5650,id);
  if((int32_t)MEM32(h+0x5650)<0)sg_ai_choose(h,0xA83CE0,0x5650,sov?"HTNK":"MTNK");
 }else if(category==2){
  if((int32_t)MEM32(h+0x30C)<1000)return 1;
  const char*engi=sov?"SENGINEER":"ENGINEER";const char*id=sg_ai_count(h,engi)<1?engi:b->turn%6==0?(sov?"SGOP":"SGLINK"):b->air>1?(sov?"FLAKT":"GGI"):b->armor>4?(sov?"FLAKT":"GGI"):(sov?"E2":"E1");
  sg_ai_choose(h,0xA8E348,0x5654,id);
  if((int32_t)MEM32(h+0x5654)<0)sg_ai_choose(h,0xA8E348,0x5654,sov?"E2":"E1");
 }
 /* Aircraft are intentionally not mass-produced in the first fair profile. */
 return 1;
}
static void sg_ai_tick(uint32_t f){
 sg_bp_tick(f);
 if(!sg_ai_mode())return;
 for(int hi=0;hi<sg_vec_count(0xA80228);hi++){
  uint32_t h=sg_vec_at(0xA80228,hi);if(!sg_ai_house(h))continue;SGAIBrain*b=sg_ai_brain(h);if(!b)continue;
  if(f<b->last){memset(b,0,sizeof *b);b->house=h;}int difficulty=(int32_t)MEM32(h+0x184);if(difficulty<0||difficulty>2)difficulty=0;int cadence=45+30*difficulty,order_limit=8-2*difficulty;if(f-b->last<(uint32_t)cadence)continue;b->last=f;b->turn++;
  MEM8(h+0x1EF)=0;MEM8(h+0x1F2)=0; /* AutocreateAllowed / AITriggersActive. */
  sg_ai_observe(b,f);
  int own=0,buildings=0;uint32_t anchor=0;
  for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p)||MEM32(p+0x21C)!=h)continue;uint32_t vt=MEM32(p);if(vt==0x7E3EBC){buildings++;if(!anchor||!strcmp(sg_ai_id(p),"GACNST")||!strcmp(sg_ai_id(p),"NACNST"))anchor=p;}else if(vt==0x7F5C70||vt==0x7EB058)own++;}
  if(anchor){b->bx=MEM32(anchor+0x9C);b->by=MEM32(anchor+0xA0);}b->ownarmy=own;
  int orders=0,contactCount=0;uint32_t bridge=0;
  for(int j=0;j<256;j++)if(b->contacts[j].seen&&f-b->contacts[j].seen<900)contactCount++;
  for(int i=0;i<sg_vec_count(0xA8EB40);i++){
   uint32_t p=sg_vec_at(0xA8EB40,i);if(!sg_ai_visible(h,p))continue;const char*id=sg_ai_id(p);if(strcmp(id,"CABHUT"))continue;
   uint32_t cell=(uint16_t)((int32_t)MEM32(p+0x9C)/256)|((uint32_t)(uint16_t)((int32_t)MEM32(p+0xA0)/256)<<16),*q=hle_heap_alloc(4,1);*q=cell;uint32_t arg=hle_va(q);int broken=sg_ai_call(0x87F7E8,0x587410,1,&arg)&255;hle_heap_free(q);if(broken){bridge=p;break;}
  }
  for(int i=0;i<sg_vec_count(0xA8EC78)&&orders<order_limit;i++){
   uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p)||MEM32(p+0x21C)!=h)continue;uint32_t vt=MEM32(p);if(vt!=0x7F5C70&&vt!=0x7EB058)continue;
   const char*id=sg_ai_id(p);if(!strcmp(id,"HARV")||!strcmp(id,"CMIN")||!strcmp(id,"SMIN")||strstr(id,"MCV")||!strcmp(id,"PCV"))continue;
   /* Rotate through units rather than issuing unlimited simultaneous orders. */
   if((MEM32(p+0x10)+b->turn)%3)continue;
   if(strstr(id,"ENGINEER")){if(bridge){sg_ai_order(p,bridge,8);orders++;snprintf(b->report,sizeof b->report,"Engineer dispatched to a discovered broken bridge.");}continue;}
   if(!strcmp(id,"SGTANK")){SGUnit*u=sg_state(p);if(u)u->nest_active=contactCount>0;}
   uint32_t best=0;double score=-1e20;int x=MEM32(p+0x9C),y=MEM32(p+0xA0);
   for(int j=0;j<256;j++){SGAIContact*c=&b->contacts[j];if(c->seen!=f)continue;if(c->kind==0&&strcmp(id,"HTK")&&strcmp(id,"FV")&&strcmp(id,"GGI")&&strcmp(id,"FLAKT")&&strcmp(id,"SGINT"))continue;uint32_t t=c->ptr;if(!sg_ai_visible(h,t)||MEM32(t+0x10)!=c->uid)continue;double dx=c->x-x,dy=c->y-y;double s=-sqrt(dx*dx+dy*dy);if(!strcmp(c->id,"HARV")||!strcmp(c->id,"CMIN"))s+=700;if(s>score){score=s;best=t;}}
   if(best){
    uint32_t type=sg_ai_type(p);int hp=MEM32(p+0x6C); /* Retreat uses only our health and own base. */
    if(hp<30&&anchor){sg_ai_move(p,b->bx,b->by);snprintf(b->report,sizeof b->report,"Pulling damaged units back. Cover the withdrawal.");}
    else {if(!strcmp(id,"SGPDRONE")){SGUnit*u=sg_state(p);if(u)u->suicide=sg_dist(p,best)<4;}sg_ai_order(p,best,1);snprintf(b->report,sizeof b->report,"Contact confirmed. Changing the attack mix.");}
    (void)type;orders++;
   }else if(MEM32(p+0xAC)!=2&&anchor){
    SGAIContact*known=NULL;for(int j=0;j<256;j++){SGAIContact*c=&b->contacts[j];if(c->seen&&f-c->seen<900&&c->kind==3){known=c;break;}}
    if(known&&own>=8){int flank=((int)(MEM32(p+0x10)%3)-1)*3*256;sg_ai_move(p,known->x+flank,known->y-flank);snprintf(b->report,sizeof b->report,"Checking the last sighting. No live target lock.");}
    else {
     /* Survey destinations use only public map bounds, never enemy starts.
        Deterministic sampling is separate from the engine's random stream. */
     uint32_t seed=MEM32(p+0x10)*2654435761u+b->turn*2246822519u;
     int span=(int32_t)MEM32(0x87F7E8+0xF4)+(int32_t)MEM32(0x87F7E8+0xFC)+2*(int32_t)MEM32(0x87F7E8+0x104)+2*(int32_t)MEM32(0x87F7E8+0x108)+16;
     if(span<32||span>512)span=256;
     for(int trial=0;trial<24;trial++){seed=seed*1664525u+1013904223u;int sx=(seed%(unsigned)span)*256+128;seed=seed*1664525u+1013904223u;int sy=(seed%(unsigned)span)*256+128;uint32_t cell=sg_ai_cell(sx,sy);if(cell){sg_ai_order(p,cell,2);break;}}
     snprintf(b->report,sizeof b->report,"Scouting. I do not have eyes on your base.");}orders++;
   }
  }
  if(b->turn%5==0)fprintf(stderr,"[adaptive] house=%d sightings air=%d armor=%d infantry=%d buildings=%d orders=%d own=%d buildings=%d credits=%d report=%s\n",MEM32(h+0x30),b->air,b->armor,b->infantry,b->staticdef,orders,own,buildings,MEM32(h+0x30C),b->report);
 }
}
#else
int sg_bp_house(uint32_t h){(void)h;return 0;}
static void sg_ai_sanitize(void){}
int sg_ai_production(uint32_t h,int category){(void)h;(void)category;return 0;}
static void sg_ai_tick(uint32_t f){(void)f;}
#endif
