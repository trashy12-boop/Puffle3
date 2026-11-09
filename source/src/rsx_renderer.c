/* SPDX-License-Identifier: MIT */
#include <rsxkit/rsxkit.h>
#include <sys/systime.h>
#include "pixel_convert.h"
#include "rsx_profile.h"
#include <malloc.h>
#include <stdint.h>
#include <string.h>

#ifndef RP_RSX_BATCHING
#define RP_RSX_BATCHING 1
#endif
#ifndef RP_RSX_GRADIENT_ATLAS
#define RP_RSX_GRADIENT_ATLAS 0
#endif

#define MEG (1024u * 1024u)
#define VERT_BYTES (4u * MEG)
#define TEX_BYTES (16u * MEG)
/* Split the 16 MiB arena so temporary gradients cannot evict live bitmaps. */
#define PERSIST_BYTES (12u * MEG)
#define TRANSIENT_BYTES (TEX_BYTES - PERSIST_BYTES)
static rsxkit_target *stage;
static rsxkit_depth *depth;
static uint8_t *arena;
static uint32_t arena_offset, vertex_used, transient_used, persistent_used, fence;
static uint64_t frame_id;
static int flush_requested, compact_requested;
static int ready, current, active, arena_failed;
static float frame_clip[4]={0,0,320,240};
static uint64_t gpu_frames, cpu_frames;
static uint64_t reasons[9];
void rp_rsx_reason(uint32_t reason) {if(reason<9) reasons[reason]++;}
void rp_rsx_reason_stats(uint64_t *out) {memcpy(out,reasons,sizeof reasons);}
static struct {const uint8_t *source;uint32_t w,h,start,pitch;int premul;} uploads[128];
static unsigned upload_count;
/* IDs come from Rust's monotonic BitmapHandle identifiers, never raw
 * addresses, and generations change whenever bitmap pixels are modified. */
static struct {uint64_t id,revision,seen;uint32_t w,h,start,pitch;int premul;} persistent[384];
static unsigned persistent_count;
#if RP_RSX_GRADIENT_ATLAS
/* Immutable linear LUTs share one texture. Every row center is exactly
 * representable; horizontal filtering/wrapping and painter order are retained.
 * Radial/focal and mutable textures keep the original path. */
#define ATLAS_ROWS 1024u
#define ATLAS_SLOTS 2048u
#define ATLAS_BYTES (256u*4u*ATLAS_ROWS)
static uint8_t *gradient_atlas;
static uint32_t gradient_offset,gradient_rows;
static uint64_t gradient_ids[ATLAS_SLOTS];
static uint16_t gradient_slots[ATLAS_SLOTS];
static int gradient_dirty,gradient_reset,gradient_failed,gradient_enabled;
#endif
/* Configure once before the first movie frame. One test binary can compare
 * enabled/disabled runs without changing the PPU executable or its JIT cache. */
