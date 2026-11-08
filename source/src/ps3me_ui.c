/* SPDX-License-Identifier: GPL-2.0-only */
/* PS3ME browser's gradients, decorative waves, listing positions, and icon
 * fallback adapted to RSXKit.  The waves are precomputed only once and are
 * only drawn in the browser; no work is added to Flash gameplay frames.
 * The bundled DejaVuSans font is rendered directly by RSXKit.
 * No SDL rendering is added to Flash gameplay. */
#include "ps3me_ui.h"
#include <stddef.h>
#include <rsxkit/ui.h>
#include <math.h>

/* PS3ME: title 32, body 23, caption 17 pixels at 1280x720. RSXKit's UI
 * glyph logical em is 13 high, so these are proportional equivalents. */
static const float scales[]={32.0f/13.0f,23.0f/13.0f,17.0f/13.0f};
void rp_ps3me_label(float x,float y,const char *str,int face,float max_width){
    if(!str || face<RP_UI_TITLE || face>RP_UI_SMALL) return;
    rsxkit_ui_text_fit(x,y,str,scales[face],0xebf3ffff,max_width);
}
void rp_ps3me_rule(float y){
    rsxkit_ui_rect(80,y,1119,1,0xd2ebff28);
}
void rp_ps3me_highlight(float x,float y,float width,float height){
    rsxkit_ui_rect(x,y,width,height,0xdcebff18);
}

/* The PS3ME source generates three translucent, stationary sinusoidal bands.
 * Rendering 4-pixel-wide steps keeps the batched UI safely under its 4096
 * quad budget even when long SWF filenames are displayed. */
void rp_ps3me_background(int browser_waves){
    rsxkit_ui_gradient(0,0,1280,720,0x17405bff,0x17405bff,0x281341ff,0x281341ff);
    if(!browser_waves) return;
    static short wave_y[3][320];
    static int ready;
    if(!ready){
        for(int layer=0;layer<3;layer++)
            for(int i=0;i<320;i++){
                float x=(float)(i*4);
                wave_y[layer][i]=(short)(360+layer*40+
                    (int)(35*sinf(x/510.f+layer*.8f)+12*sinf(x/200.f+layer)));
            }
        ready=1;
    }
    for(int layer=0;layer<3;layer++){
        for(int i=0;i<320;i++){
            float y=(float)wave_y[layer][i];
            rsxkit_ui_rect(i*4.f,y,4.f,150.f,0xd2e1ff09);
            rsxkit_ui_rect(i*4.f,y,4.f,1.f,0xdcf0ff32);
        }
    }
}
void rp_ps3me_file_icon(float x,float y,float size,int directory){
    /* PS3ME's fallback item icon, not a fabricated thumbnail. */
    const float left=x+size/4.f, right=x+size*3.f/4.f;
    rsxkit_ui_rect(left,y,right-left,size,0x78afdcC8);
    rsxkit_ui_rect(left+4,y+5,right-left-8,size*0.6f-5.f,0x1e3c5aff);
    if(directory) rsxkit_ui_rect(left+2,y+3,(right-left)*0.48f,6.f,0xbed7efff);
}
int rp_ps3me_row_y(int index,int selected){
    int diff=index-selected;
    if(!diff) return 302;
    int distance=diff<0?-diff:diff;
    return 302+(diff>0?1:-1)*(94+(distance-1)*54);
}
