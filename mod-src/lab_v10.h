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
 /*23*/{"JUMPJET",0xA8E348,63,58,0},
 /*24*/{"MTNK",0xA83CE0,61,59,1},
 /*25*/{"SGEMPT",0xA83CE0,64,66,1},
 /*26*/{"SGTANKC",0xA83CE0,76,50,1},
 /*27*/{"SGSHAHED",0xA83CE0,24,70,1},
 /*28*/{"SGSTARLINK",0xA83C68,82,40,1},
 /*29*/{"SGCYBER",0xA83CE0,78,44,1},
 /*30*/{"SGAREA",0xA83C68,40,64,0},
 /*31*/{"SGTICTAC",0xA83CE0,58,58,0},
 /*32*/{"FLAKT",0xA8E348,64,60,1},{"FLAKT",0xA8E348,65,60,1},
 /*34*/{"SGFUSION",0xA83C68,88,44,1},
 /*35*/{"TANY",0xA8E348,36,66,0},
 /*36*/{"SGTENT",0xA83C68,38,62,0},{"SNIPE",0xA8E348,37,67,0},{"YURI",0xA8E348,38,67,0},
 /*39*/{"E1",0xA8E348,39,67,0},
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

 { static int probed;if(!probed&&f>=100){probed=1;
   const char*ids[]={"SGPDRONE","SGSWARM","SGSWARMC","SGTANK","SGTANKD"};
   for(int k=0;k<5;k++){uint32_t t=sg_find_unit_type(ids[k]);fprintf(stderr,"[v10] voxel %s type=%08X MainVoxel=%08X (%s)\n",ids[k],t,t?MEM32(t+0xB0):0,t&&MEM32(t+0xB0)?"VISIBLE":"MISSING");}
 }}
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
 if(f==420&&sg_alive(bas)){SGUnit*u=sg_state(bas);for(int j=0;u&&j<5;j++)if(u->children[j])fprintf(stderr,"[v10] bastion drone %d type=%s\n",j,sg_id(u->children[j],0));}
 if(f%15==0&&f>=200&&f<=700&&sg_alive(bas))fprintf(stderr,"[v10] f=%u bastion deployed=%d type=%s mission=%d drones=%d\n",f,MEM8(bas+0x6E0),sg_id(bas,0),(int)MEM32(bas+0xAC),lab_drones(bas,NULL));
 /* 3b. Attack order on the selected Bastion sends all of its drones. */
 if(f==600&&sg_alive(bas)&&lab_ptr[24]&&sg_alive(lab_ptr[24])){tank=lab_ptr[24];MEM8(bas+0x83)=1;sg_ai_order(bas,tank,1);fprintf(stderr,"[v10] Bastion selected + attack order on a tank %.1f cells away\n",sg_dist(bas,tank));}
 if(f==630&&sg_alive(bas)){tank=lab_ptr[24];MEM8(bas+0x83)=0;SGUnit*u=sg_state(bas);int n=0,on=0;for(int j=0;u&&j<10;j++){uint32_t d=u->children[j];if(d&&MEM32(d+0x10)==u->child_uid[j]&&sg_alive(d)){n++;if(MEM32(d+0x2B4)==tank)on++;}}
  fprintf(stderr,"[v10] Bastion attack-all: %d of %d drones on the tank; bastion mission=%d deployed=%d\n",on,n,(int)MEM32(bas+0xAC),MEM8(bas+0x6E0));}
 /* 7. Rifle infantry must not shoot our Rocketeer (only drones). */
 if(f%90==0&&f>=200&&f<=1200&&lab_ptr[23]&&sg_alive(lab_ptr[23])){int on=0;for(int i=16;i<=19;i++){uint32_t e=lab_ptr[i];if(e&&sg_alive(e)&&MEM32(e+0x2B4)==lab_ptr[23])on++;}
  fprintf(stderr,"[v10] f=%u rocketeer hp=%d height=%d enemy GIs targeting it=%d\n",f,(int)MEM32(lab_ptr[23]+0x6C),(int)MEM32(lab_ptr[23]+0xA4)-sg_floor(MEM32(lab_ptr[23]+0x9C),MEM32(lab_ptr[23]+0xA0)),on);}
 /* 8. Right-click dive: put the mouse over an enemy GI, right-click with the LINK drone selected. */
 static uint32_t rc_drone;static int32_t rc_last[2];
 if(f==880)lab_view(63,60);
 if(f==898&&sg_alive(op)){uint32_t d;if(lab_drones(op,&d)&&d){MEM8(d+0x83)=1;rc_drone=d;}}
 if(f==900&&rc_drone&&sg_alive(rc_drone)){
  uint32_t e=0;for(int i=16;i<=19&&!e;i++)if(lab_ptr[i]&&sg_alive(lab_ptr[i]))e=lab_ptr[i];
  uint32_t mouse=MEM32(0x887640);int W=(int)MEM32(0xB0CE30),H=(int)MEM32(0xB0CE34)+(int)MEM32(0xB0CE2C),bx=0,by=0;double bd=1e9;
  int32_t*m=hle_heap_alloc(20,1);
  for(int y=0;e&&y<H;y+=8)for(int x=0;x<W;x+=8){m[0]=x;m[1]=y;m[2]=m[3]=m[4]=0;uint32_t args[2]={hle_va(m+2),hle_va(m)},sv=g_ecx;g_ecx=MEM32(0x887324);hle_call_guest(0x6D2280,2,args);g_ecx=sv;
   double dx=(m[2]-(int32_t)MEM32(e+0x9C))/256.,dy=(m[3]-(int32_t)MEM32(e+0xA0))/256.,dd=dx*dx+dy*dy;if((m[2]||m[3])&&dd<bd){bd=dd;bx=x;by=y;}}
  hle_heap_free(m);
  fprintf(stderr,"[v10] viewport %dx%d; enemy GI %08X at (%d,%d) is under screen point (%d,%d), error %.2f cells\n",W,H,e,e?(int32_t)MEM32(e+0x9C)/256:0,e?(int32_t)MEM32(e+0xA0)/256:0,bx,by,sqrt(bd));
  if(e&&mouse){MEM32(mouse+0x1C)=bx;MEM32(mouse+0x20)=by;sg_rclick=1;fprintf(stderr,"[v10] simulated right-click\n");}
  rc_last[0]=(int32_t)MEM32(rc_drone+0x9C);rc_last[1]=(int32_t)MEM32(rc_drone+0xA0);
 }
 if(rc_drone&&f>900&&f<=1150&&f%5==0){
  if(sg_alive(rc_drone)){SGUnit*du=sg_state(rc_drone);double sp;memcpy(&sp,hle_ptr(rc_drone+0x580),8);int32_t x=(int32_t)MEM32(rc_drone+0x9C),y=(int32_t)MEM32(rc_drone+0xA0);
   fprintf(stderr,"[v10] f=%u dive drone dive=%d target=%08X jjspeed=%d moved %.2f cells in 5 frames; to target %.2f mission %d wpn %d z %d\n",f,du?du->dive:-1,MEM32(rc_drone+0x2B4),(int)MEM32(MEM32(rc_drone+0x674)+0x1C),sqrt((double)(x-rc_last[0])*(x-rc_last[0])+(double)(y-rc_last[1])*(y-rc_last[1]))/256.,sg_targetable(MEM32(rc_drone+0x2B4))?sg_dist(rc_drone,MEM32(rc_drone+0x2B4)):-1,(int)MEM32(rc_drone+0xAC),sg_weapon_mode(rc_drone),(int)MEM32(rc_drone+0xA4)-sg_floor(MEM32(rc_drone+0x9C),MEM32(rc_drone+0xA0)));rc_last[0]=x;rc_last[1]=y;}
  else {fprintf(stderr,"[v10] f=%u dive drone gone (exploded)\n",f);rc_drone=0;}
 }
 /* reference: LINK drone following speed */
 static int32_t ref_last[2];
 if(f>=310&&f<=400&&f%5==0&&sg_alive(link)){uint32_t d;if(lab_drones(link,&d)&&d){int32_t x=(int32_t)MEM32(d+0x9C),y=(int32_t)MEM32(d+0xA0);double sp;memcpy(&sp,hle_ptr(d+0x580),8);
  if(f>310)fprintf(stderr,"[v10] f=%u follow drone speedmult=%.2f moved %.2f cells in 5 frames\n",f,sp,sqrt((double)(x-ref_last[0])*(x-ref_last[0])+(double)(y-ref_last[1])*(y-ref_last[1]))/256.);ref_last[0]=x;ref_last[1]=y;}}

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
  if(late_tank&&f%60==0&&sg_alive(late_tank))fprintf(stderr,"[v10] f=%u new tank rank=%.1f (1.0 = one star) colossus cost=%d\n",f,sg_rank(late_tank),0);}
 if(f==1250)lab_view(50,70);
 /* 11. China: HIVE deploys with ten light drones; one Shahed kills a power plant. */
 if(f==220&&lab_ptr[26]&&sg_alive(lab_ptr[26]))sg_ai_order(lab_ptr[26],0,16);
 if(f==600&&lab_ptr[26]&&sg_alive(lab_ptr[26])){SGUnit*u=sg_state(lab_ptr[26]);int n=0;const char*t="";for(int j=0;u&&j<10;j++){uint32_t d=u->children[j];if(d&&MEM32(d+0x10)==u->child_uid[j]&&sg_alive(d)){n++;t=sg_id(d,0);}}
  fprintf(stderr,"[v10] HIVE type=%s deployed=%d drones=%d (%s)\n",sg_id(lab_ptr[26],0),MEM8(lab_ptr[26]+0x6E0),n,t);}
 if(f==300&&lab_ptr[27]&&sg_alive(lab_ptr[27])&&lab_ptr[1]&&sg_alive(lab_ptr[1])){sg_ai_order(lab_ptr[27],lab_ptr[1],1);fprintf(stderr,"[v10] Shahed ordered on power plant hp=%d, %.1f cells\n",(int)MEM32(lab_ptr[1]+0x6C),sg_dist(lab_ptr[27],lab_ptr[1]));}
 if(f%60==0&&f>300&&f<=900&&lab_ptr[1])fprintf(stderr,"[v10] f=%u power plant %s hp=%d; shahed %s\n",f,sg_alive(lab_ptr[1])?"alive":"DESTROYED",sg_alive(lab_ptr[1])?(int)MEM32(lab_ptr[1]+0x6C):0,lab_ptr[27]&&sg_alive(lab_ptr[27])?"flying":"gone");if(lab_ptr[27]&&sg_alive(lab_ptr[27])&&f%60==0&&f>300)fprintf(stderr,"[v10] shahed hp=%d at (%d,%d) to plant %.1f\n",(int)MEM32(lab_ptr[27]+0x6C),(int)MEM32(lab_ptr[27]+0x9C)/256,(int)MEM32(lab_ptr[27]+0xA0)/256,sg_dist(lab_ptr[27],lab_ptr[1]));
 /* 12. Map darkening: enemy Starlink + Cyber unit gaps on and growing; the Cyber unit moves. */
 if(f==400&&lab_ptr[29]&&sg_alive(lab_ptr[29]))sg_ai_move(lab_ptr[29],70*256+128,50*256+128);
 if(f%150==0&&f>=200&&f<=1500)for(int k=28;k<=29;k++)if(lab_ptr[k]&&sg_alive(lab_ptr[k]))fprintf(stderr,"[v10] f=%u %s gap=%d radius=%d at (%d,%d)\n",f,k==28?"Starlink":"Cyber",MEM8(lab_ptr[k]+0x269),(int)MEM32(lab_ptr[k]+0x26C),(int)MEM32(lab_ptr[k]+0x9C)/256,(int)MEM32(lab_ptr[k]+0xA0)/256);
 /* 9. LINK attack order is handed to its drone. */
 if(f==1450&&sg_alive(link)){uint32_t e=0;for(int i=16;i<=25&&!e;i++)if(i!=23&&lab_ptr[i]&&sg_alive(lab_ptr[i]))e=lab_ptr[i];
  if(e){MEM8(link+0x83)=1;sg_ai_order(link,e,1);fprintf(stderr,"[v10] LINK selected + attack order on enemy %s %08X\n",sg_id(e,0),e);}}
 if(f==1460&&sg_alive(link)){uint32_t d;MEM8(link+0x83)=0;if(lab_drones(link,&d)&&d)fprintf(stderr,"[v10] LINK drone target after hand-off: %08X (LINK target %08X)\n",MEM32(d+0x2B4),MEM32(link+0x2B4));}
 /* 10. Rank after Singularity must be 1 star; uplink unlocks strike drones. */
 if(f%300==0&&late_tank){uint32_t up=sg_find_unit_type("SGFPV");fprintf(stderr,"[v10] f=%u late tank rank=%.1f; SGFPV type=%08X\n",f,sg_rank(late_tank),up);}
 /* 13. TIC TAC: no damage from enemy flak; idle until ordered; hunts once the
    Singularity is ACTIVE; lost with Area 51. 14. Fusion Reactor power. */
 {uint32_t tt=lab_ptr[31];
  if(f%90==0&&f>=200&&f<=2400)fprintf(stderr,"[v10] f=%u tictac %s hp=%d mission=%d dest=%08X target=%08X flak1 target=%08X\n",f,tt&&sg_alive(tt)?"alive":"GONE",tt&&sg_alive(tt)?(int)MEM32(tt+0x6C):0,tt&&sg_alive(tt)?(int)MEM32(tt+0xAC):-1,tt&&sg_alive(tt)?MEM32(tt+0x5A4):0,tt&&sg_alive(tt)?MEM32(tt+0x2B4):0,lab_ptr[32]&&sg_alive(lab_ptr[32])?MEM32(lab_ptr[32]+0x2B4):0);
  if(f==700&&tt&&sg_alive(tt)&&lab_ptr[34]&&sg_alive(lab_ptr[34])){sg_ai_order(tt,lab_ptr[34],1);fprintf(stderr,"[v10] tictac ordered on enemy Fusion Reactor hp=%d\n",(int)MEM32(lab_ptr[34]+0x6C));}
  if(f>700&&f<=1200&&f%30==0&&lab_ptr[34])fprintf(stderr,"[v10] f=%u fusion reactor %s hp=%d, %.1f cells from tictac, tictac target=%08X\n",f,sg_alive(lab_ptr[34])?"alive":"DESTROYED",sg_alive(lab_ptr[34])?(int)MEM32(lab_ptr[34]+0x6C):0,tt&&sg_alive(tt)&&sg_alive(lab_ptr[34])?sg_dist(tt,lab_ptr[34]):0,tt?MEM32(tt+0x2B4):0);
  if(f==900)fprintf(stderr,"[v10] heavy tank after tictac attack: %s\n",lab_ptr[20]&&sg_alive(lab_ptr[20])?"alive":"DESTROYED");
  if(f==2200&&lab_ptr[30]&&sg_alive(lab_ptr[30])){sg_remove(lab_ptr[30]);fprintf(stderr,"[v10] f=%u Area 51 removed\n",f);}
  if(f==600)fprintf(stderr,"[v10] fusion %s; enemy power output=%d drain=%d\n",lab_ptr[34]&&sg_alive(lab_ptr[34])?"built":"MISSING",(int)MEM32(houses[1]+0x53A4),(int)MEM32(houses[1]+0x53A8));}
 /* 15. Garrison: Tanya (newly an Occupier) walks into a Bastion camp tent. */
 {static uint32_t tent;uint32_t ty=lab_ptr[35];
  if(f==500&&ty&&sg_alive(ty)){tent=lab_ptr[36]&&sg_alive(lab_ptr[36])?lab_ptr[36]:0;for(int k=37;k<=39&&tent;k++)if(lab_ptr[k]&&sg_alive(lab_ptr[k]))sg_ai_order(lab_ptr[k],tent,7);
   if(tent){fprintf(stderr,"[v10] Sniper + Yuri ordered into camp tent %08X (%.1f cells)\n",tent,sg_dist(ty,tent));}else fprintf(stderr,"[v10] no camp tent to garrison\n");}
  if(tent&&f>=560&&f<=1300&&f%60==0)fprintf(stderr,"[v10] f=%u tent %s occupants=%d sniper at (%d,%d) mission %d; GI mission %d at (%d,%d); owner tent=%08X us=%08X\n",f,sg_alive(tent)?"standing":"GONE",sg_alive(tent)?(int)MEM32(tent+0x694):-1,lab_ptr[37]?(int)MEM32(lab_ptr[37]+0x9C)/256:0,lab_ptr[37]?(int)MEM32(lab_ptr[37]+0xA0)/256:0,lab_ptr[37]?(int)MEM32(lab_ptr[37]+0xAC):-1,lab_ptr[39]?(int)MEM32(lab_ptr[39]+0xAC):-1,lab_ptr[39]?(int)MEM32(lab_ptr[39]+0x9C)/256:0,lab_ptr[39]?(int)MEM32(lab_ptr[39]+0xA0)/256:0,sg_alive(tent)?MEM32(tent+0x21C):0,houses[0]);}
 if(f%300==0)fflush(stderr);
}
