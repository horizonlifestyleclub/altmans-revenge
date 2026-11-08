static void sg_v07_fail(const char*s){fprintf(stderr,"[v07-lab] FAIL %s\n",s);fflush(stderr);_exit(6);}
static uint32_t sg_v07_spawn(const char*id,uint32_t vector,uint32_t house,int x,int y){
 uint32_t type=0;for(int i=0;i<sg_vec_count(vector);i++){uint32_t t=sg_vec_at(vector,i);if(!strcmp((char*)hle_ptr(t+0x24),id)){type=t;break;}}if(!type)return 0;
 uint32_t saved=g_ecx;g_ecx=type;uint32_t p=hle_call_guest(MEM32(MEM32(type)+0x8C),1,&house);g_ecx=saved;if(!p)return 0;
 int32_t*c=hle_heap_alloc(12,1);uint32_t args[2]={hle_va(c),0};int ok=0;
 for(int k=0;k<32&&!ok;k++){c[0]=x+(k%8)*256;c[1]=y+(k/8)*256;c[2]=0;uint32_t cc=hle_va(c);g_ecx=0x87F7E8;c[2]=hle_call_guest(0x578080,1,&cc);g_ecx=p;ok=hle_call_guest(MEM32(MEM32(p)+0xD8),2,args);g_ecx=saved;}
 hle_heap_free(c);if(!ok){g_ecx=p;hle_call_guest(0x5F65F0,0,NULL);g_ecx=saved;return 0;}return p;
}
#ifdef SG_BRIDGE_LAB
#include "lab_bridge_v07.h"
#else
static void sg_v07_enter(uint32_t p,uint32_t target){uint32_t saved=g_ecx,args[2]={7,1};g_ecx=p;hle_call_guest(0x5B35E0,2,args);args[0]=target;args[1]=1;g_ecx=p;hle_call_guest(MEM32(MEM32(p)+0x480),2,args);g_ecx=saved;}
static void sg_v07_lab(uint32_t f){
 static int stage;static uint32_t transport,mcv,pilot,ggi,building,stamp;
 uint32_t house=MEM32(0xA83D4C),saved=g_ecx;
 if(f<60)return;
 if(stage==0){
  for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(sg_alive(p)&&MEM32(p+0x21C)==house&&MEM32(p)==0x7F5C70&&!strcmp(sg_id(p,0),"AMCV")){mcv=p;break;}}
  if(!mcv)sg_v07_fail("missing MCV");int x=MEM32(mcv+0x9C),y=MEM32(mcv+0xA0);
  transport=sg_v07_spawn("LCRF",0xA83CE0,house,x+3*256,y);if(!transport)sg_v07_fail("transport placement");
  uint32_t args[2]={transport,1};g_ecx=mcv;int action=hle_call_guest(0x73FD50,2,args);g_ecx=saved;fprintf(stderr,"[v07-lab] MCV hover action=%d (3 means Enter)\n",action);
  sg_v07_enter(mcv,transport);stamp=f;stage=1;
 }else if(stage==1){
  if(MEM32(transport+0x114)>0&&MEM32(mcv+0x11C)==transport){fprintf(stderr,"[v07-lab] PASS MCV boarded amphibious transport\n");stage=2;}
  else if(f>stamp+600){fprintf(stderr,"[v07-lab] MCV mission=%d dest=%08X transport=%08X distance=%.2f passenger=%08X\n",MEM32(mcv+0xAC),MEM32(mcv+0x5A4),transport,sg_dist(mcv,transport),MEM32(mcv+0x11C));sg_v07_fail("MCV did not board");}else return;
 }
 if(stage==2){
  for(int i=0;i<sg_vec_count(0xA8EB40);i++){uint32_t b=sg_vec_at(0xA8EB40,i);if(!sg_alive(b))continue;uint32_t t=MEM32(b+0x520);if(!MEM8(t+0x157B)||MEM32(t+0x1580)<5)continue;building=b;break;}
  if(!building)sg_v07_fail("no garrisonable map building");int x=MEM32(building+0x9C),y=MEM32(building+0xA0);
  pilot=sg_v07_spawn("SGLINK",0xA8E348,house,x-4*256,y);ggi=sg_v07_spawn("GGI",0xA8E348,house,x-4*256,y+256);if(!pilot||!ggi)sg_v07_fail("infantry placement");
  g_ecx=building;int can1=hle_call_guest(0x457CE0,1,&pilot);g_ecx=building;int can2=hle_call_guest(0x457CE0,1,&ggi);g_ecx=saved;
  fprintf(stderr,"[v07-lab] garrison admission operator=%d GGI=%d building=%s capacity=%d\n",can1&255,can2&255,sg_id(building,1),MEM32(MEM32(building+0x520)+0x1580));if(!(can1&255)||!(can2&255))sg_v07_fail("garrison admission");
  uint32_t cell=sg_ai_cell(x,y), order[4]={8,building,cell,cell};g_ecx=pilot;hle_call_guest(MEM32(MEM32(pilot)+0x378),4,order);g_ecx=ggi;hle_call_guest(MEM32(MEM32(ggi)+0x378),4,order);g_ecx=saved;stage=3;stamp=f;
 }else if(stage==3){
  if(sg_controller_anchor(pilot)==building){SGUnit*u=sg_state(pilot);if(!u->children[0])return;uint32_t d=u->children[0];g_ecx=d;hle_call_guest(0x5F65F0,0,NULL);g_ecx=saved;stage=4;fprintf(stderr,"[v07-lab] operator garrisoned; destroyed drone to test replacement\n");}
  else if(f>stamp+900){fprintf(stderr,"[v07-lab] pilot mission=%d dest=%08X building=%08X limbo=%d alive=%d HP=%d dist=%.2f occupantCount=%d owner=%08X house=%08X GGIlimbo=%d\n",MEM32(pilot+0xAC),MEM32(pilot+0x5A4),building,MEM8(pilot+0x81),MEM8(pilot+0x90),MEM32(pilot+0x6C),sg_dist(pilot,building),sg_vec_count(building+0x684),MEM32(building+0x21C),house,MEM8(ggi+0x81));sg_v07_fail("operator did not garrison");}
 }else if(stage==4){SGUnit*u=sg_state(pilot);if(u->ready[0]){u->ready[0]=f;stage=5;}}
 else if(stage==5){SGUnit*u=sg_state(pilot);if(u->children[0]&&sg_alive(u->children[0])){fprintf(stderr,"[v07-lab] PASS drone replaced while controller inside building; guardian inside=%d\n",sg_controller_anchor(ggi)==building);stage=6;stamp=f;sg_ai_order(ggi,building,8);}}
 if(stage==6){if(sg_controller_anchor(ggi)==building){fprintf(stderr,"[v07-lab] PASS Guardian GI garrisoned\n");fflush(stderr);_exit(0);}else if(f>stamp+450){fprintf(stderr,"[v07-lab] GGI mission=%d limbo=%d alive=%d HP=%d dist=%.2f occupants=%d target=%08X dest=%08X\n",MEM32(ggi+0xAC),MEM8(ggi+0x81),MEM8(ggi+0x90),MEM32(ggi+0x6C),sg_dist(ggi,building),sg_vec_count(building+0x684),MEM32(ggi+0x2B4),MEM32(ggi+0x5A4));sg_v07_fail("GGI did not enter");}}
}

#endif
