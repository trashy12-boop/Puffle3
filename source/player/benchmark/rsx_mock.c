/* Host command-stream recorder, NOT an RSX emulator or GPU benchmark. */
#include <rsxkit/rsxkit.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
static uint8_t *mapped;
static uint32_t vertex_offset;
static uint64_t stream_hash=14695981039346656037ULL;
static void hash(const void *p,size_t n){const uint8_t *b=p;while(n--)stream_hash=(stream_hash^*b++)*1099511628211ULL;}
uint64_t rp_probe_stream_hash(void){return stream_hash;}
uint64_t sysGetSystemTime(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
rsxkit_target *rsxkit_target_create(uint32_t w,uint32_t h){assert(w==640&&h==480);return malloc(1);}
rsxkit_depth *rsxkit_depth_create(uint32_t w,uint32_t h){assert(w==640&&h==480);return malloc(1);}
void rsxkit_target_destroy(rsxkit_target *t){free(t);}
void rsxkit_depth_destroy(rsxkit_depth *d){free(d);}
int rsxkit_map_main(void *p,uint32_t n,uint32_t *o){assert(n==20*1024*1024);mapped=p;*o=0;return 0;}
int rsxkit_unmap_main(void *p){assert(p==mapped);mapped=NULL;return 0;}
void rsxkit_finish(void){}
void rsxkit_fence_wait(uint32_t f){(void)f;}
int rsxkit_bind(rsxkit_target *t,rsxkit_depth *d,float w,float h){assert(t&&d&&w==320&&h==240);return 0;}
void rsxkit_clear(uint32_t f,uint32_t c,float z,const float r[4]){(void)z;(void)r;assert(f==3);stream_hash=14695981039346656037ULL;hash(&c,sizeof c);}
void rsxkit_texture_sync(void){}
void rsxkit_vertices(uint32_t off,uint8_t loc){assert(loc==RSXKIT_MEM_MAIN);vertex_offset=off;}
void rsxkit_draw_triangles(const rsxkit_draw *d,uint32_t first,uint32_t n){
 assert(first==0&&n>=3&&n%3==0);
 /* Hash state and vertex bytes per triangle, making batch boundaries irrelevant.
    Source texture offsets and transforms must remain equal between both runs. */
 for(uint32_t i=0;i<n;i+=3){hash(d,sizeof *d);hash(mapped+vertex_offset+i*sizeof(rsxkit_vertex),3*sizeof(rsxkit_vertex));}
}
uint32_t rsxkit_fence(void){static uint32_t f;return ++f;}
void rsxkit_target_present(rsxkit_target *t,const rsxkit_rect *a,uint32_t w,uint32_t h,const rsxkit_view *v,rsxkit_rect *b){(void)t;(void)a;(void)w;(void)h;(void)v;(void)b;}
