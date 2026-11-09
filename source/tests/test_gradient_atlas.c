/* SPDX-License-Identifier: MIT
 * Host command/texture mock, not GPU execution or a frame-rate benchmark. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rsxkit/rsxkit.h>
#include "rsx_profile.h"
#define MB (1024u*1024u)
static uint8_t *arena,*atlas;
static uint32_t vertex_offset;
static unsigned draws,vertices,waits,syncs,atlas_attempts;
static int fail_atlas;
static uint8_t sources[1100][1024];
static int texel(int x,unsigned wrap){
 if(wrap==0)return x<0?0:x>255?255:x;
 int period=wrap==1?256:512;x%=period;if(x<0)x+=period;
 return x<256?x:511-x;
}
static float sample(const uint8_t *row,float s,unsigned wrap,unsigned channel){
 float x=s*256.0f-0.5f;int left=(int)floorf(x);float f=x-left;
 return row[texel(left,wrap)*4+channel]*(1-f)+row[texel(left+1,wrap)*4+channel]*f;
}
uint64_t sysGetSystemTime(void){static uint64_t t;return ++t;}
rsxkit_target *rsxkit_target_create(unsigned w,unsigned h){(void)w;(void)h;return calloc(1,sizeof(rsxkit_target));}
rsxkit_depth *rsxkit_depth_create(unsigned w,unsigned h){(void)w;(void)h;return calloc(1,sizeof(rsxkit_depth));}
void rsxkit_target_destroy(rsxkit_target *p){free(p);}
void rsxkit_depth_destroy(rsxkit_depth *p){free(p);}
int rsxkit_map_main(void *p,uint32_t n,uint32_t *offset){
 if(n==20*MB){assert(!arena);arena=p;*offset=1024;return 0;}
 assert(n==MB);atlas_attempts++;if(fail_atlas)return -1;
 assert(!atlas);atlas=p;*offset=32*MB;return 0;
}
void rsxkit_unmap_main(void *p){if(p==arena)arena=NULL;else{assert(p==atlas);atlas=NULL;}}
void rsxkit_finish(void){}
void rsxkit_fence_wait(uint32_t f){assert(f);waits++;}
int rsxkit_bind(rsxkit_target *p,rsxkit_depth *d,unsigned w,unsigned h){assert(p&&d&&w==320&&h==240);return 0;}
void rsxkit_clear(int what,uint32_t c,float d,void *r){(void)what;(void)c;(void)d;(void)r;}
void rsxkit_texture_sync(void){syncs++;}
void rsxkit_vertices(uint32_t offset,uint8_t loc){assert(loc==RSXKIT_MEM_MAIN);vertex_offset=offset-1024;}
void rsxkit_draw_triangles(const rsxkit_draw *d,uint32_t first,uint32_t n){
 assert(first==0&&n>=3&&n%3==0);draws++;vertices+=n;
 const rsxkit_vertex *v=(const rsxkit_vertex*)(arena+vertex_offset);
 assert(d->ntex==1&&d->tex[0].width==256&&d->tex[0].pitch==1024);
 for(unsigned i=0;i<n;i++){
  unsigned source=v[i].pad;assert(source<1100);
  const uint8_t *pixels;
  if(d->tex[0].height==1024){
   assert(d->tex[0].offset==32*MB&&d->gradient==0&&atlas);
   float row=v[i].st[1]*1024.0f-0.5f;
   assert(row>=0&&row<1024&&row==floorf(row));
   assert(d->tex[0].wrap_t==0);
   pixels=atlas+(unsigned)row*1024;
  }else{
   assert(d->tex[0].height==1);pixels=arena+d->tex[0].offset-1024;
  }
  assert(!memcmp(pixels,sources[source],1024));
  /* Exact row centers preserve each source LUT's horizontal filtering and
     pad/repeat/mirror behavior, including translucent stops. */
  const float positions[]={-2.0f,-0.125f,0,0.5f,1.0f,1.75f};
  for(unsigned x=0;x<6;x++)for(unsigned c=0;c<4;c++)
   assert(sample(pixels,positions[x],d->tex[0].wrap_s,c)==sample(sources[source],positions[x],d->tex[0].wrap_s,c));
 }
}
uint32_t rsxkit_fence(void){static uint32_t f;return ++f;}
void rsxkit_target_present(rsxkit_target *p,void *a,unsigned w,unsigned h,const rsxkit_view *v,void *b){(void)p;(void)a;(void)w;(void)h;(void)v;(void)b;}
extern int rp_rsx_init(void),rp_rsx_begin(uint32_t),rp_rsx_current(void);
extern int rp_rsx_set_gradient_atlas(int);
extern int rp_rsx_mesh(const rsxkit_vertex*,uint32_t,const uint8_t*,uint32_t,uint32_t,int,uint8_t,uint8_t,uint8_t,float,const float*,const float*,uint64_t,uint64_t);
extern int rp_rsx_clip(int32_t,int32_t,int32_t,int32_t);
extern void rp_rsx_end(int),rp_rsx_shutdown(void),rp_rsx_cache_reset(void);
static void fill(unsigned source){for(unsigned x=0;x<256;x++)for(unsigned c=0;c<4;c++)sources[source][4*x+c]=(uint8_t)(source+x*7+c*31);}
static void draw(unsigned source,uint64_t id,unsigned wrap,unsigned gradient,uint64_t revision,float tint){
 rsxkit_vertex v[3]={0};for(unsigned i=0;i<3;i++){v[i].st[0]=(float)i/2;v[i].st[1]=0.5f;v[i].pad=source;}
 const float mult[4]={tint,1,1,1},add[4]={0};
 assert(rp_rsx_mesh(v,3,sources[source],256,1,0,wrap,0,gradient,0,mult,add,id,revision));
}
int main(void){
 for(unsigned i=0;i<1100;i++)fill(i);
 assert(rp_rsx_init()&&rp_rsx_set_gradient_atlas(1)&&rp_rsx_begin(0));
 unsigned before=draws;
 draw(0,100,0,0,0,1);draw(1,101,0,0,0,1);
 assert(draws==before);rp_rsx_end(1);
 assert(draws==before+1&&vertices==6&&rp_rsx_current());
 RpRsxProfile p;rp_rsx_profile_snapshot(&p);assert(p.uploaded_textures==2&&p.merged_draws==1);
 assert(rp_rsx_arena_bytes()==21*MB&&atlas_attempts==1&&syncs>=2);
 assert(rp_rsx_begin(0));unsigned sync_before=syncs;
 draw(0,100,0,0,0,1);draw(1,101,0,0,0,1);rp_rsx_end(1);
 rp_rsx_profile_snapshot(&p);assert(p.uploaded_textures==2&&p.persistent_hits==2&&syncs==sync_before);
 /* Uniform changes, wrapping and scissor changes split batches in order. */
 assert(rp_rsx_begin(0));before=draws;
 draw(0,100,0,0,0,1);draw(1,101,0,0,0,0.5f);
 draw(2,102,1,0,0,0.5f);draw(3,103,2,0,0,0.5f);
 assert(rp_rsx_clip(10,20,100,120));draw(3,103,2,0,0,0.5f);rp_rsx_end(1);
 assert(draws==before+5);
 /* Radial/focal fills and revised textures keep their separate allocations. */
 assert(rp_rsx_begin(0));draw(4,104,0,1,0,1);draw(5,105,0,2,0,1);draw(6,106,0,0,1,1);rp_rsx_end(1);
 rp_rsx_cache_reset();assert(rp_rsx_begin(0));
 /* Exhaust the page. The 1025th unique ID falls back, without overwriting
    any atlas row used by earlier queued triangles in the same frame. */
 before=draws;
 for(unsigned i=0;i<1025;i++)draw(i,10000+i,0,0,0,1);
 rp_rsx_end(1);assert(draws==before+2);
 assert(!memcmp(atlas,sources[0],1024));assert(!memcmp(atlas+1023*1024,sources[1023],1024));
 unsigned waited=waits;assert(rp_rsx_begin(0)&&waits==waited+1);
 draw(1026,77777,0,0,0,1);rp_rsx_end(1);assert(!memcmp(atlas,sources[1026],1024));
 rp_rsx_shutdown();assert(!arena&&!atlas);
 /* A failed extra mapping is retried only at an explicit reset; ordinary
    immutable-texture rendering remains available and correct. */
 assert(rp_rsx_init()&&rp_rsx_set_gradient_atlas(1)&&rp_rsx_begin(0));fail_atlas=1;unsigned attempts=atlas_attempts;
 draw(7,700,0,0,0,1);draw(8,701,0,0,0,1);rp_rsx_end(1);
 assert(!atlas&&atlas_attempts==attempts+1&&rp_rsx_current());
 assert(rp_rsx_begin(0));draw(7,700,0,0,0,1);rp_rsx_end(1);assert(atlas_attempts==attempts+1);
 fail_atlas=0;rp_rsx_cache_reset();assert(rp_rsx_begin(0));draw(7,700,0,0,0,1);rp_rsx_end(1);assert(atlas);
 rp_rsx_shutdown();assert(!arena&&!atlas);
 /* The identical executable can run the old renderer with the opt-in off. */
 assert(rp_rsx_init()&&rp_rsx_begin(0));before=draws;attempts=atlas_attempts;
 draw(7,700,0,0,0,1);draw(8,701,0,0,0,1);rp_rsx_end(1);
 assert(draws==before+2&&atlas_attempts==attempts&&!atlas&&rp_rsx_arena_bytes()==20*MB);
 assert(!rp_rsx_set_gradient_atlas(1));rp_rsx_shutdown();assert(!arena&&!atlas);
 puts("Gradient atlas mock: LUT samples, blending state, order, reuse, page exhaustion and allocation fallback passed");
}
