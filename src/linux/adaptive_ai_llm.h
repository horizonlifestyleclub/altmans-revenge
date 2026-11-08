/* LLM Commander for Brutal Plus (opt-in: SG_LLM_DIR=<folder>).
 *
 * The top Brutal Plus house writes a "ledger" to <dir>/ledger.json (and
 * appends it to <dir>/match.jsonl) every 3 seconds, and at once when
 * something happens (base hit, wave engages, new enemy type, scout finds a
 * base...). The ledger holds only what this AI has SEEN:
 *   - what is in its units' sight right now;
 *   - a memory of everything it saw earlier (enemy buildings stay remembered
 *     until it sees them gone; enemy units for 2 minutes);
 *   - its own army, base, losses and kills, wave progress, scout and raid.
 * The only map knowledge it starts with is each enemy's start area (shown on
 * the map preview to any human player), which the scout goes to check.
 *
 * llm-bridge/commander.py answers with <dir>/orders.json:
 *   {"seq":7,"stance":"attack|defend|harass|expand|retreat",
 *    "target":"core|factory|refinery|conyard|army|defences|<building ID>",
 *    "build":"armour|infantry|antiair|artillery|<unit ID>",
 *    "scout":"now|", "raid":0..8, "note":"..."}
 * The game never waits: orders expire after 60 s, after which Brutal Plus
 * plays on its own rules again. */
#include <sys/stat.h>
#include <stdarg.h>
#include <math.h>
static int sg_bp_engaged(uint32_t h,uint32_t p);
static int sg_bp_eligible(uint32_t h,const char*id);
typedef struct {uint32_t ptr,uid,seen;int x,y,house,cost,building;char id[24];} SGLLMSeen;
typedef struct {uint32_t ptr,uid;int cost,building,miner;char id[24];} SGLLMOwn;
typedef struct {int house,x,y;uint32_t scouted;} SGLLMStart;
typedef struct {uint32_t f;int cost;} SGLLMLoss;
typedef struct {
 uint32_t house,got,last_write,last_event_write,last_frame;int seq,raid_seq,threat;
 char stance[12],target[16],build[16],scout_req[8],note[96];int raid_n;
 int waves,last_launch,last_end,last_alive;uint32_t wave_launch_f;
 SGLLMSeen seen[384];
 SGLLMOwn own[512];int nown;
 SGLLMStart starts[8];int nstarts;
 SGLLMLoss deaths[128],kills[128];int ndeaths,nkills;
 uint32_t scout,scout_uid,scout_since,scout_lost_at,scout_next;int scout_tx,scout_ty,scout_goal;char scout_id[24],scout_where[40];
 uint32_t raid[8],raid_uid[8],raid_target,raid_tuid;int rn;char raid_tid[24];
 char types_seen[96][24];int ntypes;
 int prev_hp,prev_army_dist,wave_was_engaged;uint32_t last_attacked;
 char events[700];int elen;uint32_t score_min;
} SGLLM;
static SGLLM sg_llm;
static const char*sg_llm_dir(void){static int init;static const char*d;if(!init){init=1;d=getenv("SG_LLM_DIR");if(d&&!*d)d=NULL;}return d;}
/* Only one house is commanded: the first Brutal Plus house on the top slot. */
static int sg_llm_house(uint32_t h){
 if(!sg_llm_dir()||(int32_t)MEM32(h+0x184)!=0)return 0;
 if(!sg_llm.house)sg_llm.house=h;return sg_llm.house==h;}
static int sg_llm_live(uint32_t h){return sg_llm_house(h)&&sg_llm.got&&MEM32(0xA8ED84)-sg_llm.got<900;}
static int sg_llm_is(uint32_t h,const char*field,const char*value){if(!sg_llm_live(h))return 0;
 const char*v=!strcmp(field,"stance")?sg_llm.stance:!strcmp(field,"target")?sg_llm.target:sg_llm.build;return !strcmp(v,value);}
static void sg_llm_event(const char*fmt,...){
 char e[160];va_list ap;va_start(ap,fmt);vsnprintf(e,sizeof e,fmt,ap);va_end(ap);
 for(char*c=e;*c;c++)if(*c=='"'||*c==92||*c<32)*c=' ';
 if(sg_llm.elen+(int)strlen(e)+4<(int)sizeof sg_llm.events)sg_llm.elen+=snprintf(sg_llm.events+sg_llm.elen,sizeof sg_llm.events-sg_llm.elen,"%s\"%s\"",sg_llm.elen?",":"",e);
 fprintf(stderr,"[llm] min=%u event: %s\n",MEM32(0xA8ED84)/900,e);}
static int sg_llm_keyword(const char*v,const char*const*list){for(int i=0;list[i];i++)if(!strcmp(v,list[i]))return 1;return 0;}
static const char*const SG_LLM_STANCES[]={"attack","defend","harass","expand","retreat",NULL};
static const char*const SG_LLM_TARGETS[]={"core","factory","refinery","conyard","army","defences",NULL};
static const char*const SG_LLM_BUILDS[]={"armour","infantry","antiair","artillery",NULL};
static int sg_llm_is_id(const char*v){if(strlen(v)<2||strlen(v)>14)return 0;for(const char*c=v;*c;c++)if(!((*c>='A'&&*c<='Z')||(*c>='0'&&*c<='9')||*c=='_'))return 0;return 1;}
static int sg_llm_is_defence(const char*id){static const char*d[]={"GAPILL","NALASR","TESLA","ATESLA","GAPRSM","NABNKR","YAGGUN","GATSEN","NAFLAK","GASAM","NASAM","YAPSYT","GAGAP","SGAEGIS","GAWALL","NAWALL",NULL};for(int i=0;d[i];i++)if(!strcmp(id,d[i]))return 1;return 0;}

