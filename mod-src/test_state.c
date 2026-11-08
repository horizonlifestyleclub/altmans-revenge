#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static unsigned char memory[0x2000000];
#define MEM8(p) (*(uint8_t*)(memory+(uint32_t)(p)))
#define MEM16(p) (*(uint16_t*)(memory+(uint32_t)(p)))
#define MEM32(p) (*(uint32_t*)(memory+(uint32_t)(p)))
#define RA2_TARGET_GAMEMD 1
#define SG_STATE_TEST 1
#define RA2_GAME_FRAME_VA 0xA8ED84
static void* hle_ptr(uint32_t p){assert(p<sizeof memory);return memory+p;}
static const char*test_mod="Singularity";
const char*mods_active(void){return test_mod;}
typedef struct hle_menu_item {const char*label;const char*hint;int id,checked;const struct hle_menu_item*sub;int nsub;}hle_menu_item_t;
static char hud_status[256];
void hle_screen_status(const char*t){snprintf(hud_status,sizeof hud_status,"%s",t);}
static int menus;
void hle_screen_menu(const hle_menu_item_t*items,int n,void(*chosen)(int)){assert(n>0);menus++;}
#include "../../outputs/redalert2-recomp/src/linux/singularity.h"
static uint32_t H=0x1000000,E=0x1008000,A=0x1010000,B=0x1100000,T=0x1200000,U=0x1300000;
static void vec(uint32_t v,uint32_t addr,const uint32_t*data,int n){MEM32(v+4)=addr;MEM32(v+0x10)=n;memcpy(memory+addr,data,n*4);}
static void step(void){MEM32(RA2_GAME_FRAME_VA)+=15;sg_tick();for(int i=0;i<3;i++){uint32_t h=sg_houses[i].ptr;MEM32(h+0x53A8)=i?sg_drain(B+i*0x1000,50):sg_drain(B,800);}}
int main(void){
 MEM32(0xA8B238)=5;MEM32(0xA83D4C)=H;
 uint32_t houses[]={H,E,A},buildings[]={B,B+0x1000,B+0x2000},units[]={U};
 vec(0xA80228,0x1500000,houses,3);vec(0xA8EB40,0x1501000,buildings,3);vec(0xA8EC78,0x1502000,units,1);
 for(int i=0;i<3;i++){uint32_t h=houses[i],b=buildings[i],t=T+i*0x1000;MEM32(h+0x30)=i;MEM32(h+0x53A4)=3000;MEM32(h+0x53A8)=i?50:800;MEM32(b)=0x7E3EBC;MEM8(b+0x90)=1;MEM32(b+0x6c)=1500;MEM32(b+0x21c)=h;MEM32(b+0x520)=t;MEM32(b+0x10)=100+i;strcpy(hle_ptr(t+0x24),i?"GAPOWR":"SGCOLO");}
 MEM32(H+0x5788)=1u<<2;MEM32(A+0x5788)=1u<<0;
 MEM32(U)=0x7F5C70;MEM8(U+0x90)=1;MEM32(U+0x6c)=100;MEM32(U+0x21c)=E;MEM32(U+0x6c4)=T+0x3000;MEM32(U+0x10)=501;strcpy(hle_ptr(T+0x3024),"MTNK");double speed=1;memcpy(hle_ptr(U+0x580),&speed,8);
 MEM32(E+0x34)=0x1600000;MEM8(0x16001A6)=1;assert(!sg_enemy(H,E));MEM8(0x16001A6)=0;assert(sg_enemy(H,E));
 step();SGHouse*s=sg_house(H);assert(s->state==SG_READY);assert(s->progress==0);assert(sg_blocked(B));
 sg_tech_key(65,1);assert(menus==1);sg_menu_chosen(8101);assert(s->research_started);step();assert(strstr(hud_status,"07:59"));
 for(int i=0;i<200;i++)step();assert(s->state==SG_RESEARCHING);uint32_t progress=s->progress;
 MEM32(H+0x53A4)=0;step();assert(s->state==SG_SUSPENDED&&s->progress==progress);assert(!sg_house(E)->cyber);
 MEM32(H+0x53A4)=3000;for(int i=0;i<300;i++)step();assert(sg_active(s));assert(sg_house(E)->cyber);assert(!sg_house(A)->cyber);assert(!sg_house(H)->cyber);assert(!sg_blocked(B));assert(sg_drain(B+0x1000,50)==100050);
 int failures=0;for(int i=0;i<10;i++){MEM32(RA2_GAME_FRAME_VA)+=15;failures+=sg_blocked(U);}assert(failures==7);
 sg_menu_chosen(8102);step();assert(!sg_house(E)->cyber);assert(sg_drain(B+0x1000,50)==50);
 sg_menu_chosen(8102);step();assert(sg_house(E)->cyber);
 MEM32(B+0x6c)=0;step();assert(s->state==SG_OFFLINE&&!sg_house(E)->cyber);assert(!s->research_started&&s->progress==0);
 MEM32(B+0x6c)=1500;MEM32(B+0x10)=900;step();assert(s->state==SG_READY&&s->progress==0);
 /* Distributed China network: five-node prerequisite and loss of quorum. */
 memset(sg_houses,0,sizeof sg_houses);sg_lastframe=sg_lasttick=0;MEM32(RA2_GAME_FRAME_VA)=0;
 uint32_t nodes[6];for(int i=0;i<5;i++){uint32_t b=B+i*0x1000,t=T+i*0x1000;nodes[i]=b;MEM32(b)=0x7E3EBC;MEM8(b+0x90)=1;MEM32(b+0x6c)=650;MEM32(b+0x21c)=H;MEM32(b+0x520)=t;MEM32(b+0x10)=200+i;char id[16];snprintf(id,sizeof id,"SGNODE%d",i+1);strcpy(hle_ptr(t+0x24),id);}nodes[5]=B+0x5000;MEM32(nodes[5])=0x7E3EBC;MEM8(nodes[5]+0x90)=1;MEM32(nodes[5]+0x6c)=500;MEM32(nodes[5]+0x21c)=E;MEM32(nodes[5]+0x520)=T+0x5000;strcpy(hle_ptr(T+0x5024),"GAPOWR");
 vec(0xA8EB40,0x1501000,nodes,6);MEM32(H+0x53A4)=3000;MEM32(H+0x53A8)=500;step();s=sg_house(H);assert(s->nodes==5&&s->state==SG_READY);sg_menu_chosen(8101);for(int i=0;i<500;i++)step();assert(sg_active(s));MEM32(nodes[4]+0x6c)=0;step();assert(s->state==SG_SUSPENDED&&!sg_house(E)->cyber);MEM32(nodes[4]+0x6c)=650;step();assert(sg_active(s));
 puts("PASS: China five-node research prerequisite, full activation, immediate suspension on node loss, restoration with retained research.");
 MEM32(0xA8B238)=3;assert(!sg_on());assert(sg_drain(B+0x1000,50)==50);
 puts("PASS: research prerequisites, manual start, power pause, ACTIVE, alliance exclusion, blackout toggle, 7/10 targeting failures, destruction reset, rebuilding reset, LAN guard, tech menu.");
 MEM32(0xA8B238)=5;MEM32(U+0x21c)=H;MEM8(U+0x83)=1;MEM32(U+0x6c4)=T+0x8000;
 strcpy(hle_ptr(T+0x8024),"SGLINK");SGUnit*du=sg_state(U);float rank=0;memcpy(hle_ptr(U+0x150),&rank,4);
 assert(sg_capacity(U,du)==1&&sg_regen(U)==450);rank=1;memcpy(hle_ptr(U+0x150),&rank,4);assert(sg_capacity(U,du)==2&&sg_regen(U)==330);
 rank=2;memcpy(hle_ptr(U+0x150),&rank,4);assert(sg_capacity(U,du)==3&&sg_regen(U)==225);
 strcpy(hle_ptr(T+0x8024),"SGPDRONE");assert(sg_weapon_mode(U)==0);assert(sg_drone_command(7,1));assert(sg_weapon_mode(U)==1);sg_drone_command(7,1);assert(sg_weapon_mode(U)==0);
 sg_drone_fired(U,0);assert(!du->spent_frame);sg_drone_fired(U,1);assert(du->spent_frame==MEM32(RA2_GAME_FRAME_VA)+1);
 uint32_t parent=U+0x5000;MEM8(parent+0x90)=1;MEM32(parent+0x6c)=100;MEM32(parent+0x10)=765;MEM32(parent+0x21c)=H;du->controller=parent;du->controller_uid=765;assert(sg_credit_source(U)==parent);MEM32(parent+0x21c)=E;assert(sg_credit_source(U)==U);
 strcpy(hle_ptr(T+0x8024),"SGTANK");assert(sg_capacity(U,du)==0);sg_drone_command(7,1);assert(sg_capacity(U,du)==5);sg_drone_command(7,1);assert(sg_capacity(U,du)==0);
 strcpy(hle_ptr(T+0x8024),"SGLINK");du->children[0]=U+0x2000;du->child_uid[0]=99;du->children[1]=U+0x3000;du->child_uid[1]=98;sg_control_tick(20000);assert(du->ready[0]==20225&&du->ready[1]==20225);du->ready[1]=20300;sg_control_tick(20015);assert(du->ready[0]==20225&&du->ready[1]==20300);
 puts("PASS: rank 1/2/3 capacities and per-slot regeneration; selected-drone suicide toggle; five-slot tank deployment.");

 /* Garrisoned controllers remain active, transport passengers do not. */
 strcpy(hle_ptr(T+0x8024),"SGLINK");MEM32(U)=0x7EB058;MEM32(U+0x6C0)=T+0x8000;MEM8(U+0x81)=1;
 uint32_t occ[]={U};vec(B+0x684,0x1700000,occ,1);assert(sg_controller_anchor(U)==B);MEM32(B+0x21C)=E;assert(!sg_controller_anchor(U));MEM32(B+0x21C)=H;MEM32(B+0x694)=0;assert(!sg_controller_anchor(U));MEM8(U+0x81)=0;assert(sg_controller_anchor(U)==U);
 /* Unseen enemies do not enter or update the commander's observation state. */
 test_mod="Singularity - Adaptive";MEM32(H+0x34)=0x1601000;MEM8(H+0x1EC)=0;
 uint32_t observer=U,target=U+0x6000;MEM32(observer+0x9C)=2560;MEM32(observer+0xA0)=2560;
 MEM32(target)=0x7F5C70;MEM32(target+0x10)=999;MEM8(target+0x90)=1;MEM32(target+0x6C)=400;MEM32(target+0x21C)=E;MEM32(target+0x6C4)=T+0x9000;strcpy(hle_ptr(T+0x9024),"HTNK");MEM32(target+0x9C)=25600;MEM32(target+0xA0)=25600;
 uint32_t observed[]={observer,target};vec(0xA8EC78,0x1502000,observed,2);SGAIBrain*brain=sg_ai_brain(H);sg_ai_observe(brain,100);assert(!brain->armor&&!sg_ai_target_allowed(observer,target));
 MEM32(target+0x9C)=2816;MEM32(target+0xA0)=2560;sg_ai_observe(brain,115);assert(brain->armor==1&&sg_ai_target_allowed(observer,target));int seenx=brain->contacts[0].x;
 MEM32(target+0x9C)=25600;MEM32(target+0x6C)=2;strcpy(hle_ptr(T+0x9024),"ZEP");sg_ai_observe(brain,130);assert(brain->contacts[0].x==seenx&&brain->armor==1&&brain->air==0&&!sg_ai_target_allowed(observer,target));
 MEM32(target+0x9C)=2816;MEM32(target+0x220)=2;assert(!sg_ai_target_allowed(observer,target));sg_ai_observe(brain,2000);assert(brain->armor==0);
 MEM8(target+0x1D8)=1;MEM32(target+0x220)=0;assert(!sg_ai_visible(H,target));MEM8(target+0x1D8)=0;
 uint32_t cell=0x1800000;MEM32(cell)=0x7E4EEC;MEM16(cell+0x24)=100;MEM16(cell+0x26)=100;assert(!sg_ai_destination_allowed(U,cell));sg_ai_issuing=1;assert(sg_ai_destination_allowed(U,cell));sg_ai_issuing=0;

 strcpy(hle_ptr(MEM32(H+0x34)+0x24),"Russians");assert(!sg_ai_eligible(H,"SGOP"));
 strcpy(hle_ptr(MEM32(B+0x520)+0x24),"NAHAND");strcpy(hle_ptr(MEM32(B+0x1000+0x520)+0x24),"NARADR");MEM32(B+0x21C)=MEM32(B+0x1000+0x21C)=H;
 uint32_t techowned[]={U,B,B+0x1000};vec(0xA8EC78,0x1502000,techowned,3);assert(sg_ai_eligible(H,"SGOP"));MEM32(B+0x1000+0x21C)=E;assert(!sg_ai_eligible(H,"SGOP"));MEM32(B+0x1000+0x21C)=H;
 strcpy(hle_ptr(MEM32(H+0x34)+0x24),"Americans");assert(!sg_ai_eligible(H,"SGOP"));assert(!sg_ai_eligible(H,"SREF"));
 strcpy(hle_ptr(MEM32(B+0x520)+0x24),"GAWEAP");strcpy(hle_ptr(MEM32(B+0x1000+0x520)+0x24),"GATECH");assert(sg_ai_eligible(H,"SREF"));MEM32(B+0x1000+0x6C)=0;assert(!sg_ai_eligible(H,"SREF"));
 puts("PASS: explicit country and technology prerequisites reject premature production, enemy-owned prerequisites, and destroyed tech buildings.");
 puts("PASS: garrison anchor excludes transports/enemies; unseen movement/type/health cannot update AI memory; cloaked targeting denied; observations expire.");

}