int rp_rsx_set_gradient_atlas(int enabled) {
#if RP_RSX_GRADIENT_ATLAS
    if(!ready||arena||active||fence)return 0;
    gradient_enabled=enabled!=0;return 1;
#else
    return enabled?0:1;
#endif
}
void rp_rsx_cache_reset(void) {
    flush_requested=1;arena_failed=0;
#if RP_RSX_GRADIENT_ATLAS
    gradient_reset=1;gradient_failed=0;
#endif
}
uint32_t rp_rsx_arena_bytes(void){
    uint32_t bytes=arena?VERT_BYTES+TEX_BYTES:0;
#if RP_RSX_GRADIENT_ATLAS
    if(gradient_atlas)bytes+=ATLAS_BYTES;
#endif
    return bytes;
}
static RpRsxProfile perf;
#if RP_RSX_GRADIENT_ATLAS
static int gradient_row(uint64_t id,const uint8_t *pixels)
{
    if(gradient_failed)return -1;
    uint32_t slot=(uint32_t)((id^(id>>33)^(id>>17))*UINT64_C(0x9e3779b97f4a7c15))&(ATLAS_SLOTS-1);
    for(unsigned probe=0;probe<ATLAS_SLOTS;probe++,slot=(slot+1)&(ATLAS_SLOTS-1)) {
        if(gradient_ids[slot]==id) {
            perf.reused_textures++;perf.persistent_hits++;
            return gradient_slots[slot];
        }
        if(gradient_ids[slot])continue;
        if(gradient_rows==ATLAS_ROWS){gradient_reset=1;return -1;}
        if(!gradient_atlas) {
            gradient_atlas=memalign(MEG,ATLAS_BYTES);
            if(!gradient_atlas){gradient_failed=1;return -1;}
            if(rsxkit_map_main(gradient_atlas,ATLAS_BYTES,&gradient_offset)!=RSXKIT_OK){
                free(gradient_atlas);gradient_atlas=NULL;gradient_failed=1;return -1;
            }
        }
        uint32_t row=gradient_rows++;
        memcpy(gradient_atlas+row*1024u,pixels,1024u);
        gradient_ids[slot]=id;gradient_slots[slot]=(uint16_t)row;gradient_dirty=1;
        perf.uploaded_textures++;perf.uploaded_bytes+=1024u;perf.persistent_uploads++;
        return (int)row;
    }
    return -1;
}
#endif
/* Preserve painter order: only adjacent draws with byte-identical GPU state
 * may be coalesced. Vertex/texture storage remains immutable until the fence. */
static rsxkit_draw pending_draw;
static uint32_t pending_start, pending_count;
void rp_rsx_flush(void)
{
    if (!pending_count) return;
    __sync_synchronize();
#if RP_RSX_GRADIENT_ATLAS
    if(gradient_dirty){
        uint64_t start=sysGetSystemTime();rsxkit_texture_sync();
        perf.texture_sync_us+=sysGetSystemTime()-start;gradient_dirty=0;
    }
#endif
    rsxkit_vertices(arena_offset+pending_start,RSXKIT_MEM_MAIN);
    rsxkit_draw_triangles(&pending_draw,0,pending_count);
    perf.submitted_batches++;
    pending_count=0;
}

/* Called only after the previous frame fence. Drop textures that have left
 * the display list and compact survivors, preserving IDs and revisions.
 * Pressure triggers collection; a full but live cache never copies itself. */
static void collect_textures(void)
{
    uint32_t used=0;
    unsigned kept=0;
    for (unsigned i=0;i<persistent_count;i++) {
        if (persistent[i].seen!=frame_id) {perf.evicted_textures++;continue;}
        uint32_t start=(used+127u)&~127u;
        uint32_t bytes=persistent[i].pitch*persistent[i].h;
        if (start!=persistent[i].start)
            memmove(arena+VERT_BYTES+start,arena+VERT_BYTES+persistent[i].start,bytes);
        persistent[kept]=persistent[i];
        persistent[kept++].start=start;
        used=start+bytes;
    }
    persistent_count=kept;persistent_used=used;compact_requested=0;
}
void rp_rsx_profile_snapshot(RpRsxProfile *out) { if(out) *out=perf; }

