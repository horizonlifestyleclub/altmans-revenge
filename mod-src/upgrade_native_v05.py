from pathlib import Path
root=Path.cwd();p=root/'outputs/pcrecomp/runtime/win32hle/screen.c';s=p.read_text()
if 'hle_screen_status(' not in s:
 s=s.replace('static SDL_Texture *g_tex;','''static SDL_Texture *g_tex;
/* Optional single-line game status, supplied by the native host. */
static char g_status[256];
static TTF_Font *g_status_font;
void hle_screen_status(const char *text) { snprintf(g_status,sizeof g_status,"%s",text?text:""); }
static void status_draw(void) {
    if (!g_status[0]) return;
    if (!g_status_font) {
        if (!TTF_WasInit() && TTF_Init()!=0) return;
        const char *p=hle_font_file("MS Sans Serif",0,0);
        if (p) g_status_font=TTF_OpenFont(p,16);
    }
    if (!g_status_font) return;
    SDL_Surface *sf=TTF_RenderUTF8_Solid(g_status_font,g_status,(SDL_Color){130,235,210,255});
    if (!sf) return;
    SDL_Texture *tx=SDL_CreateTextureFromSurface(g_ren,sf);
    SDL_Rect rect={12,12,sf->w+16,sf->h+10};
    SDL_SetRenderDrawColor(g_ren,12,22,26,255);SDL_RenderFillRect(g_ren,&rect);
    SDL_SetRenderDrawColor(g_ren,80,130,135,255);SDL_RenderDrawRect(g_ren,&rect);
    rect.x+=8;rect.y+=5;rect.w=sf->w;rect.h=sf->h;
    if(tx){SDL_RenderCopy(g_ren,tx,NULL,&rect);SDL_DestroyTexture(tx);}
    SDL_FreeSurface(sf);
}
''')
 s=s.replace('SDL_RenderPresent(g_ren);','status_draw();\n    SDL_RenderPresent(g_ren);');p.write_text(s)
p=root/'outputs/pcrecomp/runtime/win32hle/win32hle.h';s=p.read_text()
if 'void hle_screen_status' not in s:s+='\nvoid hle_screen_status(const char *text);\n';p.write_text(s)
p=root/'outputs/redalert2-recomp/src/linux/singularity.h';s=p.read_text()
s=s.replace('if(!sg_on())return;\n    uint32_t f=', 'if(!sg_on()){hle_screen_status("");return;}\n    uint32_t f=')
s=s.replace('||!strcmp(id,"SGGUARD");','||!strcmp(id,"SGGUARD")||!strcmp(id,"SGKAMI")||!strcmp(id,"SGFACTDRONE");')
s=s.replace('if(!strcmp(id,"SGCOLO")||!strncmp(id,"SGNODE",6))return !sg_active(h);','if(!strcmp(id,"SGCOLO")||!strncmp(id,"SGNODE",6)||!strcmp(id,"GAWEAP")||!strcmp(id,"SGFACT"))return !sg_active(h);')
s=s.replace('    (void)elapsed;', '''    SGHouse *player=sg_house(MEM32(0xA83D4C));
    char hud[256]="";
    if(player&&(player->colossus||player->nodes)) {
        int remaining=(int)((SG_RESEARCH_TICKS-player->progress+14)/15)+(int)ceil((1-player->level)*SG_INTEGRATION_TICKS/15.);
        if(sg_active(player)) snprintf(hud,sizeof hud,"SINGULARITY ACTIVE | ENEMY GRID %s | FREE FACTORY SWARMS | F8",player->blackout_enabled?"OFFLINE":"ONLINE");
        else if(player->research_started)snprintf(hud,sizeof hud,"SINGULARITY %02d:%02d | %s | F8",remaining/60,remaining%60,sg_states[player->state]);
        else snprintf(hud,sizeof hud,"SINGULARITY READY | F8 TO START 08:00 COUNTDOWN");
    }
    hle_screen_status(hud);
    (void)elapsed;''')
s=s.replace('"Research: %u%% | Integration: %.0f%%",s->progress*100/SG_RESEARCH_TICKS,s->level*100','"Countdown: %02u:%02u | %s",(unsigned)(((SG_RESEARCH_TICKS-s->progress+14)/15)+(unsigned)ceil((1-s->level)*SG_INTEGRATION_TICKS/15.))/60,(unsigned)(((SG_RESEARCH_TICKS-s->progress+14)/15)+(unsigned)ceil((1-s->level)*SG_INTEGRATION_TICKS/15.))%60,sg_states[s->state]')
p.write_text(s)
p=root/'work/singularity/test_state.c';s=p.read_text();s=s.replace('static int menus;', 'static char hud_status[256];\nvoid hle_screen_status(const char*t){snprintf(hud_status,sizeof hud_status,"%s",t);}\nstatic int menus;');s=s.replace('assert(s->research_started);','assert(s->research_started);step();assert(strstr(hud_status,"07:59"));');p.write_text(s)
