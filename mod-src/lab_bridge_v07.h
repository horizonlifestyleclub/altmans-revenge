static int sg_bridge_broken(uint32_t hut){uint32_t*q=hle_heap_alloc(4,1);*q=(uint16_t)((int32_t)MEM32(hut+0x9C)/256)|((uint32_t)(uint16_t)((int32_t)MEM32(hut+0xA0)/256)<<16);uint32_t a=hle_va(q);int out=sg_ai_call(0x87F7E8,0x587410,1,&a)&255;hle_heap_free(q);return out;}
static void sg_v07_lab(uint32_t f){
 static uint32_t hut,engineer,stamp;static int stage;
 if(f<60)return;
 if(!stage){
  uint32_t house=0;for(int i=0;i<sg_vec_count(0xA80228);i++){uint32_t h=sg_vec_at(0xA80228,i);if(sg_ai_house(h)){house=h;break;}}
  if(!house)sg_v07_fail("no adaptive house");
  for(int i=0;i<sg_vec_count(0xA8EB40);i++){uint32_t b=sg_vec_at(0xA8EB40,i);if(sg_alive(b)&&!strcmp(sg_id(b,1),"CABHUT")){hut=b;break;}}
  if(!hut)sg_v07_fail("no bridge hut on test map");
  int x=MEM32(hut+0x9C),y=MEM32(hut+0xA0),broken=sg_bridge_broken(hut);
  for(int radius=1;radius<12&&!broken;radius++)for(int dx=-radius;dx<=radius&&!broken;dx++)for(int dy=-radius;dy<=radius&&!broken;dy++){
   if(abs(dx)!=radius&&abs(dy)!=radius)continue;uint32_t cell=sg_ai_cell(x+dx*256,y+dy*256);if(!cell)continue;
   if((sg_ai_call(cell,0x486750,0,NULL)&255)||(sg_ai_call(cell,0x486770,0,NULL)&255)){sg_ai_call(cell,0x47DD70,0,NULL);broken=sg_bridge_broken(hut);}
  }
  if(!broken)sg_v07_fail("could not destroy linked bridge");
  engineer=sg_v07_spawn("SENGINEER",0xA8E348,house,x-2*256,y);if(!engineer)sg_v07_fail("engineer placement");
  fprintf(stderr,"[bridge-lab] bridge broken; AI engineer sees repair hut=%d\n",sg_ai_visible(house,hut));stamp=f;stage=1;
 }else if(stage==1){
  if(!sg_bridge_broken(hut)){fprintf(stderr,"[bridge-lab] PASS adaptive AI repaired discovered broken bridge through engineer capture order at frame %u\n",f);fflush(stderr);_exit(0);}
  if(f>stamp+1500){fprintf(stderr,"[bridge-lab] engineer mission=%d dest=%08X target=%08X alive=%d distance=%.2f\n",MEM32(engineer+0xAC),MEM32(engineer+0x5A4),MEM32(engineer+0x2B4),sg_alive(engineer),sg_dist(engineer,hut));sg_v07_fail("bridge repair timed out");}
 }
}
