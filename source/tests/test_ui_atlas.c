#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "rsxkit/ui.h"
static int glyph(void *user,uint32_t cp,uint8_t *alpha,uint32_t pitch,rsxkit_glyph_info *g){
 (void)user;(void)cp;g->advance=6;g->left=0;g->width=3;
 alpha[0]=255;alpha[1]=128;alpha[2]=1;alpha[pitch]=64;return 0;
}
int main(void){
 const size_t count=(size_t)RSXKIT_UI_ATLAS_W*RSXKIT_UI_ATLAS_H;
 uint8_t *pixels=malloc(count*4);assert(pixels);
 rsxkit_ui_atlas(pixels,RSXKIT_UI_ATLAS_W*4);
 rsxkit_glyph_source source={"synthetic",NULL,glyph};
 rsxkit_ui_font(&source);rsxkit_vertex vertices[128];
 rsxkit_ui_batch(vertices,128,640,480);rsxkit_ui_text(0,0,"A",1,0xebf3ffff,0);
 rsxkit_ui_font_stats stats;rsxkit_ui_font_stats_get(&stats);assert(stats.renders==1);
 unsigned coverage[256]={0};
 for(size_t i=0;i<count;i++){
  assert(pixels[4*i]==255&&pixels[4*i+1]==255&&pixels[4*i+2]==255);
  coverage[pixels[4*i+3]]++;
 }
 assert(coverage[128]==1&&coverage[1]==1&&coverage[64]==1);
 assert(coverage[0]>0&&coverage[255]>0);
 rsxkit_ui_font(NULL);free(pixels);puts("UI atlas: white RGB and exact external-font coverage passed");
}
