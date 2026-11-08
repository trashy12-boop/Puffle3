// SPDX-License-Identifier: GPL-2.0-only
/* Settings layout/navigation adapted from PS3ME ps3/src/InGameMenu.cpp.
 * SDL drawing and Java integration replaced by RSX and RpProfile. */
/* Native frontend and Ruffle player loop. */
#include "browser.h"
#include "controls.h"
#include "profile.h"
#include "ps3me_ui.h"
#include "rsx_profile.h"
#include "player.h"
#include <io/pad.h>
#include <rsxkit/rsxkit.h>
#include <rsxkit/ui.h>
#include <rsxkit/sysfont.h>
#include <sys/systime.h>
#include <sys/process.h>
SYS_PROCESS_PARAM(1001,0x800000);
#include <sysutil/sysutil.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

#define ROOT_HDD "/dev_hdd0/ruffle/games"
#define ROOT_USB "/dev_usb000/ruffle/games"
#define PROFILES "/dev_hdd0/ruffle/profiles"
static RpBrowser browser;
static RpProfile profile;
static RpInput input;
static RpPad pad;
static uint32_t previous;
static int selection, scroll, page, control_index, binding_slot, usb, quit_requested;
static char selected_game[RP_PATH_MAX], message[160];
extern uint64_t rp_rust_probe(void);
static FILE *logfile;
static unsigned events_seen;
static void *player;
static rsxkit_fb *movie_fb;
extern int rp_rsx_init(void);
extern void rp_rsx_shutdown(void);
extern int rp_rsx_current(void);
extern void rp_rsx_present(void);
extern void rp_rsx_software(void);
extern void rp_rsx_cache_reset(void);
extern void rp_rsx_stats(uint64_t *,uint64_t *);
extern void rp_rsx_reason_stats(uint64_t *);
static int pause_row;
static u64 last_tick_us,next_tick_us,stats_since,tick_cost,render_cost,audio_cost;
static unsigned tick_count,render_count,audio_count;
static char timing[180]="Performance sample pending";
static RpRsxProfile previous_rsx_profile;
static void reset_tick_clock(void){last_tick_us=sysGetSystemTime();next_tick_us=last_tick_us;}

