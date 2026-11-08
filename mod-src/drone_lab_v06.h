/* Isolated test-only runtime fixture. Never enabled in the installed build. */
static void sg_lab_fail(const char *why){fprintf(stderr,"[drone-lab] FAIL %s\n",why);fflush(stderr);_exit(6);}
#include "drone_fire_lab.h"
static int sg_lab_alive(SGUnit*u){int n=0;for(int j=0;j<5;j++)if(u->children[j]&&sg_alive(u->children[j])&&MEM32(u->children[j]+0x10)==u->child_uid[j])n++;return n;}
static void sg_drone_lab(uint32_t f){
 static uint32_t pilot,tank,old;static int stage,reported;
 if(f<110)return;
 uint32_t house=MEM32(0xA83D4C);
 if(!pilot)for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(!sg_alive(p)||MEM32(p+0x21C)!=house)continue;uint32_t vt=MEM32(p);if(vt!=0x7F5C70&&vt!=0x7EB058)continue;if(!strcmp(sg_id(p,0),"SGLINK")&&(!pilot||sg_lab_alive(sg_state(p))>0))pilot=p;if(!strcmp(sg_id(p,0),"SGTANK"))tank=p;}
 if(!pilot||!tank){if(f>500)sg_lab_fail("missing pilot/tank fixture");return;}
 SGUnit*u=sg_state(pilot),*t=sg_state(tank);float rank;
 if(stage==0&&sg_lab_alive(u)==1){rank=1;memcpy(hle_ptr(pilot+0x150),&rank,4);t->nest_active=1;stage=1;fprintf(stderr,"[drone-lab] rookie one drone; promote veteran, deploy tank\n");}
 else if(stage==1&&sg_lab_alive(u)==2){rank=2;memcpy(hle_ptr(pilot+0x150),&rank,4);stage=2;fprintf(stderr,"[drone-lab] veteran two drones; promote elite\n");}
 else if(stage==2&&sg_lab_alive(u)==3&&sg_lab_alive(t)==5){
  for(int i=0;i<sg_vec_count(0xA8EC78);i++){uint32_t p=sg_vec_at(0xA8EC78,i);if(sg_alive(p))MEM8(p+0x83)=0;}
  uint32_t d=u->children[0];MEM8(d+0x83)=1;if(!sg_drone_command(7,1)||sg_weapon_mode(d)!=1)sg_lab_fail("suicide control");
  uint32_t saved=g_ecx,arg=0;g_ecx=d;uint32_t weapon=hle_call_guest(MEM32(MEM32(d)+185*4),1,&arg);g_ecx=saved;if(weapon!=1)sg_lab_fail("engine weapon selector");MEM8(d+0x83)=0;
  old=MEM32(d+0x10);g_ecx=d;hle_call_guest(0x5F65F0,0,NULL);g_ecx=saved;stage=3;fprintf(stderr,"[drone-lab] elite three, tank five; actual engine selects suicide weapon; remove one drone\n");
 }
 else if(stage==3&&u->ready[0]){if(sg_lab_alive(u)!=2)sg_lab_fail("other live drone slots changed");u->ready[0]=f;stage=4;}
 else if(stage==4&&sg_lab_alive(u)==3){if(MEM32(u->children[0]+0x10)==old)sg_lab_fail("replacement reused identity");stage=5;fprintf(stderr,"[drone-lab] PASS independent replacement; untouched remaining two slots\n");}
 if(stage==5&&!reported){reported=1;fprintf(stderr,"[drone-lab] PASS actual native 1/2/3 drone creation, 5-drone tank, selection command, engine suicide weapon switch, per-slot regeneration\n");}
 if(stage==5)sg_fire_lab(pilot,f);
 if(f>650&&stage<5)sg_lab_fail("timed out");
}
