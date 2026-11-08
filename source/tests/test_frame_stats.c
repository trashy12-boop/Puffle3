/* SPDX-License-Identifier: MIT */
#include "frame_stats.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    RpFrameSamples s = {0};
    assert(rp_frame_p99(&s) == 0);
    /* An isolated stall affects the tail statistic but not most frames. */
    for (unsigned i=0; i<99; ++i) rp_frame_sample(&s, 31250);
    rp_frame_sample(&s, 200000);
    assert(rp_frame_p99(&s) == 31250);
    rp_frame_sample(&s, 200000);
    assert(rp_frame_p99(&s) == 200000);
    while (s.count < RP_FRAME_SAMPLES) rp_frame_sample(&s, 31250);
    uint64_t total = s.total_us;
    rp_frame_sample(&s, UINT64_MAX);
    assert(s.count == RP_FRAME_SAMPLES && s.dropped == 1 && s.total_us == total);
    puts("frame interval percentiles: passed");
    return 0;
}
