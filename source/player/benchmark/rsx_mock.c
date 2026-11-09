/* Host command-stream recorder, NOT an RSX emulator or GPU benchmark. */
#include <rsxkit/rsxkit.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
static uint8_t *mapped;
static uint8_t *gradient_atlas;
static uint32_t vertex_offset;
static uint64_t stream_hash=14695981039346656037ULL;
static struct {uint32_t offset,pitch;uint16_t w,h;uint64_t hash;} pixel_hashes[512];
static unsigned pixel_hash_count;
static void hash(const void *p,size_t n){const uint8_t *b=p;while(n--)stream_hash=(stream_hash^*b++)*1099511628211ULL;}
static uint64_t pixels_hash(uint32_t offset,const uint8_t *p,uint32_t pitch,uint16_t w,uint16_t h){
 for(unsigned i=0;i<pixel_hash_count;i++)if(pixel_hashes[i].offset==offset&&pixel_hashes[i].pitch==pitch&&pixel_hashes[i].w==w&&pixel_hashes[i].h==h)return pixel_hashes[i].hash;
 uint64_t value=14695981039346656037ULL;
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<(unsigned)w*4;x++)value=(value^p[y*pitch+x])*1099511628211ULL;
 if(pixel_hash_count<512){unsigned i=pixel_hash_count++;pixel_hashes[i].offset=offset;pixel_hashes[i].pitch=pitch;pixel_hashes[i].w=w;pixel_hashes[i].h=h;pixel_hashes[i].hash=value;}
 return value;
}
uint64_t rp_probe_stream_hash(void){return stream_hash;}
uint64_t sysGetSystemTime(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
rsxkit_target *rsxkit_target_create(uint32_t w,uint32_t h){assert(w==640&&h==480);return malloc(1);}
rsxkit_depth *rsxkit_depth_create(uint32_t w,uint32_t h){assert(w==640&&h==480);return malloc(1);}
void rsxkit_target_destroy(rsxkit_target *t){free(t);}
void rsxkit_depth_destroy(rsxkit_depth *d){free(d);}
int rsxkit_map_main(void *p,uint32_t n,uint32_t *o){
 if(n==20*1024*1024){mapped=p;*o=0;return 0;}
 assert(n==1024*1024);gradient_atlas=p;*o=32*1024*1024;return 0;
}
int rsxkit_unmap_main(void *p){if(p==mapped)mapped=NULL;else{assert(p==gradient_atlas);gradient_atlas=NULL;}return 0;}
void rsxkit_finish(void){}
void rsxkit_fence_wait(uint32_t f){(void)f;}
int rsxkit_bind(rsxkit_target *t,rsxkit_depth *d,float w,float h){assert(t&&d&&w==320&&h==240);return 0;}
void rsxkit_clear(uint32_t f,uint32_t c,float z,const float r[4]){(void)z;(void)r;assert(f==3);stream_hash=14695981039346656037ULL;pixel_hash_count=0;hash(&c,sizeof c);}
void rsxkit_texture_sync(void){}
void rsxkit_vertices(uint32_t off,uint8_t loc){assert(loc==RSXKIT_MEM_MAIN);vertex_offset=off;}
void rsxkit_draw_triangles(const rsxkit_draw *d,uint32_t first,uint32_t n){
 assert(first==0&&n>=3&&n%3==0);
 /* Hash state and vertices per triangle, independent of batch boundaries.
    Canonicalize only constant-row linear LUT sampling, including ALL 1024 LUT
    bytes. Thus a relocated row must retain its exact colors, transforms,
    horizontal UVs, blending, scissor and painter order. Other draws keep the
    original state/vertex hash. This does not execute a GPU fragment shader. */
 for(uint32_t i=0;i<n;i+=3){
  const rsxkit_vertex *v=(const rsxkit_vertex*)(mapped+vertex_offset)+i;
  if(d->ntex==1&&d->gradient==0&&d->tex[0].width==256&&v[0].st[1]==v[1].st[1]&&v[0].st[1]==v[2].st[1]
     &&((d->tex[0].height==1&&v[0].st[1]==0.5f)||(d->tex[0].height==1024&&d->tex[0].offset==32*1024*1024))){
   rsxkit_draw canonical=*d;rsxkit_vertex vertices[3];memcpy(vertices,v,sizeof vertices);
   const uint8_t *pixels;
   if(d->tex[0].height==1024){
    float row=v[0].st[1]*1024.0f-0.5f;unsigned index=(unsigned)row;
    assert(gradient_atlas&&d->tex[0].offset==32*1024*1024&&row==(float)index&&index<1024);
    pixels=gradient_atlas+index*1024;
   }else pixels=mapped+d->tex[0].offset;
   for(unsigned j=0;j<3;j++)vertices[j].st[1]=0.5f;
   canonical.tex[0].offset=0;canonical.tex[0].height=1;canonical.tex[0].pitch=1024;
   canonical.tex[0].wrap_t=canonical.tex[0].wrap_s;
   uint32_t address=d->tex[0].height==1024?d->tex[0].offset+(uint32_t)(v[0].st[1]*1024.0f-0.5f)*1024:d->tex[0].offset;
   uint64_t colors=pixels_hash(address,pixels,1024,256,1);
   hash(&canonical,sizeof canonical);hash(vertices,sizeof vertices);hash(&colors,sizeof colors);
  }else{
   rsxkit_draw canonical=*d;uint64_t colors=0;
   if(d->ntex==1){
    assert(d->tex[0].offset<20*1024*1024);
    colors=pixels_hash(d->tex[0].offset,mapped+d->tex[0].offset,d->tex[0].pitch,d->tex[0].width,d->tex[0].height);
    canonical.tex[0].offset=0;
   }
   hash(&canonical,sizeof canonical);hash(v,3*sizeof(rsxkit_vertex));
   if(d->ntex==1)hash(&colors,sizeof colors);
  }
 }
}
uint32_t rsxkit_fence(void){static uint32_t f;return ++f;}
void rsxkit_target_present(rsxkit_target *t,const rsxkit_rect *a,uint32_t w,uint32_t h,const rsxkit_view *v,rsxkit_rect *b){(void)t;(void)a;(void)w;(void)h;(void)v;(void)b;}
