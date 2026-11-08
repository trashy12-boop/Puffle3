#ifndef RUFFLE_PS3_CONTROLS_H
#define RUFFLE_PS3_CONTROLS_H
#include <stdint.h>
#include <stdbool.h>

enum { RP_CROSS, RP_CIRCLE, RP_SQUARE, RP_TRIANGLE, RP_UP, RP_DOWN, RP_LEFT, RP_RIGHT,
       RP_L1, RP_L2, RP_R1, RP_R2, RP_L3, RP_R3, RP_START, RP_SELECT,
       RP_LSTICK_UP, RP_LSTICK_DOWN, RP_LSTICK_LEFT, RP_LSTICK_RIGHT,
       RP_RSTICK_UP, RP_RSTICK_DOWN, RP_RSTICK_LEFT, RP_RSTICK_RIGHT, RP_BUTTONS };
enum { RP_BIND_NONE=0, RP_MOUSE_LEFT=256, RP_MOUSE_RIGHT=257 };
#define RP_BINDINGS 3
#define RP_MENU_MASK ((1u << RP_L3) | (1u << RP_R3))

typedef struct {
    uint16_t binding[RP_BUTTONS][RP_BINDINGS]; /* key: Windows virtual-key style 1..255 */
    uint16_t mouse_speed; /* logical pixels per second */
    uint8_t deadzone;     /* percent */
    uint8_t acceleration; /* 0..200 percent */
    uint8_t renderer;     /* 0 automatic RSX/fallback, 1 software */
    uint8_t mouse_stick;  /* 0 off, 1 left, 2 right */
} RpProfile;

typedef enum { RP_EVENT_KEY, RP_EVENT_MOUSE_BUTTON, RP_EVENT_MOUSE_MOVE, RP_EVENT_MENU } RpEventType;
typedef struct { RpEventType type; unsigned code; bool down; double x, y; } RpEvent;
typedef void (*RpEmit)(void *ctx, const RpEvent *event);

typedef struct { uint32_t buttons; int16_t lx, ly, rx, ry; } RpPad;
typedef struct {
    bool keys[256], mouse[2], menu_open, chord_latched, release_gate;
    double cursor_x, cursor_y, l3_seconds, r3_seconds;
} RpInput;

void rp_profile_default(RpProfile *p);
void rp_profile_mario63(RpProfile *p);
void rp_profile_for_game(RpProfile *p, const char *path);
bool rp_profile_valid(const RpProfile *p);
void rp_input_init(RpInput *s, double width, double height);
/* width/height are movie-viewport coordinates, not output-screen coordinates.
   On menu transitions all delivered buttons are released; after closing, pad
   actions stay blocked until the physical controls return to neutral. */
void rp_input_update(RpInput *s, const RpProfile *p, RpPad pad, double dt,
                     double width, double height, RpEmit emit, void *ctx);
void rp_input_release_all(RpInput *s, RpEmit emit, void *ctx);
#endif
