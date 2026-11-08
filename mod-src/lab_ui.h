/* Compiled only into the isolated lab executable. */
#include <SDL2/SDL.h>
static void sg_lab_capture(const char*name){
 SDL_Window*w=SDL_GetWindowFromID(1);if(!w)return;SDL_Renderer*r=SDL_GetRenderer(w);int x,y;if(!r||SDL_GetRendererOutputSize(r,&x,&y))return;
 SDL_Surface*s=SDL_CreateRGBSurfaceWithFormat(0,x,y,32,SDL_PIXELFORMAT_ARGB8888);if(!s)return;
 if(!SDL_RenderReadPixels(r,NULL,SDL_PIXELFORMAT_ARGB8888,s->pixels,s->pitch))SDL_SaveBMP(s,name);SDL_FreeSurface(s);
}
static void sg_lab_tick(void){
 static int stage;static uint32_t since;SGHouse*s=sg_house(MEM32(0xA83D4C));if(!s)return;uint32_t now=SDL_GetTicks();
 if(stage==0&&s->state==SG_READY){sg_tech_key(65,1);since=now;stage=1;}
 else if(stage==1&&now-since>1500){sg_lab_capture("lab-tech-ready.bmp");sg_menu_chosen(8101);SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.scancode=SDL_SCANCODE_ESCAPE;e.key.keysym.sym=SDLK_ESCAPE;SDL_PushEvent(&e);stage=2;}
 else if(stage==2&&sg_active(s)){sg_tech_key(65,1);since=now;stage=3;}
 else if(stage==3&&now-since>1500){sg_lab_capture("lab-tech-active.bmp");fprintf(stderr,"[lab-ui] ACTIVE menu captured; research callback, progression and cyber state reached in native match\n");stage=4;}
}
