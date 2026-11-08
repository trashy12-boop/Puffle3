/* SPDX-License-Identifier: MIT */
#ifndef PUFFLE_FRAME_STATS_H
#define PUFFLE_FRAME_STATS_H
#include <stdint.h>
#include <stdlib.h>
/* Bounded storage for one roughly two-second report, at up to 120 Hz.
 * Report overflow rather than silently replacing tail samples. */
#define RP_FRAME_SAMPLES 256u
typedef struct {
    uint64_t us[RP_FRAME_SAMPLES];
    uint64_t total_us;
    unsigned count, dropped;
} RpFrameSamples;
static inline void rp_frame_sample(RpFrameSamples *s, uint64_t us) {
    if (s->count == RP_FRAME_SAMPLES) { s->dropped++; return; }
    s->us[s->count++] = us;
    s->total_us += us;
}
static inline int rp_frame_compare(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}
/* Nearest-rank p99; 1000 / p99_ms is a percentile-equivalent low FPS,
 * not the mean FPS of the slowest one percent of frames. */
static inline uint64_t rp_frame_p99(RpFrameSamples *s) {
    if (!s->count) return 0;
    qsort(s->us, s->count, sizeof(s->us[0]), rp_frame_compare);
    return s->us[(s->count * 99u + 99u) / 100u - 1u];
}
#endif
