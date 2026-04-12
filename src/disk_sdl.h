/********************************************************************/
/* disk_sdl.h - Direct asset loading for SDL2 (replaces .dat files) */
/********************************************************************/

#ifndef LIQUID_WAR_INCLUDE_DISK_SDL
#define LIQUID_WAR_INCLUDE_DISK_SDL

#include "sdl_compat.h"

void lw_disk_sdl_set_data_dir (const char *dat_path);

int lw_disk_sdl_load_maps (void **maps, int max_maps);
int lw_disk_sdl_load_sfx (SAMPLE ** samples, int max_samples,
                           const char *subdir);
int lw_disk_sdl_load_music (MIDI ** music, int max_music);
BITMAP *lw_disk_sdl_load_back (void);
BITMAP *lw_disk_sdl_load_font_bitmap (const char *name);

#endif
