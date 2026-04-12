/********************************************************************/
/* particles.c - Particle effects system for Liquid War 5           */
/********************************************************************/

#include <stdlib.h>
#include <math.h>
#include "particles.h"

/*==================================================================*/
/* variables                                                        */
/*==================================================================*/

static LW_PARTICLE particles[LW_MAX_PARTICLES];
static int particle_count = 0;

/*==================================================================*/
/* functions                                                        */
/*==================================================================*/

void
lw_particles_init (void)
{
  int i;
  for (i = 0; i < LW_MAX_PARTICLES; ++i)
    particles[i].active = 0;
  particle_count = 0;
}

void
lw_particles_clear (void)
{
  lw_particles_init ();
}

static float
randf (void)
{
  return (float) rand () / (float) RAND_MAX;
}

void
lw_particles_spawn (float x, float y, int count, int color, int type)
{
  int i, j;

  for (j = 0; j < count; ++j)
    {
      /* Find a free slot */
      for (i = 0; i < LW_MAX_PARTICLES; ++i)
        {
          if (!particles[i].active)
            break;
        }
      if (i >= LW_MAX_PARTICLES)
        return;

      particles[i].active = 1;
      particles[i].x = x;
      particles[i].y = y;
      particles[i].color = color;

      switch (type)
        {
        case LW_PARTICLE_SPARK:
          {
            float angle = randf () * 2.0f * 3.14159f;
            float speed = 10.0f + randf () * 30.0f;
            particles[i].vx = cosf (angle) * speed;
            particles[i].vy = sinf (angle) * speed;
            particles[i].life = 0.3f + randf () * 0.6f;
            particles[i].max_life = particles[i].life;
            particles[i].size = 1;
          }
          break;

        case LW_PARTICLE_SPLASH:
          {
            float angle = randf () * 2.0f * 3.14159f;
            float speed = 10.0f + randf () * 30.0f;
            particles[i].vx = cosf (angle) * speed;
            particles[i].vy = sinf (angle) * speed - 20.0f;
            particles[i].life = 0.5f + randf () * 0.8f;
            particles[i].max_life = particles[i].life;
            particles[i].size = 1 + (int) (randf () * 2);
          }
          break;

        case LW_PARTICLE_GLOW:
          {
            float angle = randf () * 2.0f * 3.14159f;
            float speed = 5.0f + randf () * 15.0f;
            particles[i].vx = cosf (angle) * speed;
            particles[i].vy = sinf (angle) * speed;
            particles[i].life = 0.8f + randf () * 1.2f;
            particles[i].max_life = particles[i].life;
            particles[i].size = 2 + (int) (randf () * 3);
          }
          break;
        }

      if (particle_count < LW_MAX_PARTICLES)
        particle_count++;
    }
}

void
lw_particles_update (float dt)
{
  int i;

  for (i = 0; i < LW_MAX_PARTICLES; ++i)
    {
      if (!particles[i].active)
        continue;

      particles[i].x += particles[i].vx * dt;
      particles[i].y += particles[i].vy * dt;

      /* Gravity for splashes */
      particles[i].vy += 40.0f * dt;

      /* Friction */
      particles[i].vx *= (1.0f - 0.5f * dt);
      particles[i].vy *= (1.0f - 0.5f * dt);

      particles[i].life -= dt;
      if (particles[i].life <= 0)
        {
          particles[i].active = 0;
          particle_count--;
        }
    }
}

static void
draw_particles_internal (BITMAP * bmp, float sx, float sy)
{
  int i, px, py, s;
  float alpha;

  if (!bmp)
    return;

  for (i = 0; i < LW_MAX_PARTICLES; ++i)
    {
      if (!particles[i].active)
        continue;

      px = (int) (particles[i].x * sx);
      py = (int) (particles[i].y * sy);
      s = (int) (particles[i].size * (sx > sy ? sx : sy));
      if (s < 1)
        s = 1;
      alpha = particles[i].life / particles[i].max_life;

      /* Only draw if visible and alpha high enough */
      if (alpha < 0.1f)
        continue;
      if (px < 0 || py < 0 || px >= bmp->w || py >= bmp->h)
        continue;

      /* Draw particle as small filled shape */
      if (s <= 1)
        {
          putpixel (bmp, px, py, particles[i].color);
        }
      else
        {
          int dx, dy;
          for (dy = -s / 2; dy <= s / 2; ++dy)
            for (dx = -s / 2; dx <= s / 2; ++dx)
              {
                if (dx * dx + dy * dy <= (s * s) / 4)
                  {
                    int drawx = px + dx;
                    int drawy = py + dy;
                    if (drawx >= 0 && drawx < bmp->w
                        && drawy >= 0 && drawy < bmp->h)
                      putpixel (bmp, drawx, drawy, particles[i].color);
                  }
              }
        }
    }
}

void
lw_particles_draw (BITMAP * bmp)
{
  draw_particles_internal (bmp, 1.0f, 1.0f);
}

void
lw_particles_draw_scaled (BITMAP * bmp, float scale_x, float scale_y)
{
  draw_particles_internal (bmp, scale_x, scale_y);
}
