#include "controls.h"
/* Calls the shared controller mapper. The host Rust callback translates only
   Space and mouse events; this fixture is not a complete PS3/Ruffle bridge. */
void rp_host_mapped_input_fixture(RpEmit emit, void *ctx) {
    RpProfile profile; RpInput input; RpPad pad = {0};
    rp_profile_default(&profile); profile.binding[RP_CROSS][0] = 32;
    rp_input_init(&input, 640, 480);
    pad.buttons = 1u << RP_CROSS;
    rp_input_update(&input, &profile, pad, 0.016, 640, 480, emit, ctx);
    pad.buttons = 0;
    rp_input_update(&input, &profile, pad, 0.016, 640, 480, emit, ctx);
}