/* ---- hooks used by Brutal Plus ---- */
static int sg_llm_need_pct(uint32_t h){if(!sg_llm_live(h))return 100;
 return sg_llm_is(h,"stance","attack")?60:sg_llm_is(h,"stance","harass")?40:sg_llm_is(h,"stance","defend")||sg_llm_is(h,"stance","retreat")?250:sg_llm_is(h,"stance","expand")?130:100;}
static int sg_llm_rank(uint32_t h,const char*id,int rank){if(!sg_llm_live(h)||!sg_llm.target[0])return rank;
 const char*t=sg_llm.target;
 if(!sg_llm_keyword(t,SG_LLM_TARGETS))return !strcmp(id,t)?-3:rank<0?rank:rank+2; /* a named building */
 if(rank<0&&!(!strcmp(t,"defences")&&sg_llm_is_defence(id)))return rank;
 int match=(!strcmp(t,"core")&&(!strncmp(id,"SGCOLO",6)||!strncmp(id,"SGNODE",6)))||(!strcmp(t,"factory")&&strstr(id,"WEAP"))||
  (!strcmp(t,"refinery")&&strstr(id,"REFN"))||(!strcmp(t,"conyard")&&strstr(id,"CNST"))||(!strcmp(t,"defences")&&sg_llm_is_defence(id));
 if(sg_llm_is(h,"stance","harass")&&strstr(id,"REFN"))match=1;
 return match?-2:rank+2; /* -2 beats every normal rank (0..3) */}
static int sg_llm_hunt_army(uint32_t h){return sg_llm_is(h,"target","army");}
static const char*sg_llm_build_id(uint32_t h){if(!sg_llm_live(h)||!sg_llm.build[0]||sg_llm_keyword(sg_llm.build,SG_LLM_BUILDS))return NULL;return sg_llm.build;}
static void sg_llm_wave(uint32_t h,int launched,int alive,int value){if(!sg_llm_house(h))return;
 if(launched){sg_llm.waves++;sg_llm.last_launch=launched;sg_llm.last_end=-1;sg_llm.last_alive=-1;sg_llm.wave_launch_f=MEM32(0xA8ED84);sg_llm.wave_was_engaged=0;sg_llm_event("our wave %d launched ($%d)",sg_llm.waves,launched);}
 else{sg_llm.last_end=value;sg_llm.last_alive=alive;sg_llm_event("our wave %d is over: $%d of $%d came back",sg_llm.waves,value,sg_llm.last_launch);}}
/* Scout and raid units are not available to the main army. */
static int sg_llm_reserved(uint32_t p){if(!sg_llm.house)return 0;if(p==sg_llm.scout)return 1;for(int i=0;i<sg_llm.rn;i++)if(p==sg_llm.raid[i])return 1;return 0;}

/* ---- small helpers ---- */
static int sg_llm_uid_ok(uint32_t p,uint32_t uid){return p>=0x10000&&sg_alive(p)&&MEM32(p+0x10)==uid;}
static int sg_llm_pos_visible(uint32_t h,int x,int y){
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p)||MEM32(p+0x21C)!=h)continue;
  uint32_t vt=MEM32(p);if(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E3EBC&&vt!=0x7E22A4)continue;
  int s=sg_ai_sight(sg_ai_id(p));double dx=x-(int32_t)MEM32(p+0x9C),dy=y-(int32_t)MEM32(p+0xA0);if(dx*dx+dy*dy<=(double)s*s*256*256*0.8)return 1;}
 return 0;}
static int sg_llm_cells(int dx,int dy){return (int)(sqrt((double)dx*dx+(double)dy*dy)/256);}
/* Compass direction as seen on screen (the map is drawn as a diamond). */
static const char*sg_llm_dir8(int dx,int dy){double sx=dx-dy,sy=dx+dy;if(fabs(sx)+fabs(sy)<1)return "here";
 static const char*n[]={"E","NE","N","NW","W","SW","S","SE"};double a=atan2(-sy,sx)*180/M_PI;int k=(int)floor((a+22.5+360)/45)%8;return n[k];}
static int sg_llm_sum(SGLLMLoss*r,uint32_t f,uint32_t since,int*count){int v=0,n=0;for(int i=0;i<128;i++)if(r[i].f&&r[i].f>=since&&r[i].f<=f){v+=r[i].cost;n++;}if(count)*count=n;return v;}
static void sg_llm_push(SGLLMLoss*r,int*n,uint32_t f,int cost){r[*n%128].f=f;r[*n%128].cost=cost;(*n)++;}
static int sg_llm_is_miner(const char*id){return !strcmp(id,"HARV")||!strcmp(id,"CMIN")||!strcmp(id,"SMIN");}
static int sg_llm_is_air(uint32_t q){const char*id=sg_ai_id(q);return MEM32(q)==0x7E22A4||MEM8(q+0x134)||sg_opdrone(id)||!strcmp(id,"SGSHAHED")||!strcmp(id,"SGLOCUST");}

/* ---- orders ---- */
static void sg_llm_json_str(const char*json,const char*key,char*out,size_t size){
 char pat[32];snprintf(pat,sizeof pat,"\"%s\"",key);const char*p=strstr(json,pat);out[0]=0;if(!p)return;
 p=strchr(p+strlen(pat),':');if(!p)return;while(*++p==' ');if(*p!='"')return;p++;
 size_t n=0;while(*p&&*p!='"'&&n+1<size){char c=*p++;out[n++]=(c<32||c==92)?' ':c;}out[n]=0;}
