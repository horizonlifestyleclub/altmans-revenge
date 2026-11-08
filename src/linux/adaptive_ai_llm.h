/* LLM Commander for Brutal Plus (opt-in: SG_LLM_DIR=<folder>).
 *
 * Every 3 seconds the top Brutal Plus house writes a "ledger" (what that AI
 * can actually see: its own army and base, plus enemy units and buildings
 * inside its units' sight) to <dir>/ledger.json and appends it to
 * <dir>/match.jsonl. llm-bridge/commander.py reads it, asks a local model
 * (Ollama) for a stance, and writes <dir>/orders.json:
 *   {"seq":7,"stance":"attack|defend|harass|expand",
 *    "target":"core|factory|refinery|conyard|army",
 *    "build":"armour|infantry|antiair|artillery","note":"..."}
 * The game never waits for the model: orders are read only when the file
 * changes, and they expire after 60 seconds, after which Brutal Plus plays
 * on its own rules again. */
#include <sys/stat.h>
typedef struct {uint32_t house,got,last_write;long mtime;int seq,applied_seq;char stance[12],target[12],build[12],note[96];
 int waves,last_launch,last_end,last_alive;} SGLLM;
static SGLLM sg_llm;
static const char*sg_llm_dir(void){static int init;static const char*d;if(!init){init=1;d=getenv("SG_LLM_DIR");if(d&&!*d)d=NULL;}return d;}
/* Only one house is commanded: the first Brutal Plus house on the top slot. */
static int sg_llm_house(uint32_t h){
 if(!sg_llm_dir()||(int32_t)MEM32(h+0x184)!=0)return 0;
 if(!sg_llm.house)sg_llm.house=h;return sg_llm.house==h;}
static int sg_llm_live(uint32_t h){return sg_llm_house(h)&&sg_llm.got&&MEM32(0xA8ED84)-sg_llm.got<900;}
static int sg_llm_is(uint32_t h,const char*field,const char*value){if(!sg_llm_live(h))return 0;
 const char*v=!strcmp(field,"stance")?sg_llm.stance:!strcmp(field,"target")?sg_llm.target:sg_llm.build;return !strcmp(v,value);}
/* Hooks used by Brutal Plus. */
static int sg_llm_need_pct(uint32_t h){if(!sg_llm_live(h))return 100;
 return sg_llm_is(h,"stance","attack")?60:sg_llm_is(h,"stance","harass")?40:sg_llm_is(h,"stance","defend")?250:sg_llm_is(h,"stance","expand")?130:100;}
static int sg_llm_rank(uint32_t h,const char*id,int rank){if(!sg_llm_live(h)||!sg_llm.target[0]||rank<0)return rank;
 const char*t=sg_llm.target;int match=(!strcmp(t,"core")&&!strncmp(id,"SG",2))||(!strcmp(t,"factory")&&strstr(id,"WEAP"))||
  (!strcmp(t,"refinery")&&strstr(id,"REFN"))||(!strcmp(t,"conyard")&&strstr(id,"CNST"));
 if(sg_llm_is(h,"stance","harass")&&strstr(id,"REFN"))match=1;
 return match?-2:rank+2; /* -2 beats every normal rank (0..3) */}
static int sg_llm_hunt_army(uint32_t h){return sg_llm_is(h,"target","army");}
static void sg_llm_wave(uint32_t h,int launched,int alive,int value){if(!sg_llm_house(h))return;
 if(launched){sg_llm.waves++;sg_llm.last_launch=launched;sg_llm.last_end=-1;sg_llm.last_alive=-1;}else{sg_llm.last_end=value;sg_llm.last_alive=alive;}}

static void sg_llm_json_str(const char*json,const char*key,char*out,size_t size){
 char pat[32];snprintf(pat,sizeof pat,"\"%s\"",key);const char*p=strstr(json,pat);out[0]=0;if(!p)return;
 p=strchr(p+strlen(pat),':');if(!p)return;while(*++p==' ');if(*p!='"')return;p++;
 size_t n=0;while(*p&&*p!='"'&&n+1<size){char c=*p++;out[n++]=(c<32||c==92)?' ':c;}out[n]=0;}
