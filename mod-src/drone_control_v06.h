/* Individually controllable drones. Uses the engine's UnitType spawn routine;
   each owned slot has an independent replacement deadline. */
static uint32_t sg_find_unit_type(const char *id){
 for(int i=0;i<sg_vec_count(0xA83CE0);i++){uint32_t t=sg_vec_at(0xA83CE0,i);if(t>0x10000&&!strcmp((const char*)hle_ptr(t+0x24),id))return t;}return 0;
}
#ifndef SG_STATE_TEST
static uint32_t sg_make_drone(uint32_t parent,int slot){
 uint32_t t=sg_find_unit_type("SGPDRONE");if(!t)return 0;
 int16_t *cell=hle_heap_alloc(4,1);if(!cell)return 0;
 static const int offsets[8][2]={{1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,-1},{1,-1},{-1,1}};
 cell[0]=(int16_t)((int32_t)MEM32(parent+0x9C)/256+offsets[slot%8][0]);
 cell[1]=(int16_t)((int32_t)MEM32(parent+0xA0)/256+offsets[slot%8][1]);
 uint32_t args[2]={hle_va(cell),MEM32(parent+0x21C)},saved=g_ecx;
 g_ecx=t;uint32_t ok=hle_call_guest(MEM32(MEM32(t)+0x80),2,args);g_ecx=saved;hle_heap_free(cell);
 if(!ok)return 0;
 for(int i=sg_vec_count(0xA8EC78)-1;i>=0;i--){uint32_t p=sg_vec_at(0xA8EC78,i);
  if(sg_alive(p)&&MEM32(p)==0x7F5C70&&!strcmp(sg_id(p,0),"SGPDRONE")&&MEM32(p+0x21C)==MEM32(parent+0x21C)){
   SGUnit*u=sg_state(p);if(u&&!u->controller){u->controller=parent;u->controller_uid=MEM32(parent+0x10);return p;}
  }
 }
 return 0;
}
#else
static uint32_t sg_make_drone(uint32_t parent,int slot){(void)parent;(void)slot;return 0;}
#endif
static float sg_rank(uint32_t p){float v;memcpy(&v,hle_ptr(p+0x150),4);return isfinite(v)&&v>=0&&v<100?v:0;}
static int sg_capacity(uint32_t p,SGUnit*u){const char*id=sg_id(p,0);if(!strcmp(id,"SGTANK"))return u->nest_active?5:0;float rank=sg_rank(p);return rank>=2?3:rank>=1?2:1;}
static uint32_t sg_regen(uint32_t p){float rank=sg_rank(p);return rank>=2?225:rank>=1?330:450;}
static void sg_control_tick(uint32_t f){
 int n=sg_vec_count(0xA8EC78);
 for(int i=0;i<n;i++){
  uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p))continue;
  uint32_t vt=MEM32(p);if(vt!=0x7F5C70&&vt!=0x7EB058)continue;
  const char*id=sg_id(p,0);if(strcmp(id,"SGLINK")&&strcmp(id,"SGOP")&&strcmp(id,"SGTANK"))continue;
  SGUnit*u=sg_state(p);if(!u)continue;int capacity=sg_capacity(p,u);uint32_t regen=sg_regen(p);
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
