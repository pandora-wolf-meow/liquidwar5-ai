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
/* nom           : disk.c                                           */
/* contenu       : lecture des donnees du fichier .dat              */
/* date de modif : 3 mai 98                                         */
/********************************************************************/

/*==================================================================*/
/* includes                                                         */
/*==================================================================*/

#include <string.h>
#include "sdl_compat.h"

#include "alleg2.h"
#include "init.h"
#include "disk.h"
#include "disk_sdl.h"
#include "log.h"
#include "map.h"
#include "palette.h"
#include "startup.h"
#include "texture.h"
#include "macro.h"

/*==================================================================*/
/* defines                                                          */
/*==================================================================*/

#define SAMPLE_SFX_NUMBER  6

/*==================================================================*/
/* variables globales                                               */
/*==================================================================*/

int SAMPLE_WATER_NUMBER = 0;
int RAW_TEXTURE_NUMBER = 0;
int RAW_MAPTEX_NUMBER = 0;
int RAW_MAP_NUMBER = 0;
int MIDI_MUSIC_NUMBER = 0;

int LOADED_BACK = 0;
int LOADED_TEXTURE = 0;
int LOADED_MAPTEX = 0;
int LOADED_SFX = 0;
int LOADED_WATER = 0;
int LOADED_MUSIC = 0;

SAMPLE *SAMPLE_SFX_TIME = NULL;
SAMPLE *SAMPLE_SFX_WIN = NULL;
SAMPLE *SAMPLE_SFX_GO = NULL;
SAMPLE *SAMPLE_SFX_CLICK = NULL;
SAMPLE *SAMPLE_SFX_LOOSE = NULL;
SAMPLE *SAMPLE_SFX_CONNECT = NULL;

SAMPLE *SAMPLE_WATER[SAMPLE_WATER_MAX_NUMBER];
void *RAW_MAP[RAW_MAP_MAX_NUMBER];
void *RAW_MAP_ORDERED[RAW_MAP_MAX_NUMBER];
void *RAW_TEXTURE[RAW_TEXTURE_MAX_NUMBER];
void *RAW_MAPTEX[RAW_TEXTURE_MAX_NUMBER];
MIDI *MIDI_MUSIC[MIDI_MUSIC_MAX_NUMBER];

BITMAP *BACK_IMAGE = NULL;

FONT *BIG_FONT = NULL;
FONT *SMALL_FONT = NULL;
BITMAP *BIG_MOUSE_CURSOR = NULL;
BITMAP *SMALL_MOUSE_CURSOR = NULL;
BITMAP *INVISIBLE_MOUSE_CURSOR = NULL;

/* FONT_PALETTE and BACK_PALETTE only used by old .dat loading path */

static int CUSTOM_TEXTURE_OK = 0;
static int CUSTOM_MAP_OK = 0;
static int CUSTOM_MUSIC_OK = 0;

/*------------------------------------------------------------------*/
/* Old .dat file reading functions - disabled, using direct loading  */
/*------------------------------------------------------------------*/
#if 0

/*------------------------------------------------------------------*/
static void
lock_sound (SAMPLE * smp)
{
  LOCK_VARIABLE (*smp);
#ifdef DOS
  _go32_dpmi_lock_data (smp->data, (smp->bits / 8) * smp->len);
#else
  LW_MACRO_NOP (smp);
#endif
}

/*------------------------------------------------------------------*/
static void
read_sfx_dat (DATAFILE * df)
{
  SAMPLE *list[SAMPLE_SFX_NUMBER];
  int i;

  /*
   * First, we associate the _first_ sound of the sub datafile
   * to all sounds. This will operate as default value which
   * will prevent the game from segfaulting if we use it with
   * an outdated or too recent datafile
   */
  for (i = 0; i < SAMPLE_SFX_NUMBER && df[i].type != DAT_END; ++i)
    {
      list[i] = df[0].dat;
    }

  /*
   * Now we associate the real sounds, provided that they exist...
   */
  for (i = 0; i < 6 && df[i].type != DAT_END; ++i)
    {
      list[i] = df[i].dat;
      lock_sound (list[i]);
    }

  SAMPLE_SFX_TIME = list[0];
  SAMPLE_SFX_WIN = list[1];
  SAMPLE_SFX_CONNECT = list[2];
  SAMPLE_SFX_GO = list[3];
  SAMPLE_SFX_CLICK = list[4];
  SAMPLE_SFX_LOOSE = list[5];
}

