/********************************************************************/
/*                                                                  */
/*            L   I  QQ  U U I DD    W   W  A  RR    555            */
/*            L   I Q  Q U U I D D   W   W A A R R   5              */
/*            L   I Q  Q U U I D D   W W W AAA RR    55             */
/*            L   I Q Q  U U I D D   WW WW A A R R     5            */
/*            LLL I  Q Q  U  I DD    W   W A A R R   55             */
/*                                                                  */
/*                             b                                    */
/*                             bb  y y                              */
/*                             b b yyy                              */
/*                             bb    y                              */
/*                                 yy                               */
/*                                                                  */
/*                     U U       FFF  O   O  TTT                    */
/*                     U U       F   O O O O  T                     */
/*                     U U TIRET FF  O O O O  T                     */
/*                     U U       F   O O O O  T                     */
/*                      U        F    O   O   T                     */
/*                                                                  */
/********************************************************************/

/*****************************************************************************/
/* Liquid War is a multiplayer wargame                                       */
/* Copyright (C) 1998-2025 Christian Mauduit                                 */
/*                                                                           */
/* This program is free software; you can redistribute it and/or modify      */
/* it under the terms of the GNU General Public License as published by      */
/* the Free Software Foundation; either version 2 of the License, or         */
/* (at your option) any later version.                                       */
/*                                                                           */
/* This program is distributed in the hope that it will be useful,           */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             */
/* GNU General Public License for more details.                              */
/*                                                                           */
/* You should have received a copy of the GNU General Public License         */
/* along with this program; if not, write to the Free Software               */
/* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */
/*                                                                           */
/* Liquid War homepage : https://ufoot.org/liquidwar/v5                   */
/* Contact author      : ufoot@ufoot.org                                     */
/*****************************************************************************/

/********************************************************************/
/* nom           : disp.c                                           */
/* contenu       : affichage de l'aire de jeu                       */
/* date de modif : 3 mai 98                                         */
/********************************************************************/

/*==================================================================*/
/* includes                                                         */
/*==================================================================*/

#include "area.h"
#include "config.h"
#include "disp.h"
#include "grad.h"
#include "palette.h"
#include "viewport.h"
#include "wave.h"
#include "distor.h"

/*==================================================================*/
/* variables globales                                               */
/*==================================================================*/

/*==================================================================*/
/* fonctions                                                        */
/*==================================================================*/

/*------------------------------------------------------------------*/
/*
 * Apply edge smoothing to army boundaries in the game area.
 * For pixels at team boundaries, blend with neighbors to reduce
 * the jagged appearance. Works on the 8-bit indexed bitmap by
 * choosing the most common neighbor color at edges.
 */
static void __attribute__((unused))
smooth_army_edges (void)
{
  int x, y;
  static unsigned char *smooth_buf = NULL;
  static int smooth_buf_size = 0;
  int area_size = CURRENT_AREA_W * CURRENT_AREA_H;
  unsigned char *src;

  if (!CURRENT_AREA_DISP || !CURRENT_AREA_DISP->sdl_surface)
    return;

  /* Allocate smoothing buffer once */
  if (smooth_buf_size < area_size)
    {
      if (smooth_buf)
        free (smooth_buf);
      smooth_buf = (unsigned char *) malloc (area_size);
      smooth_buf_size = area_size;
    }
  if (!smooth_buf)
    return;

  src = (unsigned char *) CURRENT_AREA_DISP->sdl_surface->pixels;

  /* Copy current state */
  memcpy (smooth_buf, src, area_size);

  /* Smooth pass: at boundary pixels, pick the majority neighbor color.
   * This softens jagged edges between different teams. */
  for (y = 1; y < CURRENT_AREA_H - 1; ++y)
    {
      int row = y * CURRENT_AREA_DISP->sdl_surface->pitch;
      int row_up = (y - 1) * CURRENT_AREA_DISP->sdl_surface->pitch;
      int row_dn = (y + 1) * CURRENT_AREA_DISP->sdl_surface->pitch;

      for (x = 1; x < CURRENT_AREA_W - 1; ++x)
        {
          unsigned char c = src[row + x];
          unsigned char up = src[row_up + x];
          unsigned char dn = src[row_dn + x];
          unsigned char lt = src[row + x - 1];
          unsigned char rt = src[row + x + 1];

          /* Only smooth if this pixel differs from at least 2 neighbors
           * (indicates a boundary) */
          int diff = (c != up) + (c != dn) + (c != lt) + (c != rt);
          if (diff >= 2)
            {
              /* Pick the most common neighbor */
              unsigned char counts[4] = { 0 };
              unsigned char vals[4] = { up, dn, lt, rt };
              int best = 0, bi, bj;
              for (bi = 0; bi < 4; ++bi)
                {
                  int cnt = 0;
                  for (bj = 0; bj < 4; ++bj)
                    if (vals[bi] == vals[bj])
                      cnt++;
                  if (cnt > counts[best])
                    {
                      counts[best] = cnt;
                      best = bi;
                    }
                }
              smooth_buf[row + x] = vals[best];
            }
        }
    }

  /* Write back smoothed pixels */
  memcpy (src, smooth_buf, area_size);
}

static void
disp_stretch_area (void)
{
  stretch_blit (CURRENT_AREA_DISP, NEXT_SCREEN, 0, 0,
                CURRENT_AREA_W, CURRENT_AREA_H,
                0, 0, NEXT_SCREEN->w, NEXT_SCREEN->h);
}

/*------------------------------------------------------------------*/
void
display_area (void)
{
  if ((CONFIG_WAVE_AMPLI[0]
       || CONFIG_WAVE_AMPLI[1]
       || CONFIG_WAVE_AMPLI[2] || CONFIG_WAVE_AMPLI[3]) && CONFIG_WAVE_ON)
    disp_distorted_area ();
  else
    disp_stretch_area ();
}

/*------------------------------------------------------------------*/
void
display_gradient (int i)
{
  BITMAP *bmp;

  bmp = create_gradient_bitmap (i);
  if (bmp)
    {
      stretch_blit (bmp, NEXT_SCREEN, 0, 0, bmp->w, bmp->h,
                    0, 0, NEXT_SCREEN->w, NEXT_SCREEN->h);
      destroy_bitmap (bmp);
    }
}

/*------------------------------------------------------------------*/
void
display_mesh (int i)
{
  BITMAP *bmp;

  bmp = create_mesh_bitmap (i);
  if (bmp)
    {
      stretch_blit (bmp, NEXT_SCREEN, 0, 0, bmp->w, bmp->h,
                    0, 0, NEXT_SCREEN->w, NEXT_SCREEN->h);
      destroy_bitmap (bmp);
    }
}
