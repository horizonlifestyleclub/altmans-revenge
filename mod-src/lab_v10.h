/* v10 verification lab: compiled only into lab-v10/build/ra2-lab.
   Checks: LINK drone follows its operator; LINK relaunch after loss (15 s);
   Bastion holds at most 5 drones, launched one at a time; enemy rifle infantry
   damage drones; a suicide dive damages a tank; new units are elite once the
   Singularity is active. Results are "[v10]" log lines. */
static const struct {const char *id;uint32_t vector;int x,y,owner;} lab_objects[]={
 {"GACNST",0xA83C68,40,78,0},{"GAPOWR",0xA83C68,34,78,0},{"GAPOWR",0xA83C68,34,73,0},{"GAPOWR",0xA83C68,34,84,0},
 {"GAPOWR",0xA83C68,34,89,0},{"GAPOWR",0xA83C68,30,78,0},{"GAPOWR",0xA83C68,30,84,0},{"GAPOWR",0xA83C68,30,89,0},{"GAPOWR",0xA83C68,30,73,0},{"GAPOWR",0xA83C68,26,78,0},
 {"GATECH",0xA83C68,46,78,0},{"AMRADR",0xA83C68,64,78,0},{"SGCOLO",0xA83C68,46,68,0},
 /*13*/{"SGLINK",0xA8E348,56,70,0},
 /*14*/{"SGOP",0xA8E348,60,64,0},
 /*15*/{"SGTANK",0xA83CE0,52,62,0},
 /*16*/{"E1",0xA8E348,66,57,1},{"E1",0xA8E348,67,57,1},{"E1",0xA8E348,68,57,1},{"E1",0xA8E348,66,58,1},
 /*20*/{"HTNK",0xA83CE0,70,62,1},
 {"NACNST",0xA83C68,75,35,1},{"NAPOWR",0xA83C68,86,35,1},
};
#define LAB_N (int)(sizeof lab_objects/sizeof lab_objects[0])
static uint32_t lab_ptr[LAB_N];
static uint32_t lab_spawn(const char *id,uint32_t vector,uint32_t house,int x,int y){
 uint32_t type=0;
 for(int i=0;i<sg_vec_count(vector);i++){uint32_t t=sg_vec_at(vector,i);if(!strcmp((char*)hle_ptr(t+0x24),id)){type=t;break;}}
 if(!type)return 0;
 uint32_t saved=g_ecx;g_ecx=type;uint32_t p=hle_call_guest(MEM32(MEM32(type)+0x8C),1,&house);g_ecx=saved;if(!p)return 0;
 int32_t*c=hle_heap_alloc(12,1);uint32_t args[2]={hle_va(c),0};int ok=0;
 for(int k=0;k<64&&!ok;k++){
  c[0]=(x+k%8)*256+128;c[1]=(y+k/8)*256+128;c[2]=0;
  uint32_t cc=hle_va(c);g_ecx=0x87F7E8;c[2]=hle_call_guest(0x578080,1,&cc);
  g_ecx=p;ok=hle_call_guest(MEM32(MEM32(p)+0xD8),2,args);g_ecx=saved;
 }
 hle_heap_free(c);
 if(!ok){g_ecx=p;hle_call_guest(0x5F65F0,0,NULL);g_ecx=saved;return 0;}
 if(vector!=0xA83C68){uint32_t guard[2]={5,1};g_ecx=p;hle_call_guest(0x5B35E0,2,guard);g_ecx=saved;}
 return p;
}
static void lab_view(int x,int y){
 uint32_t saved=g_ecx;int32_t*view=hle_heap_alloc(12,1);view[0]=x*256+128;view[1]=y*256+128;view[2]=0;
 uint32_t at=hle_va(view);g_ecx=MEM32(0x887324);hle_call_guest(0x6D6070,1,&at);hle_heap_free(view);g_ecx=saved;
}
static int lab_drones(uint32_t op,uint32_t*first){
 SGUnit*u=sg_state(op);int n=0;if(first)*first=0;if(!u)return 0;
 for(int j=0;j<10;j++){uint32_t d=u->children[j];if(d&&MEM32(d+0x10)==u->child_uid[j]&&sg_alive(d)){n++;if(first&&!*first)*first=d;}}
 return n;
}
static const char *lab_dir;
static void sg_lab_tick(uint32_t f){
 static int placed;static uint32_t previous,houses[2],lost_at,late_tank;
 if(f<previous)placed=0;previous=f;
 if(!lab_dir)lab_dir=g_dump_dir;
 g_dump_dir=lab_dir;
 if(!placed){
  if(f<30)return;
  houses[0]=MEM32(0xA83D4C);houses[1]=0;
  for(int i=0;i<sg_vec_count(0xA80228);i++){uint32_t h=sg_vec_at(0xA80228,i),t=MEM32(h+0x34);if(h!=houses[0]&&!MEM8(t+0x1A6)&&strcmp((char*)hle_ptr(t+0x24),"Neutral"))houses[1]=h;}
  if(!houses[0]||!houses[1])return;
  placed=1;
  for(int i=0;i<LAB_N;i++){
   lab_ptr[i]=lab_spawn(lab_objects[i].id,lab_objects[i].vector,houses[lab_objects[i].owner],lab_objects[i].x,lab_objects[i].y);
   fprintf(stderr,"[lab] %s %s object=%08X\n",lab_ptr[i]?"OK":"FAILED",lab_objects[i].id,lab_ptr[i]);
  }
  MEM8(houses[0]+0x5778)=1;MEM8(houses[1]+0x5778)=1;
  uint32_t saved=g_ecx;g_ecx=0x87F7E8;hle_call_guest(0x577D90,1,&houses[0]);g_ecx=saved;
  lab_view(58,64);
  return;
 }
 uint32_t link=lab_ptr[13],op=lab_ptr[14],bas=lab_ptr[15],tank=lab_ptr[20];
 /* 1. LINK walks away; its drone must follow (distance stays small). */
 if(f==300&&sg_alive(link)){sg_ai_move(link,46*256+128,74*256+128);fprintf(stderr,"[v10] LINK ordered to move\n");}
 if(f%60==0&&f>=240&&f<=1200&&sg_alive(link)){uint32_t d;int n=lab_drones(link,&d);
  fprintf(stderr,"[v10] f=%u link drones=%d link=(%d,%d) mission=%d drone_dist=%.1f\n",f,n,(int32_t)MEM32(link+0x9C)/256,(int32_t)MEM32(link+0xA0)/256,(int)MEM32(link+0xAC),d?sg_dist(d,link):-1);}
 /* 2. Shoot the LINK drone down: it should come back in about 15 s (225 frames). */
 if(f==1200&&sg_alive(link)){uint32_t d;if(lab_drones(link,&d)&&d){uint32_t saved=g_ecx;g_ecx=d;hle_call_guest(0x5F65F0,0,NULL);g_ecx=saved;lost_at=f;fprintf(stderr,"[v10] LINK drone removed at f=%u\n",f);}}
 if(lost_at&&f>lost_at&&f%15==0){uint32_t d;if(lab_drones(link,&d)){fprintf(stderr,"[v10] LINK drone back after %.1f s\n",(f-lost_at)/15.);lost_at=0;}}
 /* 3. Bastion deploy: count drones (max 5, one at a time). */
 if(f==200&&sg_alive(bas)){sg_ai_order(bas,0,16);fprintf(stderr,"[v10] Bastion deploy order\n");}
 if(f%45==0&&f>=200&&f<=1400&&sg_alive(bas))fprintf(stderr,"[v10] f=%u bastion deployed=%d drones=%d\n",f,MEM8(bas+0x6E0),lab_drones(bas,NULL));
 /* 4. Drone health vs enemy rifles; 5. NEST drone in suicide stance dives on the tank. */
 if(f%90==0&&f>=300){int hp=0,n=0;for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(sg_alive(p)&&MEM32(p+0x21C)==houses[0]&&!strcmp(sg_id(p,0),"SGPDRONE")){hp+=(int)MEM32(p+0x6C);n++;}}
  int e1t=0;for(int i=16;i<=19;i++){uint32_t e=lab_ptr[i];if(e&&sg_alive(e)){uint32_t t=MEM32(e+0x2B4);if(t>0x10000&&t<0xF0000000&&MEM32(t)==0x7F5C70&&!strcmp(sg_id(t,0),"SGPDRONE"))e1t++;}}
  fprintf(stderr,"[v10] f=%u our drones=%d total_hp=%d enemyGIs_targeting_drones=%d tank_hp=%d\n",f,n,hp,e1t,tank&&sg_alive(tank)?(int)MEM32(tank+0x6C):0);}
 if(f==700&&sg_alive(op)&&tank&&sg_alive(tank)){uint32_t d;if(lab_drones(op,&d)&&d){SGUnit*du=sg_state(d);du->mode=1;du->own=1;sg_drone_cmd(d,du,tank,1);fprintf(stderr,"[v10] NEST drone set SUICIDE, ordered on tank hp=%d\n",(int)MEM32(tank+0x6C));}}
 /* 6. Singularity: lab cards are 30 s; a tank built afterwards must be elite. */
 for(const char**id=(const char*[]){"SGRESEARCH","SGSINGULAR",NULL};*id;id++){
  uint32_t c=sg_super(houses[0],*id);if(c&&MEM8(c+0x6D)&&sg_super_left(c)>450){MEM32(c+0x30)=f;MEM32(c+0x38)=450;fprintf(stderr,"[lab] shortened %s\n",*id);}
 }
 {SGHouse*s=sg_house(houses[0]);if(s&&s->state==SG_ACTIVE&&!late_tank){late_tank=lab_spawn("MTNK",0xA83CE0,houses[0],44,72);fprintf(stderr,"[v10] f=%u Singularity ACTIVE; new tank %08X rank=%.1f\n",f,late_tank,late_tank?sg_rank(late_tank):-1);}
  if(late_tank&&f%60==0&&sg_alive(late_tank))fprintf(stderr,"[v10] f=%u new tank rank=%.1f (2.0 = elite) colossus cost=%d\n",f,sg_rank(late_tank),0);}
 if(f==1250)lab_view(50,70);
 if(f%300==0)fflush(stderr);
}