static int sg_llm_json_int(const char*json,const char*key){char pat[32];snprintf(pat,sizeof pat,"\"%s\"",key);const char*p=strstr(json,pat);if(!p)return 0;p=strchr(p+strlen(pat),':');return p?atoi(p+1):0;}
static void sg_llm_read_orders(uint32_t f){
 char path[600];snprintf(path,sizeof path,"%s/orders.json",sg_llm_dir());
 FILE*fp=fopen(path,"rb");if(!fp)return;char buf[1024];size_t n=fread(buf,1,sizeof buf-1,fp);fclose(fp);buf[n]=0;
 int q=sg_llm_json_int(buf,"seq");if(q<=0||q==sg_llm.seq)return;
 char stance[12],target[16],build[16],scout[8],note[96];
 sg_llm_json_str(buf,"stance",stance,sizeof stance);sg_llm_json_str(buf,"target",target,sizeof target);
 sg_llm_json_str(buf,"build",build,sizeof build);sg_llm_json_str(buf,"scout",scout,sizeof scout);sg_llm_json_str(buf,"note",note,sizeof note);
 int raid=sg_llm_json_int(buf,"raid");if(raid<0)raid=0;if(raid>8)raid=8;
 sg_llm.seq=q;
 if(!sg_llm_keyword(stance,SG_LLM_STANCES)){fprintf(stderr,"[llm] ignored orders #%d (bad stance '%s')\n",q,stance);return;}
 if(!sg_llm_keyword(target,SG_LLM_TARGETS)&&!sg_llm_is_id(target))target[0]=0;
 if(!sg_llm_keyword(build,SG_LLM_BUILDS)&&!sg_llm_is_id(build))build[0]=0;
 int changed=strcmp(stance,sg_llm.stance)||strcmp(target,sg_llm.target)||strcmp(build,sg_llm.build)||raid!=sg_llm.raid_n||!strcmp(scout,"now");
 memcpy(sg_llm.stance,stance,sizeof stance);memcpy(sg_llm.target,target,sizeof target);memcpy(sg_llm.build,build,sizeof build);memcpy(sg_llm.note,note,sizeof note);
 memcpy(sg_llm.scout_req,scout,sizeof scout);sg_llm.raid_n=raid;sg_llm.got=f;
 if(changed)fprintf(stderr,"[llm] min=%u orders #%d: stance=%s target=%s build=%s raid=%d scout=%s \"%s\"\n",f/900,q,stance,target[0]?target:"-",build[0]?build:"-",raid,scout[0]?scout:"-",note);
}

/* ---- memory of what we have seen ---- */
static void sg_llm_remember(uint32_t h,uint32_t f){
 /* 1. refresh everything in sight */
 for(int pass=0;pass<2;pass++){uint32_t vec=pass?0xA8EB40:0xA8EC78;
  for(int i=0;i<sg_vec_count(vec);i++){uint32_t p=sg_vec_at(vec,i);if(!sg_alive(p))continue;uint32_t vt=MEM32(p);
   if(pass==0&&vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E22A4)continue;
   if(!sg_enemy(h,MEM32(p+0x21C))||!sg_ai_visible(h,p))continue;
   uint32_t uid=MEM32(p+0x10);SGLLMSeen*c=NULL,*old=&sg_llm.seen[0];
   for(int j=0;j<384;j++){SGLLMSeen*s=&sg_llm.seen[j];if(s->ptr==p&&s->uid==uid){c=s;break;}
    if(!s->seen){if(old->seen)old=s;}else if(old->seen&&!s->building&&(old->building||s->seen<old->seen))old=s;}
   const char*id=sg_ai_id(p);
   if(!c){c=old;memset(c,0,sizeof *c);c->ptr=p;c->uid=uid;c->building=pass;c->house=MEM32(MEM32(p+0x21C)+0x30);c->cost=sg_bp_cost(id);
    size_t k=0;for(;id[k]&&k+1<sizeof c->id;k++){char ch=id[k];c->id[k]=(ch>32&&ch<127&&ch!='"'&&ch!=92)?ch:'_';}c->id[k]=0;
    int known=0;for(int t=0;t<sg_llm.ntypes;t++)if(!strcmp(sg_llm.types_seen[t],c->id)){known=1;break;}
    if(!known&&sg_llm.ntypes<96){snprintf(sg_llm.types_seen[sg_llm.ntypes++],24,"%s",c->id);if(f>90)sg_llm_event("first sighting of enemy %s %s",pass?"building":"unit",c->id);
     if(!strncmp(c->id,"SGCOLO",6)||!strncmp(c->id,"SGNODE",6))sg_llm_event("ENEMY SINGULARITY seen (%s): it wins the game if finished",c->id);}}
   c->seen=f;c->x=MEM32(p+0x9C);c->y=MEM32(p+0xA0);}}
 /* 2. forget what is gone: units after 2 minutes, buildings once we see the spot empty */
 for(int j=0;j<384;j++){SGLLMSeen*s=&sg_llm.seen[j];if(!s->seen||s->seen==f)continue;
  int alive=sg_llm_uid_ok(s->ptr,s->uid);
  if(!alive&&f-s->seen<150){sg_llm_push(sg_llm.kills,&sg_llm.nkills,f,s->cost);if(s->building)sg_llm_event("enemy %s destroyed",s->id);s->seen=0;continue;}
  if(s->building){if(sg_llm_pos_visible(h,s->x,s->y)&&!(alive&&sg_ai_visible(h,s->ptr)))s->seen=0;}
  else if(f-s->seen>1800)s->seen=0;}
}
/* ---- our own losses ---- */
static void sg_llm_own(uint32_t h,uint32_t f){
 for(int i=0;i<sg_llm.nown;i++){SGLLMOwn*o=&sg_llm.own[i];if(sg_llm_uid_ok(o->ptr,o->uid))continue;
  if(o->building){if(f>450)sg_llm_event("we lost a building: %s",o->id);}
  else{sg_llm_push(sg_llm.deaths,&sg_llm.ndeaths,f,o->cost);if(o->miner)sg_llm_event("we lost a miner");}}
 sg_llm.nown=0;
 for(int pass=0;pass<2;pass++){uint32_t vec=pass?0xA8EB40:0xA8EC78;
  for(int i=0;i<sg_vec_count(vec)&&sg_llm.nown<512;i++){uint32_t p=sg_vec_at(vec,i);if(!sg_alive(p)||MEM32(p+0x21C)!=h)continue;uint32_t vt=MEM32(p);
   if(pass==0&&vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E22A4)continue;
   SGLLMOwn*o=&sg_llm.own[sg_llm.nown++];const char*id=sg_ai_id(p);o->ptr=p;o->uid=MEM32(p+0x10);o->building=pass;o->miner=sg_llm_is_miner(id);o->cost=sg_bp_cost(id);snprintf(o->id,sizeof o->id,"%s",id);}}
}