/*------------------------------------------------------------------*/
static void
read_water_dat (DATAFILE * df)
{
  int i;

  for (i = 0; i < SAMPLE_WATER_DAT_NUMBER && df[i].type != DAT_END; ++i)
    {
      SAMPLE_WATER[i] = df[i].dat;
      lock_sound (SAMPLE_WATER[i]);
      SAMPLE_WATER_NUMBER++;
    }
}

/*------------------------------------------------------------------*/
/* chargement des autres donnees                                    */
/*------------------------------------------------------------------*/

/*------------------------------------------------------------------*/
static void
read_texture_dat (DATAFILE * df)
{
  int i;

  RAW_TEXTURE_NUMBER = 0;
  for (i = 0; i < RAW_TEXTURE_DAT_NUMBER && df[i].type != DAT_END; ++i)
    {
      RAW_TEXTURE[i] = df[i].dat;
      RAW_TEXTURE_NUMBER++;
    }
}

/*------------------------------------------------------------------*/
static void
read_maptex_dat (DATAFILE * df)
{
  int i;

  RAW_MAPTEX_NUMBER = 0;
  for (i = 0; i < RAW_TEXTURE_DAT_NUMBER && df[i].type != DAT_END; ++i)
    {
      RAW_MAPTEX[i] = df[i].dat;
      RAW_MAPTEX_NUMBER++;
    }
}

/*------------------------------------------------------------------*/
static void
read_map_dat (DATAFILE * df)
{
  int i;

  RAW_MAP_NUMBER = 0;
  for (i = 0; i < RAW_MAP_DAT_NUMBER && df[i].type != DAT_END; ++i)
    {
      RAW_MAP[i] = df[i].dat;
      RAW_MAP_NUMBER++;
    }
}

/*------------------------------------------------------------------*/
static void
read_back_dat (DATAFILE * df)
{
  int i, x, y;

  BACK_PALETTE = df[1].dat;
  BACK_IMAGE = df[0].dat;

  /*
   * strange, with Allegro 4.0, the liquidwarcol utility
   * and the datafile compiler do not work so well together,
   * and so the palette stored in the datafile always
   * start at color 0, which explains the "18 shift"
   */

  for (i = 0; i <= 45; ++i)
    GLOBAL_PALETTE[i + 18] = BACK_PALETTE[i];

  for (x = 0; x < BACK_IMAGE->w; ++x)
    for (y = 0; y < BACK_IMAGE->w; ++y)
      {
        putpixel (BACK_IMAGE, x, y, getpixel (BACK_IMAGE, x, y) + 18);
      }
}

/*------------------------------------------------------------------*/
static void
create_default_back (void)
{
  static RGB back_coul;

  memset (&back_coul, 0, sizeof (RGB));
  back_coul.r = 1;
  back_coul.g = 1;
  back_coul.b = 8;

  BACK_IMAGE = my_create_bitmap (1, 1);
  putpixel (BACK_IMAGE, 0, 0, 18);
  GLOBAL_PALETTE[18] = back_coul;
}

/*------------------------------------------------------------------*/
static void
read_font_dat (DATAFILE * df)
{
  int i;

  FONT_PALETTE = df[4].dat;
  SMALL_FONT = df[0].dat;
  BIG_FONT = df[1].dat;
  SMALL_MOUSE_CURSOR = df[2].dat;
  BIG_MOUSE_CURSOR = df[3].dat;
  INVISIBLE_MOUSE_CURSOR = df[5].dat;

  for (i = 1; i <= 17; ++i)
    GLOBAL_PALETTE[i] = FONT_PALETTE[i];
}

/*------------------------------------------------------------------*/
static void
read_music_dat (DATAFILE * df)
{
  int i;

  MIDI_MUSIC_NUMBER = 0;
  for (i = 0; i < MIDI_MUSIC_DAT_NUMBER && df[i].type != DAT_END; ++i)
    {
      MIDI_MUSIC[i] = df[i].dat;
      MIDI_MUSIC_NUMBER++;
    }
}

#endif /* old .dat reading functions */

/*------------------------------------------------------------------*/
static void
create_default_back (void)
{
  static RGB back_coul;

  memset (&back_coul, 0, sizeof (RGB));
  back_coul.r = 1;
  back_coul.g = 1;
  back_coul.b = 8;

  BACK_IMAGE = my_create_bitmap (1, 1);
  putpixel (BACK_IMAGE, 0, 0, 18);
  GLOBAL_PALETTE[18] = back_coul;
}

