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

#endif