/* ---- scouting ---- */
static void sg_llm_record_starts(uint32_t h){
 /* Adds any enemy not listed yet (alliances can change in the first seconds). */
 for(int i=0;i<sg_vec_count(0xA80228)&&sg_llm.nstarts<8;i++){uint32_t o=sg_vec_at(0xA80228,i);if(!sg_enemy(h,o)||MEM32(o+0x34)<0x10000||MEM8(MEM32(o+0x34)+0x1A6))continue;
  int have=0;for(int k=0;k<sg_llm.nstarts;k++)if(sg_llm.starts[k].house==(int)MEM32(o+0x30))have=1;if(have)continue;
  uint32_t first=0;for(int k=0;k<sg_vec_count(0xA8EC78);k++){uint32_t p=sg_vec_at(0xA8EC78,k);if(sg_alive(p)&&MEM32(p+0x21C)==o){first=p;if(MEM32(p)==0x7E3EBC||strstr(sg_ai_id(p),"MCV"))break;}}
  if(!first)continue;SGLLMStart*s=&sg_llm.starts[sg_llm.nstarts++];s->house=MEM32(o+0x30);s->x=MEM32(first+0x9C);s->y=MEM32(first+0xA0);s->scouted=0;}
}
static int sg_llm_scout_score(const char*id){static const char*fast[]={"ADOG","DOG","SGHOUND","FV","LTNK","SGLINK","HTK","MTNK","HTNK","E1","E2","INIT",NULL};for(int i=0;fast[i];i++)if(!strcmp(id,fast[i]))return i;return 50;}
static void sg_llm_scout(SGBPBrain*b,uint32_t f){
 uint32_t h=b->house;
 if(f%450<15){int before=sg_llm.nstarts;sg_llm_record_starts(h);if(sg_llm.nstarts>before)fprintf(stderr,"[llm] %d enemy start areas known\n",sg_llm.nstarts);}
 if(sg_llm.scout&&!sg_llm_uid_ok(sg_llm.scout,sg_llm.scout_uid)){sg_llm_event("our scout (%s) was killed near %s",sg_llm.scout_id,sg_llm.scout_where);sg_llm.scout=0;sg_llm.scout_lost_at=f;}
 int asked=!strcmp(sg_llm.scout_req,"now");if(asked)sg_llm.scout_req[0]=0;
 if(!sg_llm.scout){
  if(!asked&&(f<sg_llm.scout_next||(sg_llm.scout_lost_at&&f-sg_llm.scout_lost_at<450)))return;
  uint32_t best=0;int bs=1000;
  for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_bp_free(p,h)||sg_bp_in_wave(b,p))continue;int s=sg_llm_scout_score(sg_ai_id(p));if(s<bs){bs=s;best=p;}}
  if(!best)return;
  /* where to: first any enemy start area not checked yet, then whatever we know least recently */
  int goal=-1;double gd=1e30;for(int k=0;k<sg_llm.nstarts;k++)if(!sg_llm.starts[k].scouted){double d=sg_llm_cells(sg_llm.starts[k].x-b->sx,sg_llm.starts[k].y-b->sy);if(d<gd){gd=d;goal=k;}}
  int tx,ty;
  if(goal>=0){tx=sg_llm.starts[goal].x;ty=sg_llm.starts[goal].y;}
  else{uint32_t oldest=0xFFFFFFFF;int pick=-1;for(int k=0;k<sg_llm.nstarts;k++)if(sg_llm.starts[k].scouted<oldest){oldest=sg_llm.starts[k].scouted;pick=k;}
   if(pick<0)return;goal=pick;tx=sg_llm.starts[pick].x;ty=sg_llm.starts[pick].y;}
  sg_llm.scout=best;sg_llm.scout_uid=MEM32(best+0x10);sg_llm.scout_since=f;sg_llm.scout_goal=goal;sg_llm.scout_tx=tx;sg_llm.scout_ty=ty;snprintf(sg_llm.scout_id,sizeof sg_llm.scout_id,"%s",sg_ai_id(best));
  snprintf(sg_llm.scout_where,sizeof sg_llm.scout_where,"enemy start %s %d cells",sg_llm_dir8(tx-b->sx,ty-b->sy),sg_llm_cells(tx-b->sx,ty-b->sy));
  sg_bp_move(best,tx,ty);sg_llm_event("scout %s sent to %s",sg_llm.scout_id,sg_llm.scout_where);return;}
 uint32_t p=sg_llm.scout;int px=MEM32(p+0x9C),py=MEM32(p+0xA0);
 snprintf(sg_llm.scout_where,sizeof sg_llm.scout_where,"%s %d cells from our base",sg_llm_dir8(px-b->sx,py-b->sy),sg_llm_cells(px-b->sx,py-b->sy));
 if(sg_llm_cells(px-sg_llm.scout_tx,py-sg_llm.scout_ty)<6||f-sg_llm.scout_since>3600){
  SGLLMStart*s=&sg_llm.starts[sg_llm.scout_goal];s->scouted=f;int found=0;
  for(int j=0;j<384;j++){SGLLMSeen*e=&sg_llm.seen[j];if(e->seen&&e->building&&f-e->seen<300&&sg_llm_cells(e->x-s->x,e->y-s->y)<20)found++;}
  sg_llm_event(found?"scout reached enemy start %s: enemy base found (%d buildings)":"scout reached enemy start %s: empty",sg_llm_dir8(s->x-b->sx,s->y-b->sy),found);
  sg_bp_move(p,b->sx,b->sy);sg_llm.scout=0;
  int all=1;for(int k=0;k<sg_llm.nstarts;k++)if(!sg_llm.starts[k].scouted)all=0;
  sg_llm.scout_next=all?f+2700:f; /* re-scout every 3 minutes once everything is known */
 }else if(MEM32(p+0xAC)!=2&&(f-sg_llm.scout_since)%300<15)sg_bp_move(p,sg_llm.scout_tx,sg_llm.scout_ty);
}