/*------------------------------------------------------------------*/
int
load_dat (void)
{
  int result = 1;
  int n;

  log_print_str ("Loading data from \"");
  log_print_str (STARTUP_DAT_PATH);
  log_print_str ("\"");

  /* Set up the data directory from the .dat path */
  lw_disk_sdl_set_data_dir (STARTUP_DAT_PATH);
  display_success (1);

  /* Load fonts and cursors */
  {
    log_print_str ("Loading fonts");
    log_flush ();
    SMALL_MOUSE_CURSOR = lw_disk_sdl_load_font_bitmap ("mouse20.pcx");
    BIG_MOUSE_CURSOR = lw_disk_sdl_load_font_bitmap ("mouse40.pcx");
    INVISIBLE_MOUSE_CURSOR = lw_disk_sdl_load_font_bitmap ("void1.pcx");
    SMALL_FONT = lw_sdl_load_font (10);
    BIG_FONT = lw_sdl_load_font (16);
    if (!SMALL_FONT)
      SMALL_FONT = lw_sdl_load_font (8);
    if (!BIG_FONT)
      BIG_FONT = lw_sdl_load_font (12);

    /* Set up default menu palette entries since we don't load from .dat */
    {
      int i;
      /* Entry 16 = MENU_BG (dark blue background) */
      GLOBAL_PALETTE[16].r = 0;
      GLOBAL_PALETTE[16].g = 0;
      GLOBAL_PALETTE[16].b = 10;
      /* Entry 17 = MENU_FG (white foreground) */
      GLOBAL_PALETTE[17].r = 63;
      GLOBAL_PALETTE[17].g = 63;
      GLOBAL_PALETTE[17].b = 63;
      /* Entries 1-15: grayscale ramp for general UI */
      for (i = 1; i <= 15; ++i)
        {
          GLOBAL_PALETTE[i].r = i * 4;
          GLOBAL_PALETTE[i].g = i * 4;
          GLOBAL_PALETTE[i].b = i * 4;
        }
      /* Entries 18-63 will be set from background image palette */
    }

    display_success (BIG_FONT != NULL || SMALL_FONT != NULL);
  }

  /* Load maps */
  {
    log_print_str ("Loading maps");
    log_flush ();
    n = lw_disk_sdl_load_maps (RAW_MAP, RAW_MAP_MAX_NUMBER);
    RAW_MAP_NUMBER = n;
    display_success (n > 0);
    if (n <= 0)
      result = 0;
  }

  /* Load background */
  if (STARTUP_BACK_STATE)
    {
      PALETTE back_pal;
      log_print_str ("Loading background bitmap");
      log_flush ();
      memset (back_pal, 0, sizeof (back_pal));
      BACK_IMAGE = lw_disk_sdl_load_back ();
      if (BACK_IMAGE)
        {
          /* Extract full palette from the background image's SDL surface.
           * The background uses all 256 palette entries. We copy them
           * into GLOBAL_PALETTE so the menu displays correctly.
           * During gameplay, set_playing_teams_palette will override
           * entries 128-255 for team colors. */
          if (BACK_IMAGE->sdl_surface && BACK_IMAGE->sdl_surface->format->palette)
            {
              int pi;
              SDL_Palette *sp = BACK_IMAGE->sdl_surface->format->palette;
              for (pi = 0; pi < sp->ncolors && pi < 256; ++pi)
                {
                  GLOBAL_PALETTE[pi].r = sp->colors[pi].r / 4;
                  GLOBAL_PALETTE[pi].g = sp->colors[pi].g / 4;
                  GLOBAL_PALETTE[pi].b = sp->colors[pi].b / 4;
                }
            }
          /* Remap background pixels away from palette entries 0-17
           * (reserved for menu UI). Find the closest color in entries
           * 18-255 for each pixel that uses 0-17. */
          {
            int bx, by, pi;
            unsigned char remap[18];
            for (pi = 0; pi < 18; ++pi)
              {
                /* Find closest color in entries 18-255 */
                int best = 18, best_dist = 0x7FFFFFFF;
                int pr = GLOBAL_PALETTE[pi].r;
                int pg = GLOBAL_PALETTE[pi].g;
                int pb = GLOBAL_PALETTE[pi].b;
                int ci;
                for (ci = 18; ci < 256; ++ci)
                  {
                    int dr = GLOBAL_PALETTE[ci].r - pr;
                    int dg = GLOBAL_PALETTE[ci].g - pg;
                    int db = GLOBAL_PALETTE[ci].b - pb;
                    int dist = dr * dr + dg * dg + db * db;
                    if (dist < best_dist)
                      {
                        best_dist = dist;
                        best = ci;
                      }
                  }
                remap[pi] = best;
              }
            for (by = 0; by < BACK_IMAGE->h; ++by)
              for (bx = 0; bx < BACK_IMAGE->w; ++bx)
                {
                  int px = getpixel (BACK_IMAGE, bx, by);
                  if (px < 18)
                    putpixel (BACK_IMAGE, bx, by, remap[px]);
                }
          }
          /* Now set menu palette entries */
          GLOBAL_PALETTE[MENU_BG].r = 0;
          GLOBAL_PALETTE[MENU_BG].g = 0;
          GLOBAL_PALETTE[MENU_BG].b = 10;
          GLOBAL_PALETTE[MENU_FG].r = 63;
          GLOBAL_PALETTE[MENU_FG].g = 63;
          GLOBAL_PALETTE[MENU_FG].b = 63;
          /* Grayscale ramp for UI elements */
          {
            int gi;
            for (gi = 1; gi <= 15; ++gi)
              {
                GLOBAL_PALETTE[gi].r = gi * 4;
                GLOBAL_PALETTE[gi].g = gi * 4;
                GLOBAL_PALETTE[gi].b = gi * 4;
              }
          }
          LOADED_BACK = 1;
        }
      else
        create_default_back ();
      display_success (BACK_IMAGE != NULL);
    }
  else
    create_default_back ();

  /* Load sound effects */
  if (STARTUP_SFX_STATE)
    {
      SAMPLE *sfx_samples[SAMPLE_SFX_NUMBER];
      log_print_str ("Loading sound fx");
      log_flush ();
      n = lw_disk_sdl_load_sfx (sfx_samples, SAMPLE_SFX_NUMBER, "sfx");
      if (n > 0)
        {
          SAMPLE_SFX_TIME = sfx_samples[0 % n];
          SAMPLE_SFX_WIN = sfx_samples[1 % n];
          SAMPLE_SFX_CONNECT = sfx_samples[2 % n];
          SAMPLE_SFX_GO = sfx_samples[3 % n];
          SAMPLE_SFX_CLICK = sfx_samples[4 % n];
          SAMPLE_SFX_LOOSE = sfx_samples[5 % n];
          LOADED_SFX = 1;
        }
      display_success (n > 0);
    }

  /* Load textures from PCX files */
  if (STARTUP_TEXTURE_STATE)
    {
      log_print_str ("Loading textures");
      log_flush ();
      n =
        lw_disk_sdl_load_textures (RAW_TEXTURE, RAW_TEXTURE_MAX_NUMBER,
                                    "texture");
      RAW_TEXTURE_NUMBER = n;
      if (n > 0)
        LOADED_TEXTURE = 1;
      display_success (n > 0);

      log_print_str ("Loading map textures");
      log_flush ();
      n =
        lw_disk_sdl_load_textures (RAW_MAPTEX, RAW_TEXTURE_MAX_NUMBER,
                                    "maptex");
      RAW_MAPTEX_NUMBER = n;
      if (n > 0)
        LOADED_MAPTEX = 1;
      display_success (n > 0);
    }

  /* Load water sounds */
  if (STARTUP_WATER_STATE)
    {
      log_print_str ("Loading water sounds");
      log_flush ();
      n =
        lw_disk_sdl_load_sfx (SAMPLE_WATER, SAMPLE_WATER_MAX_NUMBER,
                               "water");
      SAMPLE_WATER_NUMBER = n;
      if (n > 0)
        LOADED_WATER = 1;
      display_success (n > 0);
    }

  /* Load music */
  if (STARTUP_MUSIC_STATE)
    {
      log_print_str ("Loading midi music");
      log_flush ();
      n = lw_disk_sdl_load_music (MIDI_MUSIC, MIDI_MUSIC_MAX_NUMBER);
      MIDI_MUSIC_NUMBER = n;
      if (n > 0)
        LOADED_MUSIC = 1;
      display_success (n > 0);
    }

  return result;
}

