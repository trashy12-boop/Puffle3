/* SPDX-License-Identifier: MIT */
#include "pixel_convert.h"
#include <string.h>

/* 64 KiB trades a small, fixed read-only-after-init table for three
 * 32-bit divisions per pixel on the PS3 PPE.  Exact truncation and
 * saturation are preserved for arbitrary premultiplied RGBA8 input. */
static uint8_t unpremul[256][256];
void rp_pixel_convert_init(void) {
    for (unsigned alpha = 0; alpha < 256; ++alpha) {
        for (unsigned component = 0; component < 256; ++component) {
            unsigned value = alpha ? component * 255u / alpha : 0;
            unpremul[alpha][component] = (uint8_t)(value > 255 ? 255 : value);
        }
    }
}
void rp_unpremultiply_rgba(uint8_t *dst, const uint8_t *src, size_t pixels) {
    for (size_t i = 0; i < pixels; ++i) {
        unsigned a = src[3];
        if (a == 255) {
            memcpy(dst, src, 4);
        } else {
            const uint8_t *row = unpremul[a];
            dst[0] = row[src[0]];
            dst[1] = row[src[1]];
            dst[2] = row[src[2]];
            dst[3] = (uint8_t)a;
        }
        dst += 4;
        src += 4;
    }
}
