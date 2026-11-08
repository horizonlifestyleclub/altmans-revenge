/* v09 verification lab: compiled only into lab-v09/build/ra2-lab. Places a small
   scene, orders the B-2s, and logs drone, card, promotion and bomb results. */
static const struct {const char *id;uint32_t vector;int x,y,owner;} lab_objects[]={
 {"GACNST",0xA83C68,40,78,0},{"GAPOWR",0xA83C68,34,78,0},{"GAPOWR",0xA83C68,34,73,0},{"GAPOWR",0xA83C68,34,84,0},
 {"GAPOWR",0xA83C68,34,89,0},{"GAPOWR",0xA83C68,30,78,0},{"GAPOWR",0xA83C68,30,84,0},{"GAPOWR",0xA83C68,30,89,0},{"GAPOWR",0xA83C68,30,73,0},{"GAPOWR",0xA83C68,26,78,0},
 {"GATECH",0xA83C68,46,78,0},{"AMRADR",0xA83C68,64,78,0},{"SGCOLO",0xA83C68,46,68,0},
 {"SGLINK",0xA8E348,60,62,0},{"SGLINK",0xA8E348,61,62,0},{"SGOP",0xA8E348,60,64,0},{"SGOP",0xA8E348,61,64,0},
 {"SGTANK",0xA83CE0,63,63,0},{"MTNK",0xA83CE0,52,74,0},{"GGI",0xA8E348,50,74,0},
 {"SGB2",0xA8B218,56,69,0},{"SGB2",0xA8B218,58,69,0},
 {"E2",0xA8E348,66,57,1},{"E2",0xA8E348,67,57,1},{"E2",0xA8E348,68,57,1},{"E2",0xA8E348,66,58,1},
 {"HTNK",0xA83CE0,68,59,1},
 {"NACNST",0xA83C68,75,35,1},{"NAREFN",0xA83C68,75,41,1},{"NAWEAP",0xA83C68,75,47,1},{"NAPOWR",0xA83C68,86,35,1},
 {"SGHOUND",0xA8E348,57,61,0},{"SGHOUND",0xA8E348,58,62,0},{"SGHOUND",0xA8E348,56,63,0},
 {"SGAREA",0xA83C68,53,66,0},
 {"SGSWARM",0xA83CE0,58,58,0},
};
static uint32_t lab_ptr[sizeof lab_objects/sizeof lab_objects[0]];
static uint32_t lab_spawn(const char *id,uint32_t vector,uint32_t house,int x,int y){
 uint32_t type=0;
 for(int i=0;i<sg_vec_count(vector);i++){uint32_t t=sg_vec_at(vector,i);if(!strcmp((char*)hle_ptr(t+0x24),id)){type=t;break;}}
 if(!type)return 0;
 uint32_t saved=g_ecx;g_ecx=type;uint32_t p=hle_call_guest(MEM32(MEM32(type)+0x8C),1,&house);g_ecx=saved;if(!p)return 0;
 int32_t*c=hle_heap_alloc(12,1);uint32_t args[2]={hle_va(c),0};int ok=0;
 for(int k=0;k<64&&!ok;k++){
  c[0]=(x+k%8)*256+128;c[1]=(y+k/8)*256+128;c[2]=0;
  uint32_t cc=hle_va(c);g_ecx=0x87F7E8;c[2]=hle_call_guest(0x578080,1,&cc);
  if(vector==0xA8B218)c[2]+=1500;
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
static int lab_count(uint32_t house,const char*id){
 int n=0;for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(sg_alive(p)&&MEM32(p+0x21C)==house&&!strcmp(sg_id(p,MEM32(p)==0x7E3EBC),id))n++;}return n;
}
static const char *lab_dir;
static void sg_lab_tick(uint32_t f){
 static int placed;static uint32_t previous,houses[2];
 if(f<previous)placed=0;previous=f;
 if(!lab_dir)lab_dir=g_dump_dir;
 g_dump_dir=lab_dir; /* the presenter dumps one frame every ~3 s */
 if(!placed){
  if(f<30)return;
  houses[0]=MEM32(0xA83D4C);houses[1]=0;
  for(int i=0;i<sg_vec_count(0xA80228);i++){uint32_t h=sg_vec_at(0xA80228,i),t=MEM32(h+0x34);if(h!=houses[0]&&!MEM8(t+0x1A6)&&strcmp((char*)hle_ptr(t+0x24),"Neutral"))houses[1]=h;}
  if(!houses[0]||!houses[1])return;
  placed=1;
  for(unsigned i=0;i<sizeof lab_objects/sizeof lab_objects[0];i++){
   lab_ptr[i]=lab_spawn(lab_objects[i].id,lab_objects[i].vector,houses[lab_objects[i].owner],lab_objects[i].x,lab_objects[i].y);
   fprintf(stderr,"[lab] %s %s object=%08X\n",lab_ptr[i]?"OK":"FAILED",lab_objects[i].id,lab_ptr[i]);
  }
  MEM8(houses[0]+0x5778)=1;MEM8(houses[1]+0x5778)=1;
  uint32_t saved=g_ecx;g_ecx=0x87F7E8;hle_call_guest(0x577D90,1,&houses[0]);g_ecx=saved;
  lab_view(63,60);
  return;
 }
 { static int probed;if(!probed&&f>=90){probed=1;
   fprintf(stderr,"[probe] building vtable+0x16C=%08X (ReceiveDamage 00442230?) +0x154=%08X\n",MEM32(0x7E3EBC+0x16C),MEM32(0x7E3EBC+0x154));
   for(int o=0;o<0x6000;o+=4){int run=1;for(int k=0;k<5;k++)if(MEM32(houses[0]+o+4*k)!=0x3F800000u)run=0;if(run)fprintf(stderr,"[probe] house+%04X five floats 1.0\n",o);}}}
 { static int hit;if(f<previous||f<60)hit=0;if(!hit&&f>=1500&&lab_ptr[12]&&sg_alive(lab_ptr[12])){hit=1;MEM32(lab_ptr[12]+0x6C)-=150;fprintf(stderr,"[lab] Colossus damaged\n");}}
 { /* deploy through the game's own order (Unload mission), pack, then move */
  static int stage;uint32_t b=lab_ptr[17];
  if(f<previous||f<60)stage=0;
  if(stage==0&&f>=200){stage=1;sg_ai_order(b,0,16);fprintf(stderr,"[lab] deploy order (Unload) given\n");}
  if(stage==1&&f>=1500){stage=2;sg_ai_order(b,0,16);fprintf(stderr,"[lab] second deploy order given\n");}
  if(stage==2&&f>=1700){stage=3;sg_ai_move(b,60,66);fprintf(stderr,"[lab] Bastion move ordered\n");}
 }
 if(f%120==0&&lab_ptr[17]&&sg_alive(lab_ptr[17])){SGUnit*u=sg_state(lab_ptr[17]);int n=0;for(int j=0;j<10;j++)if(u->children[j]&&sg_alive(u->children[j]))n++;fprintf(stderr,"[lab] f=%u bastion deployed=%d drones=%d\n",f,MEM8(lab_ptr[17]+0x6E0),n);}
 if(f==400){ /* B-2 #1 -> refinery, #2 -> construction yard */
  sg_ai_order(lab_ptr[20],lab_ptr[28],1);sg_ai_order(lab_ptr[21],lab_ptr[27],1);
  fprintf(stderr,"[lab] B-2 strike ordered\n");
 }
 if(f>=470&&f<480)lab_view(72,44);
 if(f==1300)lab_view(63,60);
 for(const char**id=(const char*[]){"SGRESEARCH","SGSINGULAR",NULL};*id;id++){ /* lab: 30 s cards */
  uint32_t c=sg_super(houses[0],*id);if(c&&MEM8(c+0x6D)&&sg_super_left(c)>450){MEM32(c+0x30)=f;MEM32(c+0x38)=450;fprintf(stderr,"[lab] shortened %s\n",*id);}
 }
 if(f%300==0){int hp=0;for(int i=22;i<=26;i++)hp+=lab_ptr[i]&&sg_alive(lab_ptr[i])?(int)MEM32(lab_ptr[i]+0x6C):0;fprintf(stderr,"[lab] f=%u placed enemy squad health=%d\n",f,hp);}
 if(f%60==0){
  SGHouse*s=sg_house(houses[0]);uint32_t rs=sg_super(houses[0],"SGRESEARCH"),ss=sg_super(houses[0],"SGSINGULAR");
  float r1=lab_ptr[13]&&sg_alive(lab_ptr[13])?sg_rank(lab_ptr[13]):-1,r2=lab_ptr[18]&&sg_alive(lab_ptr[18])?sg_rank(lab_ptr[18]):-1;
  fprintf(stderr,"[lab] f=%u stage=%d state=%s research(present=%d left=%d) singularity(present=%d left=%d) drones=%d _=%d _=%d enemyE2=%d enemyRhino=%d refinery=%d cy=%d wf=%d linkrank=%.1f tankrank=%.1f\n",
   f,s?s->stage:-1,s?sg_states[s->state]:"?",rs?MEM8(rs+0x6D):-1,rs?sg_super_left(rs):-1,ss?MEM8(ss+0x6D):-1,ss?sg_super_left(ss):-1,
   lab_count(houses[0],"SGPDRONE"),0,0,lab_count(houses[1],"E2"),lab_count(houses[1],"HTNK"),
   lab_ptr[28]&&sg_alive(lab_ptr[28])?(int)MEM32(lab_ptr[28]+0x6C):0,lab_ptr[27]&&sg_alive(lab_ptr[27])?(int)MEM32(lab_ptr[27]+0x6C):0,
   lab_ptr[29]&&sg_alive(lab_ptr[29])?(int)MEM32(lab_ptr[29]+0x6C):0,r1,r2);
  uint32_t card=ss&&MEM8(ss+0x6D)?ss:rs;
  if(card){fprintf(stderr,"[lab] card %s raw:",(char*)hle_ptr(MEM32(card+0x28)+0x24));for(int o=0x24;o<0x80;o+=4)fprintf(stderr," %02X=%08X",o,MEM32(card+o));fprintf(stderr," frame=%u\n",MEM32(RA2_GAME_FRAME_VA));}
  if(lab_ptr[13]){uint32_t p=lab_ptr[13];fprintf(stderr,"[lab] link mission=%d target=%08X alive=%d\n",(int)MEM32(p+0x22C),MEM32(p+0x2B4),sg_alive(p));}
  for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(MEM32(p)==0x7E22A4)fprintf(stderr,"[lab] aircraft %s owner=%08X alive=%d\n",sg_id(p,0),MEM32(p+0x21C),sg_alive(p));}
  fflush(stderr);
 }
}