/*------------------------------------------------------------------*/
static int
load_custom_texture_callback (const char *file, int mode, void *unused)
{
  void *pointeur;

  LW_MACRO_NOP (mode);
  LW_MACRO_NOP (unused);

  if ((pointeur = lw_texture_archive_raw (file)) != NULL)
    {
      RAW_TEXTURE[RAW_TEXTURE_NUMBER++] = pointeur;
      log_print_str ("+");
      CUSTOM_TEXTURE_OK = 1;
    }
  else
    {
      log_print_str ("-");
    }
  log_flush ();

  return 0;
}

/*------------------------------------------------------------------*/
static int
load_custom_texture (void)
{
  int result = 1;
  char buf[512];

  LW_MACRO_SPRINTF1 (buf, "%s\\*.*", STARTUP_TEX_PATH);

  fix_filename_case (buf);
  fix_filename_slashes (buf);

  CUSTOM_TEXTURE_OK = 0;
  for_each_file_ex (buf, 0, FA_DIREC, load_custom_texture_callback, NULL);
  result = CUSTOM_TEXTURE_OK;

  return result;
}

/*------------------------------------------------------------------*/
static int
load_custom_map_callback (const char *file, int mode, void *unused)
{
  void *pointeur;

  LW_MACRO_NOP (mode);
  LW_MACRO_NOP (unused);

  if ((pointeur = lw_map_archive_raw (file)) != NULL)
    {
      RAW_MAP[RAW_MAP_NUMBER++] = pointeur;
      log_print_str ("+");
      CUSTOM_MAP_OK = 1;
    }
  else
    {
      log_print_str ("-");
    }

  return 0;
}

