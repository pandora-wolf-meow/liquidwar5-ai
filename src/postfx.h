/********************************************************************/
/* postfx.h - Post-processing visual effects for SDL2 renderer      */
/********************************************************************/

#ifndef LIQUID_WAR_INCLUDE_POSTFX
#define LIQUID_WAR_INCLUDE_POSTFX

#include <SDL2/SDL.h>

/*
 * Apply battle glow effect to the 32-bit frame buffer.
 * Detects boundaries where neighboring pixels differ significantly
 * and adds a bright additive glow along those edges.
 */
void lw_postfx_battle_glow (Uint32 * pixels, int w, int h, int pitch,
                             int intensity);

/*
 * Apply liquid ripple distortion to the frame buffer.
 * Creates a subtle wave displacement effect that makes the
 * game area look like a fluid surface.
 */
void lw_postfx_liquid_ripple (Uint32 * pixels, int w, int h, int pitch,
                               float time, float amplitude);

/*
 * Liquid ripple that only affects pixels where the 8-bit index
 * is >= min_index (i.e. army/team colors, not background).
 */
void lw_postfx_liquid_ripple_masked (Uint32 * pixels, int w, int h,
                                      int pitch, float time,
                                      float amplitude,
                                      unsigned char *index_map,
                                      int index_pitch, int min_index);

/*
 * Apply a radial gradient vignette that darkens the corners of the
 * frame buffer. Strength is the darkening applied at the corners,
 * e.g. 0.35 -> corners at ~65% brightness, center at 100%.
 */
void lw_postfx_vignette (Uint32 * pixels, int w, int h, int pitch,
                          float strength);

#endif
