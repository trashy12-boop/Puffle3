#define _POSIX_C_SOURCE 200809L
#include "browser.h"
#include "controls.h"
#include "profile.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct { RpEvent events[1024]; unsigned n; } Events;
static void emit(void *ctx,const RpEvent *e) { Events *o=ctx; assert(o->n<1024); o->events[o->n++]=*e; }
static int count(Events *o,RpEventType t,unsigned code,int down) { int n=0; for(unsigned i=0;i<o->n;i++) if(o->events[i].type==t&&o->events[i].code==code&&o->events[i].down==down) n++; return n; }
static void test_input(void) {
    RpProfile p; rp_profile_default(&p); RpInput s; rp_input_init(&s,640,480); Events e={0}; RpPad pad={0};
    p.binding[RP_CROSS][0]='A'; p.binding[RP_SQUARE][0]='A'; p.binding[RP_CROSS][1]=RP_MOUSE_LEFT;
    pad.buttons=1u<<RP_CROSS; rp_input_update(&s,&p,pad,0.016,640,480,emit,&e);
    assert(count(&e,RP_EVENT_KEY,'A',1)==1 && count(&e,RP_EVENT_MOUSE_BUTTON,0,1)==1);
    pad.buttons|=1u<<RP_SQUARE; rp_input_update(&s,&p,pad,0.016,640,480,emit,&e);
    assert(count(&e,RP_EVENT_KEY,'A',1)==1);
    pad.buttons=1u<<RP_SQUARE; rp_input_update(&s,&p,pad,0.016,640,480,emit,&e);
    assert(count(&e,RP_EVENT_KEY,'A',0)==0 && count(&e,RP_EVENT_MOUSE_BUTTON,0,0)==1);
    pad.buttons|=RP_MENU_MASK; rp_input_update(&s,&p,pad,0.016,640,480,emit,&e);
    assert(s.menu_open && count(&e,RP_EVENT_MENU,0,1)==1 && count(&e,RP_EVENT_KEY,'A',0)==1);
    rp_input_update(&s,&p,pad,0.016,640,480,emit,&e); assert(count(&e,RP_EVENT_MENU,0,1)==1);
    pad.buttons=0; rp_input_update(&s,&p,pad,0.016,640,480,emit,&e);
    pad.buttons=RP_MENU_MASK; rp_input_update(&s,&p,pad,0.016,640,480,emit,&e); assert(!s.menu_open && count(&e,RP_EVENT_MENU,0,0)==1);
    pad.buttons=1u<<RP_CROSS; rp_input_update(&s,&p,pad,0.016,640,480,emit,&e); assert(count(&e,RP_EVENT_KEY,'A',1)==1);
    pad.buttons=0; rp_input_update(&s,&p,pad,0.016,640,480,emit,&e);
    pad.buttons=1u<<RP_CROSS; rp_input_update(&s,&p,pad,0.016,640,480,emit,&e); assert(count(&e,RP_EVENT_KEY,'A',1)==2);
    p.binding[RP_L3][0]='Q'; pad.buttons=1u<<RP_L3; rp_input_update(&s,&p,pad,0.016,640,480,emit,&e);
    assert(count(&e,RP_EVENT_KEY,'Q',1)==0);
    pad.buttons=0; rp_input_update(&s,&p,pad,0.016,640,480,emit,&e);
    assert(count(&e,RP_EVENT_KEY,'Q',1)==1 && count(&e,RP_EVENT_KEY,'Q',0)==1);
    pad.buttons=0; pad.lx=32767; for(int i=0;i<100;i++) rp_input_update(&s,&p,pad,0.1,640,480,emit,&e);
    assert(s.cursor_x==639 && count(&e,RP_EVENT_MOUSE_MOVE,0,0)>0);
    rp_input_release_all(&s,emit,&e); p.deadzone=99; assert(!rp_profile_valid(&p));
}
static void test_storage_and_browser(void) {
    char base[]="/tmp/ruffle-ps3-test-XXXXXX"; assert(mkdtemp(base));
    char sub[900],game[900],other[900]; snprintf(sub,sizeof(sub),"%s/sub",base); assert(mkdir(sub,0700)==0);
    snprintf(game,sizeof(game),"%s/Example.SWF",base); FILE *f=fopen(game,"wb"); assert(f);
    const unsigned char h[8]={'F','W','S',9,8,0,0,0}; assert(fwrite(h,1,8,f)==8); fclose(f);
    snprintf(other,sizeof(other),"%s/ignore.txt",base); f=fopen(other,"wb"); assert(f); fclose(f);
    RpBrowser b; assert(rp_browser_open(&b,base)==0 && b.count==2 && b.items[0].directory);
    unsigned ver=0; uint32_t size=0; assert(rp_swf_header(game,&size,&ver)==0 && size==8 && ver==9);
    assert(rp_browser_enter(&b,0)==0 && b.count==0); assert(rp_browser_parent(&b)==0); assert(rp_browser_parent(&b)!=0);
    RpProfile in,out; rp_profile_default(&in); in.binding[RP_CROSS][1]='K'; assert(rp_profile_save(sub,game,&in)==0);
    memset(&out,0,sizeof(out)); assert(rp_profile_load(sub,game,&out)==0); assert(out.binding[RP_CROSS][1]=='K' && out.mouse_speed==in.mouse_speed);
    in.renderer=1; in.mouse_speed=730; assert(rp_profile_save(sub,game,&in)==0);
    assert(rp_profile_load(sub,game,&out)==0 && out.mouse_speed==730 && out.renderer==1);
    assert(rp_profile_load(sub,"/different.swf",&out)!=0);
    /* Corrupt the persisted record and ensure it is rejected. */
    DIR *d=opendir(sub); assert(d); struct dirent *entry; char stored[1200]={0};
    while((entry=readdir(d))) if(strstr(entry->d_name,".rpf")) snprintf(stored,sizeof(stored),"%s/%s",sub,entry->d_name);
    closedir(d); assert(stored[0]);
    /* Legacy RPF1 records keep all bindings and select automatic rendering. */
    unsigned char legacy[1024]; f=fopen(stored,"rb");assert(f);
    size_t legacy_len=fread(legacy,1,sizeof legacy,f);fclose(f);assert(legacy_len>5);
    legacy[3]='1';legacy_len--; /* remove the renderer byte before checksum */
    uint32_t checksum=2166136261u;
    for(size_t j=0;j<legacy_len-4;j++)checksum=(checksum^legacy[j])*16777619u;
    for(unsigned j=0;j<4;j++)legacy[legacy_len-4+j]=(unsigned char)(checksum>>(8*j));
    f=fopen(stored,"wb");assert(f);assert(fwrite(legacy,1,legacy_len,f)==legacy_len);fclose(f);
    assert(rp_profile_load(sub,game,&out)==0 && out.renderer==0 && out.mouse_speed==730);
    char backup[1300]; snprintf(backup,sizeof(backup),"%s.bak",stored);
    assert(rename(stored,backup)==0);
    assert(rp_profile_load(sub,game,&out)==0 && out.mouse_speed==730);
    assert(rename(backup,stored)==0);
    f=fopen(stored,"r+b"); assert(f); assert(fseek(f,8,SEEK_SET)==0); assert(fputc(0xff,f)!=EOF); fclose(f);
    assert(rp_profile_load(sub,game,&out)!=0); assert(unlink(stored)==0); assert(rmdir(sub)==0);
    assert(unlink(game)==0 && unlink(other)==0 && rmdir(base)==0);
}
static void test_mario_controls(void) {
 RpProfile p;rp_profile_for_game(&p,"/games/Super Mario 63.swf");
 assert(p.binding[RP_CROSS][0]==38);
 rp_profile_for_game(&p,"/games/SUPER_MARIO_63.SWF");assert(p.binding[RP_CROSS][0]==38);
 rp_profile_for_game(&p,"/games/Bartender.swf");assert(p.binding[RP_CROSS][0]==RP_MOUSE_LEFT);
 rp_profile_for_game(&p,NULL);assert(p.binding[RP_CROSS][0]==RP_MOUSE_LEFT);
 rp_profile_mario63(&p);assert(rp_profile_valid(&p));RpInput s;rp_input_init(&s,320,240);Events e={0};
 RpPad pad={.buttons=1u<<RP_CROSS};rp_input_update(&s,&p,pad,.016,320,240,emit,&e);
 assert(count(&e,RP_EVENT_KEY,38,1)==1);assert(count(&e,RP_EVENT_MOUSE_BUTTON,0,1)==0);
 pad.buttons=0;pad.lx=32767;rp_input_update(&s,&p,pad,.016,320,240,emit,&e);
 assert(count(&e,RP_EVENT_KEY,39,1)==1);assert(s.cursor_x==160);
 pad.lx=0;pad.rx=32767;rp_input_update(&s,&p,pad,.016,320,240,emit,&e);assert(s.cursor_x>160);
}
int main(void) { test_mario_controls();test_input(); test_storage_and_browser(); puts("platform tests passed"); return 0; }