int rp_rsx_init(void)
{
    stage=rsxkit_target_create(640,480);
    depth=rsxkit_depth_create(640,480);
    if (!stage || !depth) goto fail;
    /* Validate GPU compatibility before committing 20 MiB of PPU RAM. */
    rp_pixel_convert_init();
    ready=1;
    return 1;
fail:
    if (stage) rsxkit_target_destroy(stage);
    if (depth) rsxkit_depth_destroy(depth);
    free(arena); stage=NULL; depth=NULL; arena=NULL;
    return 0;
}
void rp_rsx_shutdown(void)
{
    if (!ready) return;
    rsxkit_finish();
    if (arena) rsxkit_unmap_main(arena);
    free(arena);arena=NULL;arena_failed=0;fence=0;
#if RP_RSX_GRADIENT_ATLAS
    if(gradient_atlas)rsxkit_unmap_main(gradient_atlas);
    free(gradient_atlas);gradient_atlas=NULL;gradient_rows=0;gradient_dirty=gradient_reset=gradient_failed=gradient_enabled=0;
    memset(gradient_ids,0,sizeof gradient_ids);
#endif
    persistent_count=persistent_used=0;pending_count=0;
    rsxkit_depth_destroy(depth); rsxkit_target_destroy(stage);
    ready=0; current=0;
}
int rp_rsx_begin(uint32_t rgb)
{
    if (!ready || arena_failed) return 0;
    if (!arena) {
        uint8_t *candidate=memalign(MEG,VERT_BYTES+TEX_BYTES);
        if (!candidate) {arena_failed=1;return 0;}
        if (rsxkit_map_main(candidate,VERT_BYTES+TEX_BYTES,&arena_offset)!=RSXKIT_OK) {
            free(candidate);arena_failed=1;return 0;
        }
        arena=candidate;
    }
    if (fence) {
        uint64_t start=sysGetSystemTime();
        rsxkit_fence_wait(fence);
        perf.fence_wait_us+=sysGetSystemTime()-start;
    }
    current=0; active=0; pending_count=0; vertex_used=transient_used=upload_count=0;
    frame_clip[0]=frame_clip[1]=0;frame_clip[2]=320;frame_clip[3]=240;
#if RP_RSX_GRADIENT_ATLAS
    /* Reset only after the frame fence. Never overwrite a row referenced by
     * pending GPU work; overflow in this frame uses ordinary textures. */
    if(gradient_reset){
        perf.evicted_textures+=gradient_rows;gradient_rows=0;gradient_reset=0;
        memset(gradient_ids,0,sizeof gradient_ids);
    }
#endif
    if (flush_requested) {persistent_used=0; persistent_count=0; flush_requested=0;compact_requested=0;}
    else if (compact_requested) collect_textures();
    frame_id++;
    if (rsxkit_bind(stage,depth,320,240)!=RSXKIT_OK) return 0;
    rsxkit_clear(RSXKIT_CLEAR_COLOR|RSXKIT_CLEAR_DEPTH,rgb,1,NULL);
    {
        uint64_t start=sysGetSystemTime();
        rsxkit_texture_sync();
        perf.texture_sync_us+=sysGetSystemTime()-start;
    }
    active=1;
    return 1;
}
int rp_rsx_clip(int32_t left,int32_t top,int32_t right,int32_t bottom)
{
    if(!active || left<0 || top<0 || right>320 || bottom>240 || left>right || top>bottom) return 0;
    frame_clip[0]=(float)left;frame_clip[1]=(float)top;
    frame_clip[2]=(float)right;frame_clip[3]=(float)bottom;return 1;
}
int rp_rsx_mesh(const rsxkit_vertex *v, uint32_t n,
                const uint8_t *pixels,uint32_t w,uint32_t h,int premultiplied,
                uint8_t wrap,uint8_t filter,uint8_t gradient,float focal,const float mult[4],const float add[4],uint64_t texture_id,uint64_t revision)
{
    if (!active || !v || n<3 || n%3) {perf.failed_draws++;return 0;}
    uint64_t bytes=(uint64_t)n*sizeof(*v);
    if (bytes>VERT_BYTES || vertex_used>VERT_BYTES-(uint32_t)bytes) { active=0;perf.failed_draws++;return 0; }
    rsxkit_draw d; memset(&d,0,sizeof d);
    d.blend=RSXKIT_BLEND_ALPHA; d.cull=RSXKIT_CULL_NONE;
    memcpy(d.scissor,frame_clip,sizeof frame_clip);
#if RP_RSX_GRADIENT_ATLAS
    int row=-1;
    if(gradient_enabled&&pixels&&w==256&&h==1&&!premultiplied&&gradient==0&&texture_id&&revision==0){
        int linear=1;
        for(uint32_t i=0;i<n;i++)if(v[i].st[1]!=0.5f){linear=0;break;}
        if(linear)row=gradient_row(texture_id,pixels);
    }
    if(row>=0){
        d.ntex=1;d.tex[0].offset=gradient_offset;d.tex[0].pitch=1024;
        d.tex[0].width=256;d.tex[0].height=ATLAS_ROWS;
        d.tex[0].location=RSXKIT_MEM_MAIN;d.tex[0].format=RSXKIT_RGBA8888;
        d.tex[0].wrap_s=wrap;d.tex[0].wrap_t=0;d.tex[0].filter=filter;
        d.tex[0].xf[0]=d.tex[0].xf[1]=1.0f;
        d.k[RSXKIT_K_T0A][0][0]=focal;
        for(int i=0;i<4;i++){d.k[RSXKIT_K_T0][0][i]=mult[i];d.k[RSXKIT_K_ONE][0][i]=add[i];}
    }else
#endif
    if (pixels) {
        if (!w || !h || w>4096 || h>4096 || w>UINT32_MAX/4u) {active=0;perf.failed_draws++;return 0;}
        uint32_t pitch=(w*4u+63u)&~63u;
        uint64_t need=(uint64_t)pitch*h;
        uint32_t start=0;
        int cached=0, stable=0, slot=-1;
        if (texture_id) {
            for (unsigned i=0;i<persistent_count;i++) {
                if (persistent[i].id==texture_id) {slot=(int)i;break;}
            }
            if (slot>=0) {
                /* Updates preserve dimensions in Ruffle. Defensive check
                 * prevents a stale identity from reading outside its slot. */
                if (persistent[slot].w!=w || persistent[slot].h!=h ||
                    persistent[slot].premul!=premultiplied) {
                    active=0;perf.failed_draws++;return 0;
                }
                start=persistent[slot].start;
                pitch=persistent[slot].pitch;
                stable=1;
                if (persistent[slot].revision==revision) cached=1;
                else if (persistent[slot].seen==frame_id) {
                    /* Never overwrite a texture already queued this frame. */
                    active=0;perf.failed_draws++;return 0;
                }
            } else {
                uint32_t aligned=(persistent_used+127u)&~127u;
                if (need<=PERSIST_BYTES && aligned<=PERSIST_BYTES-(uint32_t)need &&
                    persistent_count<sizeof(persistent)/sizeof(persistent[0])) {
                    slot=(int)persistent_count++;
                    start=aligned;
                    persistent_used=aligned+(uint32_t)need;
                    persistent[slot].id=texture_id;
                    persistent[slot].w=w;
                    persistent[slot].h=h;
                    persistent[slot].premul=premultiplied;
                    persistent[slot].start=start;
                    persistent[slot].pitch=pitch;
                    persistent[slot].revision=revision;
                    persistent[slot].seen=0;
                    stable=1;
                }
            }
        }
        if (!stable) {
            if (texture_id) compact_requested=1;
            for (unsigned i=0;i<upload_count;i++)
                if (uploads[i].source==pixels && uploads[i].w==w && uploads[i].h==h &&
                    uploads[i].premul==premultiplied) {
                    start=uploads[i].start;
                    pitch=uploads[i].pitch;
                    cached=1;break;
                }
            if (!cached) {
                uint32_t aligned=(transient_used+127u)&~127u;
                if (need>TRANSIENT_BYTES || aligned>TRANSIENT_BYTES-(uint32_t)need) {
                    active=0;perf.failed_draws++;return 0;
                }
                start=PERSIST_BYTES+aligned;
                transient_used=aligned+(uint32_t)need;
                if(upload_count<128) {
                    uploads[upload_count].source=pixels;uploads[upload_count].w=w;
                    uploads[upload_count].h=h;uploads[upload_count].premul=premultiplied;
                    uploads[upload_count].start=start;uploads[upload_count].pitch=pitch;
                    upload_count++;
                }
            }
        }
        uint8_t *out=arena+VERT_BYTES+start;
        if (!cached) {
            uint64_t conversion_start=sysGetSystemTime();
            for (uint32_t y=0;y<h;y++) {
                const uint8_t *src=pixels+(size_t)y*w*4u;
                uint8_t *dst=out+(size_t)y*pitch;
                if (premultiplied) rp_unpremultiply_rgba(dst,src,w);
                else memcpy(dst,src,(size_t)w*4u);
            }
            perf.conversion_us+=sysGetSystemTime()-conversion_start;
            perf.uploaded_textures++;
            perf.uploaded_bytes+=(uint64_t)w*h*4u;
            if (stable) {persistent[slot].revision=revision;perf.persistent_uploads++;}
            else perf.transient_uploads++;
        } else {
            perf.reused_textures++;
            if (stable) perf.persistent_hits++;
        }
        if (stable) persistent[slot].seen=frame_id;
        d.ntex=1; d.gradient=gradient; d.k[RSXKIT_K_T0A][0][0]=focal; d.tex[0].offset=arena_offset+VERT_BYTES+start;
        d.tex[0].pitch=pitch; d.tex[0].width=(uint16_t)w; d.tex[0].height=(uint16_t)h;
        d.tex[0].location=RSXKIT_MEM_MAIN; d.tex[0].format=RSXKIT_RGBA8888;
        d.tex[0].wrap_s=wrap; d.tex[0].wrap_t=wrap; d.tex[0].filter=filter;
        d.tex[0].xf[0]=d.tex[0].xf[1]=1.0f;
        for (int i=0;i<4;i++) {d.k[RSXKIT_K_T0][0][i]=mult[i]; d.k[RSXKIT_K_ONE][0][i]=add[i];}
    } else {
        for (int i=0;i<4;i++) d.k[RSXKIT_K_ONE][1][i]=1.0f;
    }
    memcpy(arena+vertex_used,v,(size_t)bytes);
#if RP_RSX_GRADIENT_ATLAS
    if(row>=0){
        rsxkit_vertex *out=(rsxkit_vertex*)(arena+vertex_used);
        float center=((float)row+0.5f)/(float)ATLAS_ROWS;
        for(uint32_t i=0;i<n;i++)out[i].st[1]=center;
    }
#endif
    if (pending_count && memcmp(&pending_draw,&d,sizeof d)==0) {
        pending_count+=n;
        perf.merged_draws++;
    } else {
        rp_rsx_flush();
        pending_draw=d;pending_start=vertex_used;pending_count=n;
    }
    perf.draw_calls++; perf.vertices+=n;
    vertex_used+=(uint32_t)bytes;
#if !RP_RSX_BATCHING
    rp_rsx_flush();
#endif
    return 1;
}
void rp_rsx_end(int ok)
{
    if (!ready) return;
    if (ok && active) rp_rsx_flush();
    else pending_count=0;
    fence=rsxkit_fence();
    current=ok && active;
    if (current) gpu_frames++;
    active=0;
}
int rp_rsx_current(void) {return current;}
int rp_rsx_ready(void) {return ready;}
void rp_rsx_software(void) {current=0;cpu_frames++;}
void rp_rsx_present(void)
{
    if (!current) return;
    rsxkit_view view={RSXKIT_SCALE_FIT,RSXKIT_FILTER_LINEAR,4.0f/3.0f};
    rsxkit_target_present(stage,NULL,640,480,&view,NULL);
    fence=rsxkit_fence();
}
void rp_rsx_stats(uint64_t *gpu,uint64_t *cpu) {*gpu=gpu_frames;*cpu=cpu_frames;}