/* ---- raids on enemy miners / refineries ---- */
static void sg_llm_raid(SGBPBrain*b,uint32_t f){
 uint32_t h=b->house;
 if(sg_llm.rn){int alive=0;for(int i=0;i<sg_llm.rn;i++)if(sg_llm_uid_ok(sg_llm.raid[i],sg_llm.raid_uid[i]))alive++;
  int tdead=!sg_llm_uid_ok(sg_llm.raid_target,sg_llm.raid_tuid);
  if(!alive||tdead){sg_llm_event(tdead?"raid finished: %s destroyed, %d raiders left":"raid on %s failed: all raiders lost",sg_llm.raid_tid,alive);
   for(int i=0;i<sg_llm.rn;i++)if(sg_llm_uid_ok(sg_llm.raid[i],sg_llm.raid_uid[i]))sg_bp_move(sg_llm.raid[i],b->sx,b->sy);sg_llm.rn=0;}
  else if(f%150<15)for(int i=0;i<sg_llm.rn;i++)if(sg_llm_uid_ok(sg_llm.raid[i],sg_llm.raid_uid[i])&&MEM32(sg_llm.raid[i]+0xAC)!=1)sg_bp_order(sg_llm.raid[i],sg_llm.raid_target,1);
  return;}
 if(!sg_llm_live(h)||sg_llm.raid_n<=0||sg_llm.raid_seq==sg_llm.seq)return;
 sg_llm.raid_seq=sg_llm.seq;
 /* target: a remembered enemy miner (seen in the last minute), else a remembered refinery */
 SGLLMSeen*best=NULL;double bd=1e30;
 for(int pass=0;pass<2&&!best;pass++)for(int j=0;j<384;j++){SGLLMSeen*s=&sg_llm.seen[j];if(!s->seen||!sg_llm_uid_ok(s->ptr,s->uid))continue;
  if(pass==0?!(sg_llm_is_miner(s->id)&&f-s->seen<900):!strstr(s->id,"REFN"))continue;double d=sg_llm_cells(s->x-b->sx,s->y-b->sy);if(d<bd){bd=d;best=s;}}
 if(!best){sg_llm_event("raid not started: we know no enemy miner or refinery yet (scout first)");return;}
 for(int i=0;i<sg_vec_count(0xA8EC78)&&sg_llm.rn<sg_llm.raid_n;i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_bp_free(p,h)||sg_bp_in_wave(b,p)||MEM32(p)!=0x7F5C70)continue;
  sg_llm.raid[sg_llm.rn]=p;sg_llm.raid_uid[sg_llm.rn++]=MEM32(p+0x10);}
 for(int i=0;i<sg_vec_count(0xA8EC78)&&sg_llm.rn<sg_llm.raid_n;i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_bp_free(p,h)||sg_bp_in_wave(b,p)||sg_llm_reserved(p))continue;
  sg_llm.raid[sg_llm.rn]=p;sg_llm.raid_uid[sg_llm.rn++]=MEM32(p+0x10);}
 if(!sg_llm.rn){sg_llm_event("raid not started: no free units");return;}
 sg_llm.raid_target=best->ptr;sg_llm.raid_tuid=best->uid;snprintf(sg_llm.raid_tid,sizeof sg_llm.raid_tid,"%s",best->id);
 for(int i=0;i<sg_llm.rn;i++)sg_bp_order(sg_llm.raid[i],best->ptr,1);
 sg_llm_event("raid of %d units sent at enemy %s %s %d cells",sg_llm.rn,best->id,sg_llm_dir8(best->x-b->sx,best->y-b->sy),sg_llm_cells(best->x-b->sx,best->y-b->sy));
}

/* ---- the ledger ---- */
typedef struct {char id[24];int n;} SGLLMTally;
static void sg_llm_tally(SGLLMTally*t,int*nt,int max,const char*id){for(int i=0;i<*nt;i++)if(!strcmp(t[i].id,id)){t[i].n++;return;}if(*nt<max){size_t k=0;for(;id[k]&&k+1<sizeof t[0].id;k++){char c=id[k];t[*nt].id[k]=(c>32&&c<127&&c!='"'&&c!=92)?c:'_';}t[*nt].id[k]=0;t[*nt].n=1;(*nt)++;}} /* type names go into JSON: plain ASCII only */
static int sg_llm_put_tally(char*o,int n,int size,const char*key,SGLLMTally*t,int nt){
 n+=snprintf(o+n,size-n,"\"%s\":{",key);for(int i=0;i<nt&&n<size-40;i++)n+=snprintf(o+n,size-n,"%s\"%s\":%d",i?",":"",t[i].id,t[i].n);return n+snprintf(o+n,size-n,"}");}
