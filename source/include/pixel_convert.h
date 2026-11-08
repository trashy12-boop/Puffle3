/* SPDX-License-Identifier: MIT */
#ifndef RUFFLE_PS3_PIXEL_CONVERT_H
#define RUFFLE_PS3_PIXEL_CONVERT_H
#include <stddef.h>
#include <stdint.h>

/* Initialize once at startup, before any rendering begins. */
void rp_pixel_convert_init(void);
/* Convert premultiplied RGBA8 to straight RGBA8, exactly matching the
 * integer division/clamping used in the original renderer. Buffers may
 * overlap exactly (src == dst), but otherwise must not overlap. */
void rp_unpremultiply_rgba(uint8_t *dst, const uint8_t *src, size_t pixels);
#endif