static void game_defaults(RpProfile *p) {
#ifdef RP_MARIO63
    rp_profile_mario63(p);
#else
    rp_profile_for_game(p,selected_game);
#endif
}
#ifndef RP_TITLE_ID
#define RP_TITLE_ID "RFLP00001"
#endif
static void launch_game(void);
static void open_controls(void);
void rp_present(const unsigned char *p,uint32_t w,uint32_t h) { rp_rsx_software(); if(movie_fb) rsxkit_fb_upload(movie_fb,p,w,h,w*4,RSXKIT_RGBA8888); }
enum { PAGE_BROWSER, PAGE_DETAILS, PAGE_CONTROLS, PAGE_INPUT, PAGE_GAME };
static const char *button_names[RP_BUTTONS]={"Cross","Circle","Square","Triangle","D-pad Up","D-pad Down","D-pad Left","D-pad Right","L1","L2","R1","R2","L3","R3","Start","Select","Left stick Up","Left stick Down","Left stick Left","Left stick Right","Right stick Up","Right stick Down","Right stick Left","Right stick Right"};
typedef struct { unsigned value; const char *name; } Choice;
static const Choice choices[]={ {0,"None"},{RP_MOUSE_LEFT,"Mouse left"},{RP_MOUSE_RIGHT,"Mouse right"},{32,"Space"},{13,"Enter"},{16,"Shift"},{17,"Ctrl"},{18,"Alt"},{27,"Escape"},{37,"Left arrow"},{38,"Up arrow"},{39,"Right arrow"},{40,"Down arrow"},{'A',"A"},{'B',"B"},{'C',"C"},{'D',"D"},{'E',"E"},{'P',"P"},{'Q',"Q"},{'S',"S"},{'W',"W"},{'X',"X"},{'Z',"Z"} };
static void note(const char *s) { if(logfile) { fprintf(logfile,"%s\n",s); fflush(logfile); } }
static void sample_performance(u64 now){
 if(!stats_since){stats_since=now;return;}
 if(now-stats_since<2000000)return;
 if(player&&page==PAGE_GAME&&!input.menu_open){
  snprintf(timing,sizeof(timing),"Draw %.1f/s %.1fms | logic %.1fms | audio %.1fms",
   render_count*1000000.0/(now-stats_since),render_count?render_cost/(1000.0*render_count):0.,
   tick_count?tick_cost/(1000.0*tick_count):0.,audio_count?audio_cost/(1000.0*audio_count):0.);
  note(timing);
  RpVmStats vm={0};
  if(rp_player_vm_stats(&vm)) {
   char vline[240];
   snprintf(vline,sizeof(vline),"VM profile sim %.2f/s vm %lluus collision %lluus GC %lluus ops %llu gets %llu sets %llu calls %llu hits %llu",
    vm.frames*1000000.0/(now-stats_since),(unsigned long long)vm.vm_us,(unsigned long long)vm.collision_us,(unsigned long long)vm.gc_us,(unsigned long long)vm.opcodes,(unsigned long long)vm.gets,(unsigned long long)vm.sets,(unsigned long long)vm.calls,(unsigned long long)vm.hit_tests);note(vline);
  }
  uint64_t mesh[3]={0};rp_player_mesh_stats(player,mesh);char meshline[160];
  snprintf(meshline,sizeof(meshline),"GPU mesh buffers: %llu shapes, %llu KiB packed / %llu KiB previous capacity",(unsigned long long)mesh[2],(unsigned long long)(mesh[1]/1024),(unsigned long long)(mesh[0]/1024));note(meshline);
  struct mallinfo heap=mallinfo();
  snprintf(meshline,sizeof(meshline),"C heap: arena %u KiB allocated %u KiB free %u KiB top %u KiB",(unsigned)heap.arena/1024,(unsigned)heap.uordblks/1024,(unsigned)heap.fordblks/1024,(unsigned)heap.keepcost/1024);note(meshline);
  RpCacheStats cache={0};rp_player_cache_stats(player,&cache);char details[160];
  snprintf(details,sizeof(details),"Path calls %llu | cache hits %llu / misses %llu | cache %llu KiB",(unsigned long long)cache.paths,(unsigned long long)cache.hits,(unsigned long long)cache.misses,(unsigned long long)(cache.bytes/1024));note(details);
  uint64_t gpu=0,cpu=0;rp_rsx_stats(&gpu,&cpu);
  snprintf(details,sizeof(details),"RSX rendered frames %llu | software rendered frames %llu",(unsigned long long)gpu,(unsigned long long)cpu);note(details);
  snprintf(details,sizeof(details),"Rust heap live %llu KiB peak %llu KiB",(unsigned long long)(rp_rust_bytes()/1024),(unsigned long long)(rp_rust_peak()/1024));note(details);
  uint64_t why[9]={0};rp_rsx_reason_stats(why);
  snprintf(details,sizeof(details),"RSX fallback gradients %llu masks %llu blend %llu lines/cache %llu other %llu",(unsigned long long)why[1],(unsigned long long)why[2],(unsigned long long)why[3],(unsigned long long)why[4],(unsigned long long)(why[5]+why[6]+why[7]+why[8]));note(details);
  snprintf(details,sizeof(details),"RSX detail missing bitmap %llu missing mesh %llu perspective %llu stage3d %llu",(unsigned long long)why[5],(unsigned long long)why[6],(unsigned long long)why[7],(unsigned long long)why[8]);note(details);
  RpRsxProfile snapshot;rp_rsx_profile_snapshot(&snapshot);
  snprintf(details,sizeof(details),"RSX interval: %llu draws, %llu vertices; upload %llu textures / %llu KiB",
    (unsigned long long)(snapshot.draw_calls-previous_rsx_profile.draw_calls),
    (unsigned long long)(snapshot.vertices-previous_rsx_profile.vertices),
    (unsigned long long)(snapshot.uploaded_textures-previous_rsx_profile.uploaded_textures),
    (unsigned long long)((snapshot.uploaded_bytes-previous_rsx_profile.uploaded_bytes)/1024));note(details);
  snprintf(details,sizeof(details),"RSX interval: reused %llu, convert %lluus, fence wait %lluus, sync %lluus, failures %llu",
    (unsigned long long)(snapshot.reused_textures-previous_rsx_profile.reused_textures),
    (unsigned long long)(snapshot.conversion_us-previous_rsx_profile.conversion_us),
    (unsigned long long)(snapshot.fence_wait_us-previous_rsx_profile.fence_wait_us),
    (unsigned long long)(snapshot.texture_sync_us-previous_rsx_profile.texture_sync_us),
    (unsigned long long)(snapshot.failed_draws-previous_rsx_profile.failed_draws));note(details);
  snprintf(details,sizeof(details),"RSX cache: %llu resident hits; %llu resident uploads; %llu transient uploads",
   (unsigned long long)(snapshot.persistent_hits-previous_rsx_profile.persistent_hits),
   (unsigned long long)(snapshot.persistent_uploads-previous_rsx_profile.persistent_uploads),
   (unsigned long long)(snapshot.transient_uploads-previous_rsx_profile.transient_uploads));note(details);
  snprintf(details,sizeof(details),"RSX batches %llu merged %llu",(unsigned long long)(snapshot.submitted_batches-previous_rsx_profile.submitted_batches),(unsigned long long)(snapshot.merged_draws-previous_rsx_profile.merged_draws));note(details);
  snprintf(details,sizeof(details),"RSX staging resident %u KiB",rp_rsx_arena_bytes()/1024);note(details);
  previous_rsx_profile=snapshot;
 }
 rp_log_flush();stats_since=now;tick_cost=render_cost=audio_cost=0;tick_count=render_count=audio_count=0;
}
static void system_cb(u64 status,u64 param,void *user) { (void)param;(void)user; if(status==SYSUTIL_EXIT_GAME) quit_requested=1; }
static int16_t axis_value(unsigned value) { int n=((int)value-128)*256; if(n>32767)n=32767; if(n< -32768)n= -32768; return (int16_t)n; }
static void read_pad(void) {
    padInfo info; memset(&info,0,sizeof(info));
    if(ioPadGetInfo(&info)!=PAD_OK) return;
    int port=-1; for(int i=0;i<MAX_PORT_NUM;i++) if(info.status[i]) { port=i; break; }
    if(port<0) { memset(&pad,0,sizeof(pad)); return; }
    padData d; memset(&d,0,sizeof(d)); if(ioPadGetData(port,&d)!=PAD_OK || d.len==0) return;
    pad.buttons=0;
#define B(field,bit) do { if(d.field) pad.buttons |= 1u<<(bit); } while(0)
    B(BTN_CROSS,RP_CROSS); B(BTN_CIRCLE,RP_CIRCLE); B(BTN_SQUARE,RP_SQUARE); B(BTN_TRIANGLE,RP_TRIANGLE);
    B(BTN_UP,RP_UP); B(BTN_DOWN,RP_DOWN); B(BTN_LEFT,RP_LEFT); B(BTN_RIGHT,RP_RIGHT);
    B(BTN_L1,RP_L1); B(BTN_L2,RP_L2); B(BTN_R1,RP_R1); B(BTN_R2,RP_R2);
    B(BTN_L3,RP_L3); B(BTN_R3,RP_R3); B(BTN_START,RP_START); B(BTN_SELECT,RP_SELECT);
#undef B
    pad.lx=axis_value(d.ANA_L_H); pad.ly=axis_value(d.ANA_L_V); pad.rx=axis_value(d.ANA_R_H); pad.ry=axis_value(d.ANA_R_V);
}
static void emit(void *ctx,const RpEvent *e) { (void)ctx; next_tick_us=0; events_seen++; if(player && e->type!=RP_EVENT_MENU) rp_player_event(player,e); if(e->type==RP_EVENT_MENU) note(e->down?"input test: menu opened":"input test: menu closed"); }
static const char *binding_name(unsigned value) { for(unsigned i=0;i<sizeof(choices)/sizeof(choices[0]);i++) if(choices[i].value==value) return choices[i].name; return "Other key"; }
static void save_profile(void) {
    if(rp_profile_save(PROFILES,selected_game,&profile)==0) snprintf(message,sizeof(message),"Profile saved");
    else snprintf(message,sizeof(message),"Could not save profile");
}
static void cycle_binding(int delta) {
    unsigned v=profile.binding[control_index][binding_slot], i=0,n=sizeof(choices)/sizeof(choices[0]);
    while(i<n && choices[i].value!=v) i++;
    if(i==n)i=0;
    i=(unsigned)((int)i+delta+(int)n)%n; profile.binding[control_index][binding_slot]=(uint16_t)choices[i].value;
    save_profile();
}
/* PS3ME scrolling rows, adapted for Flash keyboard/mouse bindings. */
enum { ROW_STICK=RP_BUTTONS, ROW_SPEED, ROW_ZONE, ROW_ACCEL, ROW_RENDERER, ROW_SAVE, ROW_BACK, SETTINGS_ROWS };
static void change_setting(int delta) {
    if(control_index<RP_BUTTONS) { cycle_binding(delta); return; }
    switch(control_index) {
    case ROW_STICK: profile.mouse_stick=(profile.mouse_stick+3+delta)%3; break;
    case ROW_SPEED: { int n=profile.mouse_speed+delta*30; profile.mouse_speed=n<60?1200:n>1200?60:n; break; }
    case ROW_ZONE: { int n=profile.deadzone+delta*5; profile.deadzone=n<0?80:n>80?0:n; break; }
    case ROW_ACCEL: { int n=profile.acceleration+delta*10; profile.acceleration=n<0?200:n>200?0:n; break; }
    case ROW_RENDERER: profile.renderer^=1; if(player)rp_player_set_software(player,profile.renderer); break;
    case ROW_SAVE: save_profile(); return;
    case ROW_BACK: page=player?PAGE_GAME:PAGE_BROWSER; return;
    }
    save_profile();
}
/* PS3ME 01.16-inspired 1280x720 geometry and palette. These panels are
 * rendered by RSXKit, not by an SDL framebuffer or an extra game overlay. */
