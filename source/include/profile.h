#ifndef RUFFLE_PS3_PROFILE_H
#define RUFFLE_PS3_PROFILE_H
#include "controls.h"
/* Profiles are keyed by the complete SWF path, stored in the record and
   checked on load. A hash collision never silently loads another profile. */
int rp_profile_save(const char *directory, const char *game_path, const RpProfile *profile);
int rp_profile_load(const char *directory, const char *game_path, RpProfile *profile);
#endif
