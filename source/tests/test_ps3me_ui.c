/* SPDX-License-Identifier: GPL-2.0-only */
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <rsxkit/ui.h>
#include "ps3me_ui.h"
static unsigned quads, labels, waves;
void rsxkit_ui_gradient(float x,float y,float w,float h,uint32_t tl,uint32_t tr,uint32_t bl,uint32_t br) {
 (void)x;(void)y;(void)w;(void)h;(void)tr;(void)bl;(void)br;
 if(tl==0xd2e1ff09 || tl==0xdcf0ff32) waves++;
 quads++;
}
float rsxkit_ui_text_fit(float x,float y,const char *str,float scale,uint32_t rgba,float max_w) {
 (void)x;(void)y;(void)str;(void)scale;(void)max_w;
 assert(rgba==0xebf3ffff);
 labels++;
 return 0.f;
}
int main(void) {
 assert(rp_ps3me_row_y(4,4)==302);
 assert(rp_ps3me_row_y(3,4)==208);
 assert(rp_ps3me_row_y(2,4)==154);
 assert(rp_ps3me_row_y(5,4)==396);
 assert(rp_ps3me_row_y(6,4)==450);
 assert(rp_ps3me_row_y(7,4)==504);
 rp_ps3me_background(0);
 assert(quads==1 && waves==0);
 rp_ps3me_background(1);
 assert(quads==1922 && waves==1920);
 rp_ps3me_label(80,44,"RUFFLE",RP_UI_TITLE,1120);
 rp_ps3me_highlight(70,171,1130,40);
 rp_ps3me_rule(142);
 rp_ps3me_file_icon(92,302,70,1);
 assert(labels==1 && quads==1927);
 puts("PS3ME layout: rows, gradients, wave layers, text and icons passed");
 return 0;
}