static void sg_llm_write(SGBPBrain*b,uint32_t f){
 uint32_t h=b->house;int fresh=!sg_llm.last_write;sg_llm.last_write=f;
 sg_llm_remember(h,f);sg_llm_own(h,f);
 SGLLMTally mu[40],mb[40],eu[40],eb[40];int nmu=0,nmb=0,neu=0,neb=0,army=0,value=0,free_n=0,eunits=0,evalue=0,near_base=0,near_ore=0,eair=0,harv=0;
 uint32_t refs[8];int nref=0;
 for(int i=0;i<sg_vec_count(0xA8EB40);i++){uint32_t q=sg_vec_at(0xA8EB40,i);if(!sg_alive(q))continue;const char*id=sg_ai_id(q);
  if(MEM32(q+0x21C)==h){sg_llm_tally(mb,&nmb,40,id);if(strstr(id,"REFN")&&nref<8)refs[nref++]=q;}
  else if(sg_enemy(h,MEM32(q+0x21C))&&sg_ai_visible(h,q))sg_llm_tally(eb,&neb,40,id);}
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p))continue;uint32_t vt=MEM32(p);if(vt==0x7E3EBC)continue;
  const char*id=sg_ai_id(p);
  if(MEM32(p+0x21C)==h){if(sg_llm_is_miner(id)){harv++;continue;}
   if(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E22A4)continue;sg_llm_tally(mu,&nmu,40,id);army++;value+=sg_bp_cost(id);if(sg_bp_free(p,h))free_n++;continue;}
  if(!sg_enemy(h,MEM32(p+0x21C))||(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E22A4)||!sg_ai_visible(h,p))continue;
  sg_llm_tally(eu,&neu,40,id);eunits++;evalue+=sg_bp_cost(id);if(sg_llm_is_air(p))eair++;
  int dx=(int32_t)MEM32(p+0x9C)-b->sx,dy=(int32_t)MEM32(p+0xA0)-b->sy;if(sg_llm_cells(dx,dy)<20)near_base++;
  for(int k=0;k<nref;k++)if(sg_llm_cells((int32_t)MEM32(p+0x9C)-(int32_t)MEM32(refs[k]+0x9C),(int32_t)MEM32(p+0xA0)-(int32_t)MEM32(refs[k]+0xA0))<10){near_ore++;break;}}
 static const char*states[]={"gather","march","assault"};
 static char o[16000];int S=sizeof o;
 int lost_n,lost=sg_llm_sum(sg_llm.deaths,f,f>900?f-900:0,&lost_n),kill_n,killed=sg_llm_sum(sg_llm.kills,f,f>900?f-900:0,&kill_n);
 int n=snprintf(o,S,"{\"frame\":%u,\"min\":%u,\"house\":%d,\"side\":\"%s\",\"credits\":%d,\"power\":[%d,%d],\"base_threat\":%d,"
  "\"army\":{\"units\":%d,\"value\":%d,\"free\":%d,\"harvesters\":%d},\"last_minute\":{\"lost_units\":%d,\"lost_value\":%d,\"killed_units\":%d,\"killed_value\":%d},",
  f,f/900,MEM32(h+0x30),MEM32(h+0x1E8)==1?"soviet":MEM32(h+0x1E8)==2?"yuri":"allied",(int32_t)MEM32(h+0x30C),(int32_t)MEM32(h+0x53A4),(int32_t)MEM32(h+0x53A8),sg_llm.threat,
  army,value,free_n,harv,lost_n,lost,kill_n,killed);
 /* the current wave */
 int wv=0,wa=0,eng=0,cx=0,cy=0;for(int i=0;i<b->wn;i++){uint32_t p=b->wave[i];if(!sg_alive(p)||MEM32(p+0x21C)!=h)continue;wa++;wv+=sg_bp_cost(sg_ai_id(p));cx+=MEM32(p+0x9C)/64;cy+=MEM32(p+0xA0)/64;if(sg_bp_engaged(h,p))eng++;}
 int wdist=-1;if(wa&&b->target&&sg_alive(b->target)){cx=cx/wa*64;cy=cy/wa*64;wdist=sg_llm_cells(cx-(int32_t)MEM32(b->target+0x9C),cy-(int32_t)MEM32(b->target+0xA0));}
 int wl,wlost=sg_llm.wave_launch_f?sg_llm_sum(sg_llm.deaths,f,sg_llm.wave_launch_f,&wl):0,wk,wkill=sg_llm.wave_launch_f?sg_llm_sum(sg_llm.kills,f,sg_llm.wave_launch_f,&wk):0;
 n+=snprintf(o+n,S-n,"\"wave\":{\"state\":\"%s\",\"number\":%d,\"units\":%d,\"value\":%d,\"launched_value\":%d,\"target\":\"%s\",\"cells_to_target\":%d,\"fighting\":%d,\"killed_since_launch\":%d,\"lost_since_launch\":%d},"
  "\"last_wave\":{\"launched_value\":%d,\"end_value\":%d,\"survivors\":%d},",
  states[b->state<3?b->state:0],sg_llm.waves,wa,wv,b->state?b->launch_value:0,b->target&&sg_alive(b->target)?sg_ai_id(b->target):"",wdist,eng,b->state?wkill:0,b->state?wlost:0,
  sg_llm.last_launch,sg_llm.last_end,sg_llm.last_alive);
 /* scout and raid */
 n+=snprintf(o+n,S-n,"\"scout\":{\"unit\":\"%s\",\"status\":\"%s\",\"where\":\"%s\"},",sg_llm.scout?sg_llm.scout_id:"",sg_llm.scout?"travelling":sg_llm.scout_lost_at&&f-sg_llm.scout_lost_at<900?"lost":"home",sg_llm.scout?sg_llm.scout_where:"");
 int ralive=0;for(int i=0;i<sg_llm.rn;i++)if(sg_llm_uid_ok(sg_llm.raid[i],sg_llm.raid_uid[i]))ralive++;
 n+=snprintf(o+n,S-n,"\"raid\":{\"units\":%d,\"target\":\"%s\"},",ralive,sg_llm.rn?sg_llm.raid_tid:"");
 n=sg_llm_put_tally(o,n,S,"my_units",mu,nmu);n+=snprintf(o+n,S-n,",");n=sg_llm_put_tally(o,n,S,"my_buildings",mb,nmb);
 /* what we can build right now (for named build orders) */
 static const char*cand[]={"E1","E2","GGI","FLAKT","SGOP","SGLINK","INIT","BRUTE","MTNK","FV","SREF","TNKD","SGTANK","SGEMPT","HTNK","HTK","V3","APOC","SGSHAHED","SGTANKC","LTNK","YTNK",NULL};
 n+=snprintf(o+n,S-n,",\"can_build\":[");int nc=0;for(int i=0;cand[i];i++)if(sg_bp_eligible(h,cand[i]))n+=snprintf(o+n,S-n,"%s\"%s\"",nc++?",":"",cand[i]);n+=snprintf(o+n,S-n,"]");
 /* in sight right now */
 n+=snprintf(o+n,S-n,",\"in_sight\":{\"units\":%d,\"value\":%d,\"near_my_base\":%d,\"near_my_ore\":%d,\"air_or_drones\":%d,",eunits,evalue,near_base,near_ore,eair);
 n=sg_llm_put_tally(o,n,S,"unit_types",eu,neu);n+=snprintf(o+n,S-n,",");n=sg_llm_put_tally(o,n,S,"buildings",eb,neb);n+=snprintf(o+n,S-n,"}");
 /* remembered enemy army (units seen in the last minute) */
 int ku=0,kv=0,kair=0,kx=0,ky=0;uint32_t newest=0;SGLLMTally kt[40];int nkt=0;
 for(int j=0;j<384;j++){SGLLMSeen*s=&sg_llm.seen[j];if(!s->seen||s->building||f-s->seen>900||sg_llm_is_miner(s->id))continue;ku++;kv+=s->cost;kx+=s->x/64;ky+=s->y/64;if(s->seen>newest)newest=s->seen;sg_llm_tally(kt,&nkt,40,s->id);
  if(sg_opdrone(s->id)||!strcmp(s->id,"SGSHAHED")||!strcmp(s->id,"SGLOCUST")||!strcmp(s->id,"ZEP")||!strcmp(s->id,"ORCA")||!strcmp(s->id,"BEAG")||!strcmp(s->id,"SGB2")||!strcmp(s->id,"DISK"))kair++;}
 const char*heading="unknown";int kd=-1;const char*kdir="";
 if(ku){kx=kx/ku*64;ky=ky/ku*64;kd=sg_llm_cells(kx-b->sx,ky-b->sy);kdir=sg_llm_dir8(kx-b->sx,ky-b->sy);
  heading=sg_llm.prev_army_dist<0?"unknown":kd<sg_llm.prev_army_dist-2?"toward our base":kd>sg_llm.prev_army_dist+2?"away from our base":"holding";sg_llm.prev_army_dist=kd;}else sg_llm.prev_army_dist=-1;
 n+=snprintf(o+n,S-n,",\"enemy_army_known\":{\"units\":%d,\"value\":%d,\"air_or_drones\":%d,\"seconds_since_seen\":%d,\"direction\":\"%s\",\"cells_from_our_base\":%d,\"heading\":\"%s\",",
  ku,kv,kair,ku?(int)(f-newest)/15:-1,kdir,kd,heading);
 n=sg_llm_put_tally(o,n,S,"types",kt,nkt);n+=snprintf(o+n,S-n,"}");
 /* remembered enemy bases */
 n+=snprintf(o+n,S-n,",\"enemy_bases\":[");
 for(int k=0;k<sg_llm.nstarts;k++){SGLLMStart*st=&sg_llm.starts[k];SGLLMTally bt[40];int nbt=0,defs=0,air=0,sing=0,drones=0,navy=0;uint32_t last=0;
  for(int j=0;j<384;j++){SGLLMSeen*s=&sg_llm.seen[j];if(!s->seen||s->house!=st->house)continue;
   if(!s->building){if(sg_opdrone(s->id)||!strcmp(s->id,"SGSHAHED"))drones=1;continue;}
   sg_llm_tally(bt,&nbt,40,s->id);if(s->seen>last)last=s->seen;if(sg_llm_is_defence(s->id))defs++;
   if(strstr(s->id,"AIRC")||strstr(s->id,"AFLD")||!strcmp(s->id,"NARADR"))air=1;if(!strncmp(s->id,"SGCOLO",6)||!strncmp(s->id,"SGNODE",6))sing=1;if(!strcmp(s->id,"SGFACT"))drones=1;if(strstr(s->id,"YARD"))navy=1;}
  n+=snprintf(o+n,S-n,"%s{\"house\":%d,\"direction\":\"%s\",\"cells_from_our_base\":%d,\"scouted\":%s,\"minutes_since_seen\":%d,\"defences\":%d,\"has_air\":%d,\"has_singularity\":%d,\"has_drones\":%d,\"has_navy\":%d,",
   k?",":"",st->house,sg_llm_dir8(st->x-b->sx,st->y-b->sy),sg_llm_cells(st->x-b->sx,st->y-b->sy),st->scouted?"true":"false",last?(int)(f-last)/900:-1,defs,air,sing,drones,navy);
  n=sg_llm_put_tally(o,n,S,"buildings",bt,nbt);n+=snprintf(o+n,S-n,"}");}
 n+=snprintf(o+n,S-n,"],\"events\":[%s],\"current_orders\":{\"seq\":%d,\"stance\":\"%s\",\"target\":\"%s\",\"build\":\"%s\",\"raid\":%d,\"live\":%d}}\n",
  sg_llm.events,sg_llm.seq,sg_llm.stance,sg_llm.target,sg_llm.build,sg_llm.raid_n,sg_llm_live(h));
 sg_llm.elen=0;sg_llm.events[0]=0;
 if(n>=S-1)return;
 char path[600],tmp[620];snprintf(path,sizeof path,"%s/ledger.json",sg_llm_dir());snprintf(tmp,sizeof tmp,"%s.tmp",path);
 FILE*fp=fopen(tmp,"wb");if(fp){fwrite(o,1,n,fp);fclose(fp);rename(tmp,path);}
 snprintf(path,sizeof path,"%s/match.jsonl",sg_llm_dir());if(fresh&&(fp=fopen(path,"wb")))fclose(fp); /* new match: fresh log */
 if((fp=fopen(path,"ab"))){fwrite(o,1,n,fp);fclose(fp);}
 if(f%900<45)fprintf(stderr,"[llm] min=%u ledger: army %d ($%d), in sight %d enemy units, remembers %d enemy units, %d bases (%d scouted), orders %s\n",f/900,army,value,eunits,ku,sg_llm.nstarts,
  ({int c=0;for(int k=0;k<sg_llm.nstarts;k++)c+=sg_llm.starts[k].scouted!=0;c;}),sg_llm_live(h)?sg_llm.stance:"none (playing on its own)");
}

