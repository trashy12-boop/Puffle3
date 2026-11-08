/* Portable fake RSX declarations for host tests ONLY, never PS3 builds. */
#ifndef RSXKIT_MOCK_H
#define RSXKIT_MOCK_H
#include <stdint.h>
typedef struct rsxkit_target {int unused;} rsxkit_target;
typedef struct rsxkit_depth {int unused;} rsxkit_depth;
typedef struct rsxkit_vertex {float pos[4],st[2];uint8_t rgba[4];uint32_t pad;} rsxkit_vertex;
typedef struct rsxkit_tex {uint32_t offset,pitch;uint16_t width,height;uint8_t location,format,wrap_s,wrap_t,filter;float xf[2];} rsxkit_tex;
typedef struct rsxkit_draw {int blend,cull,ntex,gradient;uint32_t scissor[4];float k[4][2][4];rsxkit_tex tex[1];} rsxkit_draw;
typedef struct rsxkit_view {int mode,filter;float aspect;} rsxkit_view;
#define RSXKIT_OK 0
#define RSXKIT_CLEAR_COLOR 1
#define RSXKIT_CLEAR_DEPTH 2
#define RSXKIT_BLEND_ALPHA 1
#define RSXKIT_CULL_NONE 0
#define RSXKIT_K_T0A 0
#define RSXKIT_K_T0 1
#define RSXKIT_K_ONE 2
#define RSXKIT_MEM_MAIN 0
#define RSXKIT_RGBA8888 0
#define RSXKIT_SCALE_FIT 0
#define RSXKIT_FILTER_LINEAR 0
rsxkit_target *rsxkit_target_create(unsigned w,unsigned h);
rsxkit_depth *rsxkit_depth_create(unsigned w,unsigned h);
void rsxkit_target_destroy(rsxkit_target *);
void rsxkit_depth_destroy(rsxkit_depth *);
int rsxkit_map_main(void *,uint32_t,uint32_t *);
void rsxkit_unmap_main(void *);
void rsxkit_finish(void);
void rsxkit_fence_wait(uint32_t);
int rsxkit_bind(rsxkit_target *,rsxkit_depth *,unsigned,unsigned);
void rsxkit_clear(int,uint32_t,float,void *);
void rsxkit_texture_sync(void);
void rsxkit_vertices(uint32_t,uint8_t);
void rsxkit_draw_triangles(const rsxkit_draw *,uint32_t,uint32_t);
uint32_t rsxkit_fence(void);
void rsxkit_target_present(rsxkit_target *,void *,unsigned,unsigned,const rsxkit_view *,void *);
#endif