static void sg_llm_read_orders(uint32_t f){
 char path[600];snprintf(path,sizeof path,"%s/orders.json",sg_llm_dir());struct stat st;if(stat(path,&st)||st.st_mtime==sg_llm.mtime)return;
 FILE*fp=fopen(path,"rb");if(!fp)return;char buf[1024];size_t n=fread(buf,1,sizeof buf-1,fp);fclose(fp);buf[n]=0;sg_llm.mtime=st.st_mtime;
 char seq[16];const char*s=strstr(buf,"\"seq\"");int q=s?atoi(strchr(s,':')?strchr(s,':')+1:"0"):0;(void)seq;
 char stance[12],target[12],build[12],note[96];
 sg_llm_json_str(buf,"stance",stance,sizeof stance);sg_llm_json_str(buf,"target",target,sizeof target);
 sg_llm_json_str(buf,"build",build,sizeof build);sg_llm_json_str(buf,"note",note,sizeof note);
 static const char*ok_s[]={"attack","defend","harass","expand"},*ok_t[]={"core","factory","refinery","conyard","army"},*ok_b[]={"armour","infantry","antiair","artillery"};
 int a=0,b=0,c=0;for(int i=0;i<4;i++)a|=!strcmp(stance,ok_s[i]);for(int i=0;i<5;i++)b|=!strcmp(target,ok_t[i]);for(int i=0;i<4;i++)c|=!strcmp(build,ok_b[i]);
 if(!a){fprintf(stderr,"[llm] ignored orders (bad stance '%s')\n",stance);return;}
 if(!b)target[0]=0;if(!c)build[0]=0;
 int changed=strcmp(stance,sg_llm.stance)||strcmp(target,sg_llm.target)||strcmp(build,sg_llm.build);
 memcpy(sg_llm.stance,stance,sizeof stance);memcpy(sg_llm.target,target,sizeof target);memcpy(sg_llm.build,build,sizeof build);memcpy(sg_llm.note,note,sizeof note);
 sg_llm.got=f;sg_llm.seq=q;
 if(changed)fprintf(stderr,"[llm] min=%u orders #%d: stance=%s target=%s build=%s \"%s\"\n",f/900,q,stance,target[0]?target:"-",build[0]?build:"-",note);
}

/* Tiny "type -> count" tally for the ledger. */
typedef struct {char id[24];int n;} SGLLMTally;
static void sg_llm_tally(SGLLMTally*t,int*nt,int max,const char*id){for(int i=0;i<*nt;i++)if(!strcmp(t[i].id,id)){t[i].n++;return;}if(*nt<max){size_t k=0;for(;id[k]&&k+1<sizeof t[0].id;k++){char c=id[k];t[*nt].id[k]=(c>32&&c<127&&c!='"'&&c!=92)?c:'_';}t[*nt].id[k]=0;t[*nt].n=1;(*nt)++;}} /* type names go into JSON: plain ASCII only */
static int sg_llm_put_tally(char*o,int n,int size,const char*key,SGLLMTally*t,int nt){
 n+=snprintf(o+n,size-n,"\"%s\":{",key);for(int i=0;i<nt&&n<size-40;i++)n+=snprintf(o+n,size-n,"%s\"%s\":%d",i?",":"",t[i].id,t[i].n);return n+snprintf(o+n,size-n,"}");}

