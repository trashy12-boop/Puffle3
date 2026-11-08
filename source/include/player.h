#ifndef RP_PLAYER_H
#define RP_PLAYER_H
#include "controls.h"
#include <stddef.h>
#include <stdint.h>
uint64_t rp_rust_probe(void);
void *rp_player_open(const unsigned char *data,size_t len,uint32_t width,uint32_t height,const char *path);
void rp_player_close(void *player);
void rp_player_tick(void *player,double milliseconds);
void rp_player_set_software(void *player,int software);
int rp_player_render(void *player);
uint64_t rp_player_next_tick_us(void *player);
void rp_log_flush(void);
void rp_player_event(void *player,const RpEvent *event);
void rp_player_mix(void *player,float *out,size_t samples);
int rp_audio_init(void);
void rp_audio_pump(void *player);
void rp_audio_pause(int pause);
void rp_audio_reset(void);
void rp_audio_shutdown(void);
size_t rp_rust_bytes(void);
size_t rp_rust_peak(void);

typedef struct { uint64_t paths,hits,misses,bytes; } RpCacheStats;
void rp_player_cache_stats(void *player,RpCacheStats *out);

typedef struct {
 uint64_t vm_us,collision_us,gc_us,opcodes,gets,sets,calls,hit_tests,frames,vm_entries;
} RpVmStats;
int rp_player_vm_stats(RpVmStats *out);
void rp_player_mesh_stats(void *player,uint64_t out[3]);
#endif
