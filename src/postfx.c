/********************************************************************/
/* postfx.c - Post-processing visual effects for SDL2 renderer      */
/********************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "postfx.h"

/*==================================================================*/
/* Helpers                                                          */
/*==================================================================*/

static inline int
color_diff (Uint32 a, Uint32 b)
{
  int dr = ((a >> 16) & 0xFF) - ((b >> 16) & 0xFF);
  int dg = ((a >> 8) & 0xFF) - ((b >> 8) & 0xFF);
  int db = (a & 0xFF) - (b & 0xFF);
  return dr * dr + dg * dg + db * db;
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline Uint32
add_glow (Uint32 pixel, int gr, int gg, int gb)
{
  int r = ((pixel >> 16) & 0xFF) + gr;
  int g = ((pixel >> 8) & 0xFF) + gg;
  int b = (pixel & 0xFF) + gb;
  if (r > 255)
    r = 255;
  if (g > 255)
    g = 255;
  if (b > 255)
    b = 255;
  return (255u << 24) | (r << 16) | (g << 8) | b;
}

/*==================================================================*/
/* Battle glow                                                      */
/*==================================================================*/

void
lw_postfx_battle_glow (Uint32 * pixels, int w, int h, int pitch,
                        int intensity)
{
  int x, y;
  int threshold = 3000;         /* high threshold: only army boundaries, not textures */
  static unsigned char *edge_map = NULL;
  static int edge_map_size = 0;
  int glow_radius = 2;
  float pulse;
  static int frame = 0;

  frame++;

  if (w * h > edge_map_size)
    {
      if (edge_map)
        free (edge_map);
      edge_map = (unsigned char *) calloc (w * h, 1);
      edge_map_size = w * h;
    }
  if (!edge_map)
    return;

  memset (edge_map, 0, w * h);

  /* Pulsing intensity */
  pulse = 0.7f + 0.3f * sinf (frame * 0.08f);

  /* Pass 1: detect boundary pixels */
  for (y = 1; y < h - 1; ++y)
    {
      Uint32 *row = pixels + y * pitch;
      Uint32 *row_up = pixels + (y - 1) * pitch;
      Uint32 *row_dn = pixels + (y + 1) * pitch;

      for (x = 1; x < w - 1; ++x)
        {
          Uint32 c = row[x];
          int is_edge = 0;

          if (color_diff (c, row[x - 1]) > threshold)
            is_edge = 1;
          else if (color_diff (c, row[x + 1]) > threshold)
            is_edge = 1;
          else if (color_diff (c, row_up[x]) > threshold)
            is_edge = 1;
          else if (color_diff (c, row_dn[x]) > threshold)
            is_edge = 1;

          if (is_edge)
            edge_map[y * w + x] = 255;
        }
    }

  /* Pass 2: for each edge pixel, add glow to surrounding area */
  for (y = 1; y < h - 1; ++y)
    {
      for (x = 1; x < w - 1; ++x)
        {
          if (!edge_map[y * w + x])
            continue;

          /* This pixel is an edge - glow it and neighbors */
          {
            int dx, dy;
            for (dy = -glow_radius; dy <= glow_radius; ++dy)
              {
                int gy = y + dy;
                if (gy < 0 || gy >= h)
                  continue;
                for (dx = -glow_radius; dx <= glow_radius; ++dx)
                  {
                    int gx = x + dx;
                    int dist, strength;
                    if (gx < 0 || gx >= w)
                      continue;
                    dist = abs (dx) + abs (dy);
                    strength =
                      (int) (intensity * pulse * (glow_radius + 1 -
                                                   dist)) /
                      (glow_radius + 1);
                    if (strength > 0)
                      pixels[gy * pitch + gx] =
                        add_glow (pixels[gy * pitch + gx], strength,
                                  strength, (strength * 3) / 4);
                  }
              }
          }
        }
    }
}

/*==================================================================*/
/* Liquid ripple distortion                                         */
/*==================================================================*/

void
lw_postfx_liquid_ripple (Uint32 * pixels, int w, int h, int pitch,
                          float time, float amplitude)
{
  static Uint32 *temp_buf = NULL;
  static int temp_size = 0;
  int x, y;

  if (w * h > temp_size)
    {
      if (temp_buf)
        free (temp_buf);
      temp_buf = (Uint32 *) malloc (w * h * sizeof (Uint32));
      temp_size = w * h;
    }
  if (!temp_buf)
    return;

  /* Copy current frame */
  for (y = 0; y < h; ++y)
    memcpy (temp_buf + y * w, pixels + y * pitch, w * sizeof (Uint32));

  /* Apply displacement - two overlapping sine waves */
  for (y = 2; y < h - 2; ++y)
    {
      for (x = 2; x < w - 2; ++x)
        {
          /* Two waves at different frequencies and directions */
          float dx =
            amplitude * sinf (y * 0.03f + time * 1.5f) *
            sinf (x * 0.02f + time * 0.7f);
          float dy =
            amplitude * sinf (x * 0.025f + time * 1.1f) *
            cosf (y * 0.015f + time * 0.9f);

          int sx = x + (int) dx;
          int sy = y + (int) dy;

          if (sx >= 0 && sx < w && sy >= 0 && sy < h)
            pixels[y * pitch + x] = temp_buf[sy * w + sx];
        }
    }
}

/*==================================================================*/
/* Masked liquid ripple - only affects army pixels                  */
/*==================================================================*/

void
lw_postfx_liquid_ripple_masked (Uint32 * pixels, int w, int h, int pitch,
                                 float time, float amplitude,
                                 unsigned char *index_map,
                                 int index_pitch, int min_index)
{
  static Uint32 *temp_buf = NULL;
  static int temp_size = 0;
  int x, y;

  if (w * h > temp_size)
    {
      if (temp_buf)
        free (temp_buf);
      temp_buf = (Uint32 *) malloc (w * h * sizeof (Uint32));
      temp_size = w * h;
    }
  if (!temp_buf || !index_map)
    return;

  /* Copy current frame */
  for (y = 0; y < h; ++y)
    memcpy (temp_buf + y * w, pixels + y * pitch, w * sizeof (Uint32));

  /* Apply displacement only where palette index indicates army pixels */
  for (y = 2; y < h - 2; ++y)
    {
      unsigned char *idx_row = index_map + y * index_pitch;

      for (x = 2; x < w - 2; ++x)
        {
          /* Skip non-army pixels */
          if (idx_row[x] < min_index)
            continue;

          /* Two overlapping sine waves for organic fluid look */
          float dx =
            amplitude * sinf (y * 0.04f + time * 2.0f) *
            sinf (x * 0.03f + time * 0.8f);
          float dy =
            amplitude * sinf (x * 0.035f + time * 1.3f) *
            cosf (y * 0.02f + time * 1.1f);

          int sx = x + (int) dx;
          int sy = y + (int) dy;

          if (sx >= 0 && sx < w && sy >= 0 && sy < h)
            pixels[y * pitch + x] = temp_buf[sy * w + sx];
        }
    }
}
