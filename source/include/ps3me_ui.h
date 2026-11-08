/* SPDX-License-Identifier: GPL-2.0-only */
/* PS3ME 01.16 layout and colors, ported to Ruffle's batched RSX UI.
 * Reference: trashy12-boop/PS3ME-a-PS2ME-Fork ps3/src/BrowserUI.cpp
 * and ps3/src/InGameMenu.cpp. No SDL or J2ME dependency. */
#ifndef RUFFLE_PS3_PS3ME_UI_H
#define RUFFLE_PS3_PS3ME_UI_H
#include <stdint.h>

enum { RP_UI_TITLE, RP_UI_NORMAL, RP_UI_SMALL };
void rp_ps3me_background(int browser_waves);
void rp_ps3me_label(float x, float y, const char *str, int face, float max_width);
void rp_ps3me_rule(float y);
void rp_ps3me_highlight(float x, float y, float width, float height);
void rp_ps3me_file_icon(float x, float y, float size, int directory);
int rp_ps3me_row_y(int index, int selected);
#endif
