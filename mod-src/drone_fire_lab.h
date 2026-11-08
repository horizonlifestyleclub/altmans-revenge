static uint32_t sg_lab_spawn(const char*id,uint32_t vector,uint32_t house,int x,int y){
 uint32_t type=0;for(int i=0;i<sg_vec_count(vector);i++){uint32_t t=sg_vec_at(vector,i);if(!strcmp((char*)hle_ptr(t+0x24),id)){type=t;break;}}if(!type)return 0;
 uint32_t saved=g_ecx;g_ecx=type;uint32_t p=hle_call_guest(MEM32(MEM32(type)+0x8C),1,&house);g_ecx=saved;if(!p)return 0;
 int32_t*c=hle_heap_alloc(12,1);c[0]=x;c[1]=y;c[2]=0;uint32_t args[2]={hle_va(c),0};g_ecx=p;int ok=hle_call_guest(MEM32(MEM32(p)+0xD8),2,args);g_ecx=saved;hle_heap_free(c);
 if(!ok){g_ecx=p;hle_call_guest(0x5F65F0,0,NULL);g_ecx=saved;return 0;}if(!strcmp(id,"SGPDRONE"))sg_launch_drone(p);return p;
}
static void sg_lab_fire(uint32_t drone,uint32_t target,int mode){if(!sg_alive(drone)||!sg_alive(target))sg_lab_fail("dead actor before attack order");uint32_t saved=g_ecx;sg_state(drone)->suicide=mode;g_ecx=drone;hle_call_guest(MEM32(MEM32(drone)+242*4),1,&target);uint32_t args[2]={1,1};g_ecx=drone;hle_call_guest(0x5B35E0,2,args);g_ecx=saved;}
static void sg_fire_lab(uint32_t pilot,uint32_t f){
 static int stage;static uint32_t enemy,tank,drone,soldiers[3],second,stamp;uint32_t own=MEM32(pilot+0x21C);
 if(!stage){float initial=0;memcpy(hle_ptr(pilot+0x150),&initial,4);
  for(int i=0;i<sg_vec_count(0xA80228);i++){uint32_t h=sg_vec_at(0xA80228,i);if(sg_enemy(own,h)){enemy=h;break;}}if(!enemy)sg_lab_fail("no opponent");
  int x=(int32_t)MEM32(pilot+0x9C)+6*256,y=(int32_t)MEM32(pilot+0xA0);
  for(int j=0;j<8&&!tank;j++)tank=sg_lab_spawn("HTNK",0xA83CE0,enemy,x,y+j*256);
  if(!tank)sg_lab_fail("cannot place tank target");
  x=MEM32(tank+0x9C);y=MEM32(tank+0xA0);drone=sg_lab_spawn("SGPDRONE",0xA83CE0,own,x-256,y);if(!drone)sg_lab_fail("cannot place strike drone");
  sg_state(drone)->controller=pilot;sg_state(drone)->controller_uid=MEM32(pilot+0x10);
  sg_lab_fire(drone,tank,0);stamp=f;stage=1;fprintf(stderr,"[drone-fire] normal shot fired at 400HP Rhino\n");
 }else if(stage==1&&f>=stamp+30){
  int hp=MEM32(tank+0x6C);fprintf(stderr,"[drone-fire] normal shot Rhino HP %d\n",hp);if(hp>=400&&f<stamp+180)return;if(hp<=300||hp>=400)sg_lab_fail("normal weapon damage");
  fprintf(stderr,"[drone-fire] drone alive=%d altitude=%d\n",sg_alive(drone),(int)MEM32(drone+0xA4));if(!sg_alive(drone))sg_lab_fail("drone lost before suicide order");sg_state(drone)->suicide=1;sg_lab_fire(drone,tank,1);stamp=f;stage=2;
 }else if(stage==2&&f>=stamp+30){
  fprintf(stderr,"[drone-fire] suicide shot Rhino alive=%d drone alive=%d\n",sg_alive(tank),sg_alive(drone));if(sg_alive(tank)&&sg_alive(drone)&&f<stamp+180)return;if(sg_alive(tank)||sg_alive(drone))sg_lab_fail("tank kill or suicide");
  fprintf(stderr,"[drone-fire] controller veterancy after kill %.3f\n",sg_rank(pilot));
  int x=(int32_t)MEM32(pilot+0x9C),y=(int32_t)MEM32(pilot+0xA0)+6*256;
  int offset[3][2]={{0,0},{64,64},{-64,64}};
  for(int j=0;j<3;j++){soldiers[j]=sg_lab_spawn("E1",0xA8E348,enemy,x+offset[j][0],y+offset[j][1]);if(!soldiers[j])sg_lab_fail("cannot place infantry target");}
  second=sg_lab_spawn("SGPDRONE",0xA83CE0,own,x-256,y);if(!second)sg_lab_fail("cannot place second strike drone");
  sg_state(second)->controller=pilot;sg_state(second)->controller_uid=MEM32(pilot+0x10);sg_state(second)->suicide=1;
  sg_lab_fire(second,soldiers[0],1);stamp=f;stage=3;
 }else if(stage==3&&f>=stamp+30){int alive=0;for(int j=0;j<3;j++)alive+=sg_alive(soldiers[j]);fprintf(stderr,"[drone-fire] clustered infantry survivors=%d; suicide drone alive=%d\n",alive,sg_alive(second));if(alive||sg_alive(second))sg_lab_fail("infantry splash");fprintf(stderr,"[drone-fire] PASS weak normal fire, one Rhino kill, three clustered infantry kills, drones consumed\n");stage=4;}
}
