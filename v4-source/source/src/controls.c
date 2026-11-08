#include "controls.h"
#include <math.h>
#include <string.h>

static void send(RpEmit emit, void *ctx, RpEventType t, unsigned code, bool down, double x, double y) {
    if (emit) { RpEvent e = {t, code, down, x, y}; emit(ctx, &e); }
}
void rp_profile_default(RpProfile *p) {
    memset(p, 0, sizeof(*p));
    p->binding[RP_CROSS][0]=RP_MOUSE_LEFT; p->binding[RP_CIRCLE][0]=RP_MOUSE_RIGHT;
    p->binding[RP_R1][0]=RP_MOUSE_LEFT; p->binding[RP_L1][0]=RP_MOUSE_RIGHT;
    p->binding[RP_SQUARE][0]='Z'; p->binding[RP_TRIANGLE][0]='C';
    p->binding[RP_UP][0]=38; p->binding[RP_DOWN][0]=40;
    p->binding[RP_LEFT][0]=37; p->binding[RP_RIGHT][0]=39;
    p->binding[RP_START][0]=13;
    p->mouse_speed=400; p->deadzone=18; p->acceleration=50; p->mouse_stick=1;
}
/* Verified against KeyUP/KeySPIN/KeyZ in the imported Mario SWF. */
void rp_profile_mario63(RpProfile *p) {
    rp_profile_default(p);
    p->binding[RP_CROSS][0]=38;p->binding[RP_SQUARE][0]='X';
    p->binding[RP_CIRCLE][0]='Z';p->binding[RP_TRIANGLE][0]='C';
    p->binding[RP_START][0]='P';p->binding[RP_SELECT][0]=16;
    p->binding[RP_LSTICK_UP][0]=38;p->binding[RP_LSTICK_DOWN][0]=40;
    p->binding[RP_LSTICK_LEFT][0]=37;p->binding[RP_LSTICK_RIGHT][0]=39;
    p->mouse_stick=2;
}
/* Only recognized basenames get the game preset; saved profiles override it. */
void rp_profile_for_game(RpProfile *p, const char *path) {
    const char *name=path ? strrchr(path,'/') : NULL;
    name=name ? name+1 : (path ? path : "");
    char normalized[64]; size_t n=0;
    for(;*name && n+1<sizeof normalized;name++) {
        unsigned char c=(unsigned char)*name;
        if(c==' '||c=='_'||c=='-') continue;
        normalized[n++]=(char)(c>='A'&&c<='Z' ? c+'a'-'A' : c);
    }
    normalized[n]=0;
    if(!*name && strcmp(normalized,"supermario63.swf")==0) rp_profile_mario63(p);
    else rp_profile_default(p);
}
bool rp_profile_valid(const RpProfile *p) {
    if (!p || p->mouse_speed<10 || p->mouse_speed>2000 || p->deadzone>90 || p->acceleration>200 || p->mouse_stick>2 || p->renderer>1) return false;
    for (int i=0; i<RP_BUTTONS; i++) for (int j=0; j<RP_BINDINGS; j++)
        if (p->binding[i][j]>RP_MOUSE_RIGHT) return false;
    return true;
}
void rp_input_init(RpInput *s, double w, double h) {
    memset(s,0,sizeof(*s)); s->cursor_x=w>0 ? w/2 : 0; s->cursor_y=h>0 ? h/2 : 0;
}
void rp_input_release_all(RpInput *s, RpEmit emit, void *ctx) {
    for (unsigned k=1;k<256;k++) if (s->keys[k]) { s->keys[k]=false; send(emit,ctx,RP_EVENT_KEY,k,false,s->cursor_x,s->cursor_y); }
    for (unsigned k=0;k<2;k++) if (s->mouse[k]) { s->mouse[k]=false; send(emit,ctx,RP_EVENT_MOUSE_BUTTON,k,false,s->cursor_x,s->cursor_y); }
}
static double axis(int16_t a) { return a < 0 ? (double)a/32768.0 : (double)a/32767.0; }
void rp_input_update(RpInput *s, const RpProfile *p, RpPad pad, double dt, double w, double h, RpEmit emit, void *ctx) {
    if (!s || !rp_profile_valid(p) || !isfinite(w) || !isfinite(h) || w<1 || h<1) return;
    if (!isfinite(dt) || dt<0) dt=0;
    if (dt>0.1) dt=0.1;
    uint32_t buttons=pad.buttons & ((1u<<16)-1u);
    bool chord=(buttons & RP_MENU_MASK)==RP_MENU_MASK;
    if (chord && !s->chord_latched) {
        s->chord_latched=true; s->menu_open=!s->menu_open; s->release_gate=true;
        rp_input_release_all(s,emit,ctx); s->l3_seconds=s->r3_seconds=0;
        send(emit,ctx,RP_EVENT_MENU,0,s->menu_open,s->cursor_x,s->cursor_y);
    }
    if ((buttons & RP_MENU_MASK)==0) s->chord_latched=false;
    if (s->menu_open) return;
    if (s->release_gate) {
        rp_input_release_all(s,emit,ctx);
        if (!buttons && fabs(axis(pad.lx))<0.2 && fabs(axis(pad.ly))<0.2 && fabs(axis(pad.rx))<0.2 && fabs(axis(pad.ry))<0.2) s->release_gate=false;
        return;
    }
    /* Delay individual stick-click bindings briefly to make a near-simultaneous
       menu chord less likely to generate a game action. */
    double old_l3=s->l3_seconds, old_r3=s->r3_seconds;
    bool tap_l3=!(buttons&(1u<<RP_L3)) && old_l3>0 && old_l3<0.20;
    bool tap_r3=!(buttons&(1u<<RP_R3)) && old_r3>0 && old_r3<0.20;
    s->l3_seconds=(buttons&(1u<<RP_L3)) ? old_l3+dt : 0;
    s->r3_seconds=(buttons&(1u<<RP_R3)) ? old_r3+dt : 0;
    if (s->l3_seconds<0.20) buttons &= ~(1u<<RP_L3);
    if (s->r3_seconds<0.20) buttons &= ~(1u<<RP_R3);
    double axes[4]={axis(pad.lx),axis(pad.ly),axis(pad.rx),axis(pad.ry)};
    double threshold=(double)p->deadzone/100.0; if (threshold<0.25) threshold=0.25;
    for (int stick=0;stick<2;stick++) {
        double x=axes[2*stick], y=axes[2*stick+1]; unsigned base=RP_LSTICK_UP+4u*(unsigned)stick;
        if (y < -threshold) buttons|=1u<<base;
        if (y > threshold) buttons|=1u<<(base+1);
        if (x < -threshold) buttons|=1u<<(base+2);
        if (x > threshold) buttons|=1u<<(base+3);
    }
    bool wanted[258]={false};
    for (int i=0;i<RP_BUTTONS;i++) if (buttons&(1u<<i)) for (int j=0;j<RP_BINDINGS;j++) wanted[p->binding[i][j]]=true;
    for (unsigned k=1;k<256;k++) if (wanted[k]!=s->keys[k]) { s->keys[k]=wanted[k]; send(emit,ctx,RP_EVENT_KEY,k,wanted[k],s->cursor_x,s->cursor_y); }
    if (p->mouse_stick) {
        unsigned off=p->mouse_stick==1 ? 0 : 2; double x=axes[off], y=axes[off+1];
        double mag=sqrt(x*x+y*y), dz=(double)p->deadzone/100.0;
        if (mag>dz) {
            if (mag>1) mag=1;
            double strength=(mag-dz)/(1-dz);
            double speed=p->mouse_speed*strength*(1.0+(p->acceleration/100.0)*strength);
            double norm=sqrt(x*x+y*y); double ox=s->cursor_x, oy=s->cursor_y;
            s->cursor_x+=x/norm*speed*dt; s->cursor_y+=y/norm*speed*dt;
            if(s->cursor_x<0)s->cursor_x=0;
            if(s->cursor_y<0)s->cursor_y=0;
            if(s->cursor_x>w-1)s->cursor_x=w-1;
            if(s->cursor_y>h-1)s->cursor_y=h-1;
            if(s->cursor_x!=ox||s->cursor_y!=oy) send(emit,ctx,RP_EVENT_MOUSE_MOVE,0,false,s->cursor_x,s->cursor_y);
        }
    }
    for (unsigned k=0;k<2;k++) if (wanted[RP_MOUSE_LEFT+k]!=s->mouse[k]) {
        s->mouse[k]=wanted[RP_MOUSE_LEFT+k]; send(emit,ctx,RP_EVENT_MOUSE_BUTTON,k,s->mouse[k],s->cursor_x,s->cursor_y);
    }
    /* Short solo stick-clicks are delivered on release; never synthesize an
       up event for an action that is already held by a different button. */
    bool pulse[258]={false};
    for(int i=0;i<RP_BINDINGS;i++) {
        if(tap_l3) pulse[p->binding[RP_L3][i]]=true;
        if(tap_r3) pulse[p->binding[RP_R3][i]]=true;
    }
    for(unsigned k=1;k<258;k++) if(pulse[k]&&!wanted[k]) {
        RpEventType type=k<256?RP_EVENT_KEY:RP_EVENT_MOUSE_BUTTON;
        unsigned code=k<256?k:k-RP_MOUSE_LEFT;
        send(emit,ctx,type,code,true,s->cursor_x,s->cursor_y);
        send(emit,ctx,type,code,false,s->cursor_x,s->cursor_y);
    }
}