static void draw(void) {
    rsxkit_frame_begin(0x17405b);
    if(page==PAGE_GAME) {
        if(!input.menu_open){
            u64 begin=sysGetSystemTime();
            if(rp_player_render(player)){render_cost+=sysGetSystemTime()-begin;render_count++;}
            if(rp_rsx_current()) rp_rsx_present();
            else {rsxkit_view view={RSXKIT_SCALE_FIT,RSXKIT_FILTER_LINEAR,4.0f/3.0f};rsxkit_fb_draw(movie_fb,&view,NULL);}
        }
        rsxkit_ui_begin(1280,720);
        if(input.menu_open) {
            rp_ps3me_background(0);
            rp_ps3me_label(80,44,"Puffle 3 / In-game settings",RP_UI_TITLE,1120);
            rp_ps3me_label(80,106,"L3 + R3   /   Settings for this game",RP_UI_NORMAL,1120);
            const char *labels[]={"Resume game","Restart SWF","Controller settings","Return to game library"};
            for(int i=0;i<4;i++){
                float y=176+i*46.f;
                if(i==pause_row) rp_ps3me_highlight(70,y-5,1130,40);
                rp_ps3me_label(90,y,labels[i],RP_UI_NORMAL,1050);
            }
            char memory[120];
            snprintf(memory,sizeof(memory),"Ruffle memory: %u KiB / peak %u KiB",(unsigned)(rp_rust_bytes()/1024),(unsigned)(rp_rust_peak()/1024));
            rp_ps3me_label(80,516,memory,RP_UI_SMALL,1100);
            rp_ps3me_label(80,586,timing,RP_UI_SMALL,1110);
            rp_ps3me_label(80,640,"Up/Down: select   X: confirm   Circle: resume",RP_UI_NORMAL,1120);
            rp_ps3me_label(80,681,"Ruffle playback and audio are paused in this menu.",RP_UI_SMALL,1120);
        } else if(profile.mouse_stick) {
            float x=160+(float)input.cursor_x*3,y=(float)input.cursor_y*3;
            rsxkit_ui_rect(x-6,y,13,2,0xffffffff);rsxkit_ui_rect(x,y-6,2,13,0xffffffff);
        }
        rsxkit_ui_end();rsxkit_frame_end();return;
    }
    rsxkit_ui_begin(1280,720);
    rp_ps3me_background(page==PAGE_BROWSER);
    char line[256];
    if(page==PAGE_BROWSER) {
        /* Copy PS3ME BrowserUI.cpp's arrangement, selection offsets, sidebar,
         * horizontal rules and footer. This changes only the browser UI. */
        rp_ps3me_label(76,38,"Puffle 3",RP_UI_TITLE,225);
        rp_ps3me_label(255,48,"Games",RP_UI_NORMAL,650);
        rp_ps3me_label(1000,48,"01.08 / Native Flash",RP_UI_SMALL,240);
        rp_ps3me_label(80,108,browser.current,RP_UI_SMALL,1120);
        rp_ps3me_rule(142);
        if(!browser.count) rp_ps3me_label(92,302,"No SWF games here. Select: input test",RP_UI_NORMAL,1050);
        for(int i=selection-2;i<selection+4;i++){
            if(i<0 || i>=(int)browser.count)continue;
            int y=rp_ps3me_row_y(i,selection);
            if(y<150 || y>550)continue;
            const RpEntry *e=&browser.items[i];
            int focused=i==selection;
            if(focused)rp_ps3me_highlight(72,y-13,623,94);
            rp_ps3me_file_icon(92,(float)y,focused?70:40,e->directory);
            rp_ps3me_label(190,(float)y,e->name,focused?RP_UI_TITLE:RP_UI_NORMAL,480);
            if(focused){
                rp_ps3me_label(193,y+44,e->directory?"Folder":"Flash movie",RP_UI_SMALL,450);
                if(!e->directory){
                    rp_ps3me_file_icon(886,224,176,0);
                    rp_ps3me_label(795,431,e->name,RP_UI_NORMAL,385);
                    rp_ps3me_label(795,473,"SWF / Flash movie",RP_UI_SMALL,385);
                    snprintf(line,sizeof(line),"%llu KiB / Controls: Square",(unsigned long long)((e->size+1023)/1024));
                    rp_ps3me_label(795,520,line,RP_UI_SMALL,385);
                }
            }
        }
        if(message[0])rp_ps3me_label(80,595,message,RP_UI_SMALL,1120);
        rp_ps3me_rule(628);
        rp_ps3me_label(80,648,"X Open   O Parent   Square Controls   Select Input test",RP_UI_SMALL,1120);
        rp_ps3me_label(80,680,"Start HDD / USB   L3 + R3 Pause in game",RP_UI_SMALL,1120);
    } else if(page==PAGE_CONTROLS) {
        rp_ps3me_label(80,44,"Puffle 3 / In-game settings",RP_UI_TITLE,1120);
        snprintf(line,sizeof(line),"Settings for this SWF   /   Binding slot %d of %d",binding_slot+1,RP_BINDINGS);
        rp_ps3me_label(80,106,line,RP_UI_NORMAL,1120);
        int first=control_index>3?control_index-3:0;
        for(int i=first;i<SETTINGS_ROWS && i<first+8;i++) {
            float y=176+(i-first)*46.f;
            if(i==control_index)rp_ps3me_highlight(70,y-5,1130,40);
            if(i<RP_BUTTONS)snprintf(line,sizeof(line),"%-18s   %s",button_names[i],binding_name(profile.binding[i][binding_slot]));
            else if(i==ROW_STICK)snprintf(line,sizeof(line),"Mouse cursor: %s",profile.mouse_stick==0?"Disabled":profile.mouse_stick==1?"Left stick":"Right stick");
            else if(i==ROW_SPEED)snprintf(line,sizeof(line),"Cursor speed: %u",profile.mouse_speed);
            else if(i==ROW_ZONE)snprintf(line,sizeof(line),"Stick deadzone: %u%%",profile.deadzone);
            else if(i==ROW_RENDERER)snprintf(line,sizeof(line),"Renderer: %s",profile.renderer?"Software":"Automatic RSX / fallback");
            else if(i==ROW_ACCEL)snprintf(line,sizeof(line),"Cursor acceleration: %u%%",profile.acceleration);
            else snprintf(line,sizeof(line),"%s",i==ROW_SAVE?"Save settings for this game":"Return to game library");
            rp_ps3me_label(90,y,line,RP_UI_NORMAL,1080);
        }
        rp_ps3me_label(80,586,message,RP_UI_NORMAL,1120);
        rp_ps3me_label(80,640,"Up/Down: select   Left/Right or X: change   Circle: back",RP_UI_NORMAL,1120);
        rp_ps3me_label(80,681,"L1/R1: select keyboard and mouse binding slot.",RP_UI_SMALL,1120);
    } else {
        rp_ps3me_label(80,44,page==PAGE_DETAILS?"Puffle 3 / File details":"Puffle 3 / Controller test",RP_UI_TITLE,1120);
        if(page==PAGE_DETAILS){
            rp_ps3me_label(80,106,selected_game,RP_UI_SMALL,1120);
            rp_ps3me_label(80,176,message,RP_UI_NORMAL,1120);
            rp_ps3me_label(80,222,"SWF load failed. See /dev_hdd0/ruffle/ruffle-core.log",RP_UI_NORMAL,1120);
            rp_ps3me_label(80,640,"Square: edit settings   Circle: return to game library",RP_UI_NORMAL,1120);
        } else {
            rp_ps3me_label(80,106,"Input diagnostics / L3 + R3 toggles the menu",RP_UI_NORMAL,1120);
            snprintf(line,sizeof(line),"Cursor %.0f, %.0f   Events %u",input.cursor_x,input.cursor_y,events_seen);
            rp_ps3me_label(80,176,line,RP_UI_NORMAL,1120);
            rp_ps3me_label(80,222,input.menu_open?"MENU OPEN: input suspended":"MENU CLOSED: input active",RP_UI_NORMAL,1120);
            rp_ps3me_label(80,640,"Start: return to browser",RP_UI_NORMAL,1120);
        }
    }
    rsxkit_ui_end();rsxkit_frame_end();
}
static void open_controls(void) { game_defaults(&profile); if(rp_profile_load(PROFILES,selected_game,&profile)!=0) snprintf(message,sizeof(message),"Default profile (changes save automatically)"); else snprintf(message,sizeof(message),"Saved profile loaded"); control_index=0; binding_slot=0; page=PAGE_CONTROLS; }
static void browse(int root_usb) {
    usb=root_usb; selection=scroll=0;
    if(rp_browser_open(&browser,usb?ROOT_USB:ROOT_HDD)!=0) snprintf(message,sizeof(message),"Cannot open game directory");
}
static void resume_game(void) {
    rp_input_release_all(&input,emit,NULL);input.menu_open=false;input.release_gate=true;rp_audio_pause(0);reset_tick_clock();page=PAGE_GAME;
}
static void launch_game(void) {
    rp_audio_reset();rp_rsx_cache_reset();
    if(player) {rp_input_release_all(&input,emit,NULL);rp_player_close(player);player=NULL;}
    FILE *f=fopen(selected_game,"rb"); if(!f){snprintf(message,sizeof(message),"Cannot open SWF");page=PAGE_DETAILS;return;}
    if(fseek(f,0,SEEK_END)!=0){fclose(f);return;}
    long size=ftell(f);rewind(f);
    if(size<8 || size>16*1024*1024){fclose(f);snprintf(message,sizeof(message),"SWF file limit: 16 MiB");page=PAGE_DETAILS;return;}
    unsigned char *data=malloc((size_t)size);
    if(!data){fclose(f);snprintf(message,sizeof(message),"Not enough memory to load SWF");page=PAGE_DETAILS;return;}
    size_t n=fread(data,1,(size_t)size,f);fclose(f);
    if(n==(size_t)size)player=rp_player_open(data,n,320,240,selected_game);
    free(data);
    if(!player){snprintf(message,sizeof(message),"Ruffle rejected this SWF");page=PAGE_DETAILS;return;}
    game_defaults(&profile);rp_profile_load(PROFILES,selected_game,&profile);
    rp_player_set_software(player,profile.renderer);
    rp_audio_pause(0);
    rp_input_init(&input,320,240);input.release_gate=true;pause_row=0;reset_tick_clock();stats_since=0;tick_cost=render_cost=audio_cost=0;tick_count=render_count=audio_count=0;page=PAGE_GAME;note("Ruffle SWF launched");
}
static void update(uint32_t pressed,double dt) {
    if(page==PAGE_GAME) {
        bool was_open=input.menu_open;
        rp_input_update(&input,&profile,pad,dt,320,240,emit,NULL);
        if(was_open!=input.menu_open){rp_audio_pause(input.menu_open);reset_tick_clock();pause_row=0;return;}
        if(!input.menu_open){
 u64 now=sysGetSystemTime();
 if(now>=next_tick_us){double elapsed=(now-last_tick_us)/1000.0;last_tick_us=now;
  rp_player_tick(player,elapsed);tick_cost+=sysGetSystemTime()-now;tick_count++;
  next_tick_us=now+rp_player_next_tick_us(player);}
 return;
}
        if((pressed&(1u<<RP_UP))&&pause_row>0)pause_row--;
        if((pressed&(1u<<RP_DOWN))&&pause_row<3)pause_row++;
        if(pressed&(1u<<RP_CIRCLE)){resume_game();return;}
        if(pressed&(1u<<RP_CROSS)) {
            if(pause_row==0)resume_game();
            else if(pause_row==1)launch_game();
            else if(pause_row==2)open_controls();
            else {rp_input_release_all(&input,emit,NULL);rp_audio_reset();rp_player_close(player);player=NULL;
                char stats[100];snprintf(stats,sizeof(stats),"player closed: Rust heap %u KiB, peak %u KiB",(unsigned)(rp_rust_bytes()/1024),(unsigned)(rp_rust_peak()/1024));note(stats);page=PAGE_BROWSER;}
        }
        return;
    }
    if(page==PAGE_INPUT) { rp_input_update(&input,&profile,pad,dt,640,480,emit,NULL); if(input.menu_open && selected_game[0] && (pressed&(1u<<RP_SQUARE))) { rp_input_release_all(&input,emit,NULL); open_controls(); return; } if(pressed&(1u<<RP_START)) { rp_input_release_all(&input,emit,NULL); page=PAGE_BROWSER; } return; }
    if(page==PAGE_BROWSER) {
        if((pressed&(1u<<RP_UP))&&selection>0) selection--;
        if((pressed&(1u<<RP_DOWN))&&selection+1<(int)browser.count) selection++;
        if(selection<scroll)scroll=selection;
        if(selection>=scroll+9)scroll=selection-8;
        if(pressed&(1u<<RP_START)) { browse(!usb); return; }
        if(pressed&(1u<<RP_SELECT)) { if(selected_game[0]) { game_defaults(&profile); rp_profile_load(PROFILES,selected_game,&profile); } else game_defaults(&profile); rp_input_init(&input,640,480); events_seen=0; page=PAGE_INPUT; return; }
        if(pressed&(1u<<RP_CIRCLE)) { if(rp_browser_parent(&browser)==0) selection=scroll=0; return; }
        if(browser.count && ((pressed&(1u<<RP_CROSS))||(pressed&(1u<<RP_SQUARE)))) {
            if(browser.items[selection].directory) { if((pressed&(1u<<RP_CROSS))&&rp_browser_enter(&browser,(unsigned)selection)==0) selection=scroll=0; return; }
            if(rp_browser_path(&browser,(unsigned)selection,selected_game,sizeof(selected_game))) return;
            if(pressed&(1u<<RP_SQUARE)) { open_controls(); return; }
            uint32_t size; unsigned version;
            if(rp_swf_header(selected_game,&size,&version)==0) {launch_game();return;}
            else snprintf(message,sizeof(message),"Invalid, unreadable, or oversized SWF header.");
            page=PAGE_DETAILS; return;
        }
    } else if(page==PAGE_DETAILS) {
        if(pressed&(1u<<RP_SQUARE)) open_controls(); else if(pressed&(1u<<RP_CIRCLE)) page=PAGE_BROWSER;
    } else if(page==PAGE_CONTROLS) {
        if((pressed&(1u<<RP_UP))&&control_index>0)control_index--;
        if((pressed&(1u<<RP_DOWN))&&control_index<SETTINGS_ROWS-1)control_index++;
        if(pressed&(1u<<RP_LEFT))change_setting(-1);
        if(pressed&((1u<<RP_RIGHT)|(1u<<RP_CROSS)))change_setting(1);
        if(pressed&(1u<<RP_L1))binding_slot=(binding_slot+RP_BINDINGS-1)%RP_BINDINGS;
        if(pressed&(1u<<RP_R1))binding_slot=(binding_slot+1)%RP_BINDINGS;
        if(pressed&(1u<<RP_CIRCLE))page=player?PAGE_GAME:PAGE_BROWSER;
    }
}
int main(void) {
    mkdir("/dev_hdd0/ruffle",0777); mkdir(ROOT_HDD,0777); mkdir(PROFILES,0777);
    logfile=fopen("/dev_hdd0/ruffle/ruffle-ps3-dev.log","w"); note("Puffle 3 Emulator 01.08 v4 starting");
    if(rp_rust_probe()!=66) { note("Rust ABI probe failed"); return 2; }
    note("Rust ABI probe passed");
    if(ioPadInit(MAX_PORT_NUM)!=PAD_OK) note("pad init failed");
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0,system_cb,NULL);
    rsxkit_config config={.host_size=16u*1024u*1024u};
    int result=rsxkit_init(&config); if(result) { note("RSX initialization failed"); if(logfile)fclose(logfile); return 1; }
    if(rsxkit_ui_init()!=0) { note("RSX UI initialization failed"); rsxkit_shutdown(); if(logfile)fclose(logfile); return 1; }
    if(rsxkit_sysfont_open_file("/dev_hdd0/game/" RP_TITLE_ID "/USRDIR/ui/DejaVuSans.ttf")==RSXKIT_OK) {
        rsxkit_ui_font(rsxkit_sysfont_source());
        note("Bundled DejaVuSans font loaded");
    } else if(rsxkit_sysfont_open(RSXKIT_SYSFONT_LATIN,0)==RSXKIT_OK) {
        rsxkit_ui_font(rsxkit_sysfont_source());
        note("PS3 system font active for PS3ME-style menus");
    } else note("System font unavailable; RSXKit fallback font active");
    note(rp_rsx_init()?"RSX mesh renderer initialized":"RSX mesh renderer unavailable; using software");
    movie_fb=rsxkit_fb_create(640,480);
    if(!movie_fb){note("movie framebuffer allocation failed");rsxkit_shutdown();return 3;}
    if(rp_audio_init()!=0)note("PS3 audio initialization failed; playback will be silent");
    browse(0); game_defaults(&profile); note("main menu ready");
#ifdef RP_MARIO63
    snprintf(selected_game,sizeof(selected_game),"%s/SuperMario63.swf",ROOT_HDD);
    launch_game();
#endif
    u64 last=sysGetSystemTime();
    while(!quit_requested) {
        sysUtilCheckCallback(); read_pad(); u64 now=sysGetSystemTime(); double dt=(now-last)/1000000.0; last=now;
        uint32_t pressed=pad.buttons&~previous; previous=pad.buttons; update(pressed,dt);
        if(player&&page==PAGE_GAME&&!input.menu_open){u64 begin=sysGetSystemTime();rp_audio_pump(player);audio_cost+=sysGetSystemTime()-begin;audio_count++;}
        sample_performance(sysGetSystemTime());
        if(rsxkit_frame_ready()) draw(); else sysUsleep(1000);
    }
    note("shutdown"); rp_log_flush();rp_audio_shutdown();if(player)rp_player_close(player);rp_rsx_shutdown();if(movie_fb)rsxkit_fb_destroy(movie_fb);ioPadEnd(); rsxkit_ui_font(NULL);rsxkit_sysfont_close();rsxkit_shutdown(); if(logfile)fclose(logfile); return 0;
}
