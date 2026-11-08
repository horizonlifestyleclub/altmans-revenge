/* Fixture population is compiled only into unit-showcase/build/ra2-showcase. */
static const struct {const char *id;uint32_t vector;int x,y,owner,rank;} sg_show_objects[]={
/* INVENTORY */
};
static uint32_t sg_show_spawn(const char *id,uint32_t vector,uint32_t house,int x,int y){
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
static void sg_showcase_tick(uint32_t f){
 static int done;static uint32_t previous;
 if(f>300)g_dump_dir=NULL; /* Capture startup only, never record the player's ongoing session. */
 if(f<previous)done=0;previous=f;
 if(done||f<30)return;
 uint32_t houses[3]={MEM32(0xA83D4C),0,0};
 if(houses[0]<0x10000)return;
 for(int i=0;i<sg_vec_count(0xA80228);i++){
  uint32_t h=sg_vec_at(0xA80228,i),t=MEM32(h+0x34);
  if(!strcmp((char*)hle_ptr(t+0x24),"Neutral"))houses[2]=h;
  else if(h!=houses[0]&&!MEM8(t+0x1A6))houses[1]=h;
 }
 if(!houses[1])return;
 if(!houses[2])houses[2]=houses[0];
 done=1;int good=0,bad=0;
 for(unsigned i=0;i<sizeof sg_show_objects/sizeof sg_show_objects[0];i++){
  const typeof(sg_show_objects[0])*o=&sg_show_objects[i];
  uint32_t p=sg_show_spawn(o->id,o->vector,houses[o->owner],o->x,o->y);
  fprintf(stderr,"[showcase] %s %s owner=%d object=%08X\n",p?"OK":"FAILED",o->id,o->owner,p);
  if(!p){bad++;continue;}good++;
  if(o->rank)*(float*)hle_ptr(p+0x150)=(float)o->rank;
  if(!strcmp(o->id,"SGTANK"))sg_state(p)->nest_active=1;
  if(!strcmp(o->id,"SGNEEDLE")||!strcmp(o->id,"SGLOCUST"))sg_launch_drone(p);
 }
 for(int i=0;i<2;i++)MEM8(houses[i]+0x5778)=1;
 uint32_t saved=g_ecx;
 g_ecx=0x87F7E8;hle_call_guest(0x577D90,1,&houses[0]);
 int32_t*view=hle_heap_alloc(12,1);view[0]=54*256+128;view[1]=74*256+128;view[2]=0;
 uint32_t at=hle_va(view);g_ecx=MEM32(0x887324);hle_call_guest(0x6D6070,1,&at);hle_heap_free(view);g_ecx=saved;
 fprintf(stderr,"[showcase] READY placed=%d failed=%d frame=%u\n",good,bad,f);fflush(stderr);
}
