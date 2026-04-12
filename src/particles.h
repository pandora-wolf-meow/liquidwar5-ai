/********************************************************************/
/* particles.h - Particle effects system for Liquid War 5           */
/********************************************************************/

#ifndef LIQUID_WAR_INCLUDE_PARTICLES
#define LIQUID_WAR_INCLUDE_PARTICLES

#include "sdl_compat.h"

#define LW_MAX_PARTICLES 512

typedef struct
{
  float x, y;
  float vx, vy;
  float life;
  float max_life;
  int color;
  int size;
  int active;
} LW_PARTICLE;

void lw_particles_init (void);
void lw_particles_spawn (float x, float y, int count, int color, int type);
void lw_particles_update (float dt);
void lw_particles_draw (BITMAP * bmp);
void lw_particles_draw_scaled (BITMAP * bmp, float scale_x, float scale_y);
void lw_particles_clear (void);

/* Particle types */
#define LW_PARTICLE_SPARK    0
#define LW_PARTICLE_SPLASH   1
#define LW_PARTICLE_GLOW     2

#endif