/* Once a minute, the full truth for the after-match review only (like the
 * score screen): every player's buildings, units and army value. The live
 * commander never reads this file. */
static void sg_llm_score(uint32_t h,uint32_t f){
 if(f/900==sg_llm.score_min&&sg_llm.score_min)return;sg_llm.score_min=f/900;
 char line[1600];int n=snprintf(line,sizeof line,"{\"min\":%u,\"frame\":%u,\"players\":[",f/900,f);int first=1;
 for(int i=0;i<sg_vec_count(0xA80228);i++){uint32_t o=sg_vec_at(0xA80228,i);if(MEM32(o+0x34)<0x10000||MEM8(MEM32(o+0x34)+0x1A6))continue;
  int nb=0,nu=0,val=0;for(int k=0;k<sg_vec_count(0xA8EC78);k++){uint32_t q=sg_vec_at(0xA8EC78,k);if(!sg_alive(q)||MEM32(q+0x21C)!=o)continue;if(MEM32(q)==0x7E3EBC)nb++;else{nu++;val+=sg_bp_cost(sg_ai_id(q));}}
  const char*who=o==h?"qwen":o==MEM32(0xA83D4C)?"human":sg_bp_house(o)?"brutalplus":"classic";
  n+=snprintf(line+n,sizeof line-n,"%s{\"house\":%d,\"who\":\"%s\",\"enemy\":%d,\"buildings\":%d,\"units\":%d,\"value\":%d,\"credits\":%d}",first?"":",",MEM32(o+0x30),who,o!=h&&sg_enemy(h,o),nb,nu,val,(int32_t)MEM32(o+0x30C));first=0;
  if(n>=(int)sizeof line-200)break;}
 n+=snprintf(line+n,sizeof line-n,"]}\n");
 char path[600];snprintf(path,sizeof path,"%s/score.jsonl",sg_llm_dir());FILE*fp=fopen(path,f<1800?"wb":"ab");if(fp){fwrite(line,1,n,fp);fclose(fp);}
}
/* Every frame (cheap checks spread out): read orders, notice events, move the
 * scout and raiders, and write an extra ledger at once when something happened. */
