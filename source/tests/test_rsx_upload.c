/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <rsxkit/rsxkit.h>
#include "rsx_profile.h"
static uint8_t *mapped;
static int map_failure;
static unsigned map_count;
static uint32_t last_texture_offset;
static uint8_t last_pixel[4];
static unsigned draw_count,wait_count,texture_sync_count;
uint64_t sysGetSystemTime(void){static uint64_t tick=1000;return tick+=10;}
rsxkit_target *rsxkit_target_create(unsigned w,unsigned h){assert(w==640&&h==480);return calloc(1,sizeof(rsxkit_target));}
rsxkit_depth *rsxkit_depth_create(unsigned w,unsigned h){assert(w==640&&h==480);return calloc(1,sizeof(rsxkit_depth));}
void rsxkit_target_destroy(rsxkit_target *t){free(t);}
void rsxkit_depth_destroy(rsxkit_depth *d){free(d);}
int rsxkit_map_main(void *ptr,uint32_t size,uint32_t *offset){assert(size==20u*1024*1024);map_count++;if(map_failure)return -1;mapped=ptr;*offset=1024;return 0;}
void rsxkit_unmap_main(void *ptr){assert(ptr==mapped);mapped=NULL;}
void rsxkit_finish(void){}
void rsxkit_fence_wait(uint32_t f){assert(f!=0);wait_count++;}
int rsxkit_bind(rsxkit_target *t,rsxkit_depth *d,unsigned w,unsigned h){assert(t&&d&&w==320&&h==240);return 0;}
void rsxkit_clear(int what,uint32_t color,float depth,void *rect){(void)color;(void)depth;(void)rect;assert(what==3);}
void rsxkit_texture_sync(void){texture_sync_count++;}
void rsxkit_vertices(uint32_t offset,uint8_t location){(void)offset;assert(location==0);}
void rsxkit_draw_triangles(const rsxkit_draw *d,uint32_t first,uint32_t n){
    assert(first==0&&n>=3&&n%3==0);draw_count++;
    if (d->ntex) {last_texture_offset=d->tex[0].offset;memcpy(last_pixel,mapped+last_texture_offset-1024,4);}
}
uint32_t rsxkit_fence(void){static uint32_t f=0;return ++f;}
void rsxkit_target_present(rsxkit_target *t,void *a,unsigned w,unsigned h,const rsxkit_view *v,void *b){(void)a;(void)b;assert(t&&w==640&&h==480&&v);}
extern int rp_rsx_init(void),rp_rsx_begin(uint32_t),rp_rsx_ready(void),rp_rsx_current(void);
extern int rp_rsx_mesh(const rsxkit_vertex *,uint32_t,const uint8_t *,uint32_t,uint32_t,int,uint8_t,uint8_t,uint8_t,float,const float *,const float *,uint64_t,uint64_t);
extern void rp_rsx_flush(void);
extern void rp_rsx_end(int),rp_rsx_present(void),rp_rsx_shutdown(void),rp_rsx_cache_reset(void);
int main(void){
    const float mult[4]={1,1,1,1},add[4]={0,0,0,0};
    const rsxkit_vertex triangle[3]={0};
    const uint8_t tex[8]={64,32,16,128,255,127,1,255};
    assert(rp_rsx_init()&&rp_rsx_ready());assert(!mapped&&map_count==0);
    rp_rsx_shutdown();assert(!mapped);
    assert(rp_rsx_init());map_failure=1;assert(!rp_rsx_begin(0));assert(!mapped);
    unsigned attempts=map_count;assert(!rp_rsx_begin(0));assert(map_count==attempts);
    map_failure=0;rp_rsx_cache_reset();

    assert(rp_rsx_begin(0x123456));
    assert(rp_rsx_mesh(triangle,3,tex,2,1,1,0,0,0,0,mult,add,42,0));
    rp_rsx_flush();
    assert(last_pixel[0]==127&&last_pixel[1]==63&&last_pixel[2]==31&&last_pixel[3]==128);
    assert(rp_rsx_mesh(triangle,3,tex,2,1,1,0,0,0,0,mult,add,42,0));
    RpRsxProfile p={0};rp_rsx_profile_snapshot(&p);
    assert(p.draw_calls==2&&p.vertices==6&&p.uploaded_textures==1&&p.reused_textures==1&&p.uploaded_bytes==8);
    rp_rsx_flush();
    assert(draw_count==2&&last_texture_offset==1024+4u*1024*1024);
    rp_rsx_end(1);assert(rp_rsx_current());rp_rsx_present();
    assert(rp_rsx_begin(0));assert(wait_count==1&&texture_sync_count==2);
    assert(rp_rsx_mesh(triangle,3,tex,2,1,1,0,0,0,0,mult,add,42,0));
    rp_rsx_profile_snapshot(&p);assert(p.uploaded_textures==1 && p.reused_textures==2 && p.persistent_hits==2);
    rp_rsx_end(1);
    assert(rp_rsx_begin(0));
    /* Mutable bitmap invalidation must update the same GPU allocation. */
    assert(rp_rsx_mesh(triangle,3,tex,2,1,1,0,0,0,0,mult,add,42,1));
    rp_rsx_profile_snapshot(&p);
    assert(p.uploaded_textures==2 && p.reused_textures==2 && p.persistent_uploads==2);
    rp_rsx_end(1);
    assert(rp_rsx_begin(0));
    /* At the same generation, no new upload is required. */
    assert(rp_rsx_mesh(triangle,3,tex,2,1,1,0,0,0,0,mult,add,42,1));
    rp_rsx_profile_snapshot(&p);
    assert(p.uploaded_textures==2 && p.reused_textures==3);
    rp_rsx_end(1);
    rp_rsx_cache_reset();
    assert(rp_rsx_begin(0));
    assert(rp_rsx_mesh(triangle,3,tex,2,1,1,0,0,0,0,mult,add,42,1));
    rp_rsx_profile_snapshot(&p);
    assert(p.uploaded_textures==3);
    rp_rsx_end(1);
    /* Saturate metadata, then leave the scene: stale IDs must not occupy
       the persistent cache for the rest of a long game session. */
    assert(rp_rsx_begin(0));
    for(uint64_t id=1000;id<1400;id++)
        assert(rp_rsx_mesh(triangle,3,tex,2,1,1,0,0,0,0,mult,add,id,0));
    rp_rsx_end(1);
    assert(rp_rsx_begin(0));
    assert(rp_rsx_mesh(triangle,3,tex,2,1,1,0,0,0,0,mult,add,42,1));
    assert(rp_rsx_mesh(triangle,3,tex,2,1,1,0,0,0,0,mult,add,1500,0));
    rp_rsx_end(1);
    assert(rp_rsx_begin(0));
    rp_rsx_profile_snapshot(&p);assert(p.evicted_textures>=383);
    assert(rp_rsx_mesh(triangle,3,tex,2,1,1,0,0,0,0,mult,add,1500,0));
    rp_rsx_end(1);
    assert(rp_rsx_begin(0));
    uint64_t uploads_before=p.uploaded_textures;
    rp_rsx_profile_snapshot(&p);uploads_before=p.uploaded_textures;
    assert(rp_rsx_mesh(triangle,3,tex,2,1,1,0,0,0,0,mult,add,1500,0));
    rp_rsx_profile_snapshot(&p);assert(p.uploaded_textures==uploads_before);
    rp_rsx_end(1);
    /* Immutable gradient LUT: changing draw uniforms must not upload again. */
    uint8_t lut[256*4]; memset(lut,64,sizeof lut);
    rp_rsx_profile_snapshot(&p);uploads_before=p.uploaded_textures;
    assert(rp_rsx_begin(0));
    assert(rp_rsx_mesh(triangle,3,lut,256,1,0,0,0,1,0,mult,add,2000,0));
    rp_rsx_flush();
    uint32_t lut_offset=last_texture_offset;
    assert(last_pixel[0]==64 && last_pixel[3]==64);
    rp_rsx_end(1);
    assert(rp_rsx_begin(0));
    const float tinted[4]={0.5f,1,1,0.5f};
    assert(rp_rsx_mesh(triangle,3,lut,256,1,0,0,0,1,0,tinted,add,2000,0));
    rp_rsx_flush();
    assert(last_texture_offset==lut_offset);
    rp_rsx_profile_snapshot(&p);assert(p.uploaded_textures==uploads_before+1);
    rp_rsx_end(1);
    /* An actual batch retains all six vertices and one state. Color changes
       and clipping changes split batches; they may never reorder blending. */
    assert(rp_rsx_begin(0));
    rp_rsx_profile_snapshot(&p);uint64_t batches=p.submitted_batches,merged=p.merged_draws;
    unsigned before_draws=draw_count;
    assert(rp_rsx_mesh(triangle,3,NULL,0,0,0,0,0,0,0,mult,add,0,0));
    assert(rp_rsx_mesh(triangle,3,NULL,0,0,0,0,0,0,0,mult,add,0,0));
    assert(draw_count==before_draws);
    rp_rsx_end(1);rp_rsx_profile_snapshot(&p);
    assert(draw_count==before_draws+1 && p.submitted_batches==batches+1 && p.merged_draws==merged+1);
    assert(rp_rsx_begin(0));
    before_draws=draw_count;
    assert(rp_rsx_mesh(triangle,3,tex,2,1,1,0,0,0,0,mult,add,42,1));
    assert(rp_rsx_mesh(triangle,3,tex,2,1,1,0,0,0,0,tinted,add,42,1));
    rp_rsx_end(1);assert(draw_count==before_draws+2);
    rp_rsx_shutdown();assert(!rp_rsx_ready());
    puts("RSX mock: upload, conversion, reuse, synchronization and shutdown passed");
    return 0;
}