/*------------------------------------------------------------------*/
static int
load_custom_map (void)
{
  int result = 1;
  char buf[512];

  LW_MACRO_SPRINTF1 (buf, "%s\\*.*", STARTUP_MAP_PATH);

  fix_filename_case (buf);
  fix_filename_slashes (buf);

  CUSTOM_MAP_OK = 0;
  for_each_file_ex (buf, 0, FA_DIREC, load_custom_map_callback, NULL);
  result = CUSTOM_MAP_OK;

  return result;
}

/*------------------------------------------------------------------*/
static int
load_custom_music_callback (const char *file, int mode, void *unused)
{
  void *pointeur;

  LW_MACRO_NOP (mode);
  LW_MACRO_NOP (unused);

  if ((pointeur = load_midi (file)) != NULL)
    {
      MIDI_MUSIC[MIDI_MUSIC_NUMBER++] = pointeur;
      log_print_str ("+");
      CUSTOM_MUSIC_OK = 1;
    }
  else
    {
      log_print_str ("-");
    }
  log_flush ();

  return 0;
}

/*------------------------------------------------------------------*/
static int
load_custom_music (void)
{
  int result = 1;
  char buf[512];

  LW_MACRO_SPRINTF1 (buf, "%s\\*.*", STARTUP_MID_PATH);

  fix_filename_case (buf);
  fix_filename_slashes (buf);

  CUSTOM_MUSIC_OK = 0;
  for_each_file_ex (buf, 0, FA_DIREC, load_custom_music_callback, NULL);
  result = CUSTOM_MUSIC_OK;

  return result;
}

/*------------------------------------------------------------------*/
int
load_custom (void)
{
  int success, result = 1;

  if (STARTUP_TEXTURE_STATE && STARTUP_CUSTOM_STATE)
    {
      log_print_str ("Loading custom textures from \"");
      log_print_str (STARTUP_TEX_PATH);
      log_print_str ("\" ");
      log_flush ();
      success = load_custom_texture ();
      if (!success)
        result &= !STARTUP_CHECK;
      display_success (success);
    }

  if (STARTUP_CUSTOM_STATE)
    {
      log_print_str ("Loading custom maps from \"");
      log_print_str (STARTUP_MAP_PATH);
      log_print_str ("\" ");
      log_flush ();
      success = load_custom_map ();
      if (!success)
        result &= !STARTUP_CHECK;
      display_success (success);
    }

  if (STARTUP_CUSTOM_STATE && STARTUP_MUSIC_STATE)
    {
      log_print_str ("Loading custom musics from \"");
      log_print_str (STARTUP_MID_PATH);
      log_print_str ("\" ");
      log_flush ();
      success = load_custom_music ();
      if (!success)
        result &= !STARTUP_CHECK;
      display_success (success);
    }

  return result;
}

/*------------------------------------------------------------------*/
void
order_map (void)
{
  int incorrect_order = 1;
  int i;


  char name1[LW_MAP_READABLE_NAME_SIZE + 1];
  char name2[LW_MAP_READABLE_NAME_SIZE + 1];
  void *temp;

  for (i = 0; i < RAW_MAP_NUMBER; ++i)
    {
      RAW_MAP_ORDERED[i] = RAW_MAP[i];
    }

  while (incorrect_order)
    {
      incorrect_order = 0;

      for (i = 0; i < RAW_MAP_NUMBER - 1; ++i)
        {
          LW_MACRO_STRCPY (name1, lw_map_get_readable_name (i, 0, 0));
          LW_MACRO_STRCPY (name2, lw_map_get_readable_name (i + 1, 0, 0));
          if (strcmp (name1, name2) > 0)
            {
              incorrect_order = 1;

              temp = RAW_MAP_ORDERED[i];
              RAW_MAP_ORDERED[i] = RAW_MAP_ORDERED[i + 1];
              RAW_MAP_ORDERED[i + 1] = temp;
            }
        }
    }
}