static void sg_llm_frame(SGBPBrain*b,uint32_t f){
 uint32_t h=b->house;if(!sg_llm_house(h)||!b->sx)return;
 if(f<sg_llm.last_frame||!sg_llm.last_write&&!sg_llm.nstarts){uint32_t keep=sg_llm.house;memset(&sg_llm,0,sizeof sg_llm);sg_llm.house=keep;sg_llm.prev_army_dist=-1;sg_llm_record_starts(h);
  fprintf(stderr,"[llm] new match: %d enemy start areas known from the map\n",sg_llm.nstarts);}
 sg_llm.last_frame=f;
 if(f%8==0)sg_llm_read_orders(f);
 if(f%900==0&&f)sg_llm_score(h,f);
 if(f%15==0){
  int hp=0;for(int i=0;i<sg_vec_count(0xA8EB40);i++){uint32_t q=sg_vec_at(0xA8EB40,i);if(sg_alive(q)&&MEM32(q+0x21C)==h)hp+=MEM32(q+0x6C);}
  if(sg_llm.prev_hp&&hp<sg_llm.prev_hp-50&&f-sg_llm.last_attacked>300){sg_llm.last_attacked=f;sg_llm_event("our base is being attacked");}
  sg_llm.prev_hp=hp;
  if(b->state&&!sg_llm.wave_was_engaged){int e=0;for(int i=0;i<b->wn;i++)if(sg_alive(b->wave[i])&&sg_bp_engaged(h,b->wave[i]))e++;if(e>=2){sg_llm.wave_was_engaged=1;sg_llm_event("our wave is now fighting (%d units engaged)",e);}}
  sg_llm_scout(b,f);sg_llm_raid(b,f);}
 if(sg_llm.elen&&sg_llm.last_write&&f-sg_llm.last_write>=15)sg_llm_write(b,f);
}
static void sg_llm_tick(SGBPBrain*b,uint32_t f,int threat){
 uint32_t h=b->house;if(!sg_llm_house(h))return;sg_llm.threat=threat;
 if(sg_llm.last_write&&f-sg_llm.last_write<45)return;
 sg_llm_write(b,f);
}
