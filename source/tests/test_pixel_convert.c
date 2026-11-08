/* SPDX-License-Identifier: MIT */
#include "pixel_convert.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
static uint8_t original(uint8_t component, uint8_t alpha) {
    unsigned value = alpha ? (unsigned)component * 255u / alpha : 0;
    return (uint8_t)(value > 255u ? 255u : value);
}
int main(void) {
    rp_pixel_convert_init();
    /* All 65,536 possible component/alpha combinations, including
     * alpha zero, full opacity, and inputs that must saturate. */
    for (unsigned alpha=0; alpha<256; alpha++) {
        for (unsigned c=0; c<256; c++) {
            uint8_t in[4]={(uint8_t)c,(uint8_t)c,(uint8_t)c,(uint8_t)alpha};
            uint8_t out[4]={};
            rp_unpremultiply_rgba(out,in,1);
            assert(out[0]==original((uint8_t)c,(uint8_t)alpha));
            assert(out[1]==out[0]&&out[2]==out[0]&&out[3]==alpha);
            rp_unpremultiply_rgba(in,in,1);
            assert(memcmp(out,in,4)==0);
        }
    }
    uint8_t in[4096], out[4096];
    for (unsigned i=0;i<1024;i++) {
        in[i*4+0]=(uint8_t)((i*17u)&255u);
        in[i*4+1]=(uint8_t)((i*23u)&255u);
        in[i*4+2]=(uint8_t)((i*47u)&255u);
        in[i*4+3]=(uint8_t)((i*7u)&255u);
    }
    rp_unpremultiply_rgba(out,in,1024);
    for (unsigned i=0;i<1024;i++) {
        for (unsigned c=0;c<3;c++) assert(out[i*4+c]==original(in[i*4+c],in[i*4+3]));
        assert(out[i*4+3]==in[i*4+3]);
    }
    puts("PS3 RGBA conversion: all component/alpha pairs and 1024 pixels passed");
    return 0;
}