static void sg_llm_tick(SGBPBrain*b,uint32_t f,int threat){
 uint32_t h=b->house;if(!sg_llm_house(h))return;
 int fresh=!sg_llm.last_write||f<sg_llm.last_write; /* first ledger of a match */
 if(fresh&&sg_llm.last_write){memset(&sg_llm,0,sizeof sg_llm);sg_llm.house=h;}
 sg_llm_read_orders(f);
 if(!fresh&&f-sg_llm.last_write<45)return;sg_llm.last_write=f;
 SGLLMTally mu[40],mb[40],eu[40],eb[40];int nmu=0,nmb=0,neu=0,neb=0,army=0,value=0,free_n=0,eunits=0,evalue=0,near_base=0,near_ore=0,drones=0,eair=0,harv=0;
 uint32_t refs[8];int nref=0;
 for(int i=0;i<sg_vec_count(0xA8EB40);i++){uint32_t q=sg_vec_at(0xA8EB40,i);if(!sg_alive(q))continue;const char*id=sg_ai_id(q);
  if(MEM32(q+0x21C)==h){sg_llm_tally(mb,&nmb,40,id);if(strstr(id,"REFN")&&nref<8)refs[nref++]=q;}
  else if(sg_enemy(h,MEM32(q+0x21C))&&sg_ai_visible(h,q))sg_llm_tally(eb,&neb,40,id);}
 for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p))continue;uint32_t vt=MEM32(p);if(vt==0x7E3EBC)continue;
  const char*id=sg_ai_id(p);
  if(MEM32(p+0x21C)==h){if(!strcmp(id,"HARV")||!strcmp(id,"CMIN")||!strcmp(id,"SMIN")){harv++;continue;}
   if(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E22A4)continue;sg_llm_tally(mu,&nmu,40,id);army++;value+=sg_bp_cost(id);if(sg_bp_free(p,h))free_n++;continue;}
  if(!sg_enemy(h,MEM32(p+0x21C))||(vt!=0x7F5C70&&vt!=0x7EB058&&vt!=0x7E22A4)||!sg_ai_visible(h,p))continue;
  sg_llm_tally(eu,&neu,40,id);eunits++;evalue+=sg_bp_cost(id);
  if(vt==0x7E22A4||MEM8(p+0x134)){eair++;if(strstr(id,"FPV")||strstr(id,"SHAHED")||strstr(id,"FIBRE")||strstr(id,"LOCUST")||strstr(id,"DRONE"))drones++;}
  double dx=(int32_t)MEM32(p+0x9C)-b->sx,dy=(int32_t)MEM32(p+0xA0)-b->sy;if(dx*dx+dy*dy<20.*20*256*256)near_base++;
  for(int k=0;k<nref;k++){double rx=(int32_t)MEM32(p+0x9C)-(int32_t)MEM32(refs[k]+0x9C),ry=(int32_t)MEM32(p+0xA0)-(int32_t)MEM32(refs[k]+0xA0);if(rx*rx+ry*ry<10.*10*256*256){near_ore++;break;}}}
 static const char*states[]={"gather","march","assault"};
 char o[6000];int n=snprintf(o,sizeof o,"{\"frame\":%u,\"min\":%u,\"house\":%d,\"side\":\"%s\",\"credits\":%d,\"power\":[%d,%d],\"army_state\":\"%s\",\"wave_units\":%d,\"waves\":%d,"
  "\"last_wave\":{\"launched_value\":%d,\"end_value\":%d,\"survivors\":%d},\"base_threat\":%d,\"army\":{\"units\":%d,\"value\":%d,\"free\":%d,\"harvesters\":%d},",
  f,f/900,MEM32(h+0x30),MEM32(h+0x1E8)==1?"soviet":MEM32(h+0x1E8)==2?"yuri":"allied",(int32_t)MEM32(h+0x30C),(int32_t)MEM32(h+0x53A4),(int32_t)MEM32(h+0x53A8),
  states[b->state<3?b->state:0],b->wn,sg_llm.waves,sg_llm.last_launch,sg_llm.last_end,sg_llm.last_alive,threat,army,value,free_n,harv);
 n=sg_llm_put_tally(o,n,sizeof o,"my_units",mu,nmu);n+=snprintf(o+n,sizeof o-n,",");
 n=sg_llm_put_tally(o,n,sizeof o,"my_buildings",mb,nmb);
 n+=snprintf(o+n,sizeof o-n,",\"seen_enemy\":{\"units\":%d,\"value\":%d,\"near_my_base\":%d,\"near_my_ore\":%d,\"air\":%d,\"drones\":%d,",eunits,evalue,near_base,near_ore,eair,drones);
 n=sg_llm_put_tally(o,n,sizeof o,"unit_types",eu,neu);n+=snprintf(o+n,sizeof o-n,",");
 n=sg_llm_put_tally(o,n,sizeof o,"buildings",eb,neb);
 n+=snprintf(o+n,sizeof o-n,"},\"current_orders\":{\"seq\":%d,\"stance\":\"%s\",\"target\":\"%s\",\"build\":\"%s\",\"live\":%d}}\n",
  sg_llm.seq,sg_llm.stance,sg_llm.target,sg_llm.build,sg_llm_live(h));
 if(n>=(int)sizeof o)return;
 char path[600],tmp[620];snprintf(path,sizeof path,"%s/ledger.json",sg_llm_dir());snprintf(tmp,sizeof tmp,"%s.tmp",path);
 FILE*fp=fopen(tmp,"wb");if(fp){fwrite(o,1,n,fp);fclose(fp);rename(tmp,path);}
 snprintf(path,sizeof path,"%s/match.jsonl",sg_llm_dir());if(fresh&&(fp=fopen(path,"wb")))fclose(fp); /* new match: fresh log */
 if((fp=fopen(path,"ab"))){fwrite(o,1,n,fp);fclose(fp);}
 if(f%900<45)fprintf(stderr,"[llm] min=%u ledger: army %d ($%d), sees %d enemy units, %d enemy buildings, orders %s\n",f/900,army,value,eunits,neb,sg_llm_live(h)?sg_llm.stance:"none (playing on its own)");
}
