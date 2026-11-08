/* SPDX-License-Identifier: MIT */
#ifndef RUFFLE_PS3_RSX_PROFILE_H
#define RUFFLE_PS3_RSX_PROFILE_H
#include <stdint.h>
typedef struct RpRsxProfile {
    uint64_t draw_calls, vertices;
    uint64_t uploaded_textures, reused_textures, uploaded_bytes;
    uint64_t conversion_us, fence_wait_us, texture_sync_us;
    uint64_t failed_draws;
    uint64_t persistent_hits, persistent_uploads, transient_uploads;
    uint64_t evicted_textures;
    uint64_t submitted_batches, merged_draws;
} RpRsxProfile;
uint32_t rp_rsx_arena_bytes(void);
void rp_rsx_profile_snapshot(RpRsxProfile *out);
#endif
