/********************************************************************/
/* SDL2 compatibility layer implementation for Liquid War 5         */
/* Implements Allegro 4 API functions using SDL2                    */
/********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "sdl_compat.h"

/*==================================================================*/
/* Global state                                                     */
/*==================================================================*/

SDL_Window *lw_sdl_window = NULL;
SDL_Renderer *lw_sdl_renderer = NULL;

BITMAP *screen = NULL;
FONT *font = NULL;
int SCREEN_W = 0, SCREEN_H = 0, VIRTUAL_H = 0;

char *allegro_id = "SDL2 compatibility layer for Liquid War 5";
char allegro_error[256] = "";

PALETTE black_palette;

volatile char key[KEY_MAX];
volatile int mouse_x = 0, mouse_y = 0, mouse_b = 0;

int num_joysticks = 0;
JOYSTICK_INFO joy[8];

int midi_pos = -1;

static void (*lw_close_button_callback) (void) = NULL;

/* Timer callback storage */
#define MAX_TIMER_CALLBACKS 8
static struct
{
  void (*handler) (void);
  SDL_TimerID timer_id;
  int interval_ms;
} lw_timer_callbacks[MAX_TIMER_CALLBACKS];
static int lw_timer_count = 0;

/* Driver info instances */
static LW_DRIVER_INFO gfx_driver_info = { "SDL2", 1 };
static LW_DRIVER_INFO timer_driver_info = { "SDL2 Timer", 1 };
static LW_DRIVER_INFO keyboard_driver_info = { "SDL2 Keyboard", 1 };
static LW_DRIVER_INFO mouse_driver_info = { "SDL2 Mouse", 1 };
static LW_DRIVER_INFO digi_driver_info = { "SDL2_mixer", 1 };
static LW_DRIVER_INFO midi_driver_info = { "SDL2_mixer MIDI", 1 };
static LW_DRIVER_INFO joystick_driver_info = { "SDL2 Joystick", 1 };

LW_DRIVER_INFO *gfx_driver = &gfx_driver_info;
LW_DRIVER_INFO *timer_driver = &timer_driver_info;
LW_DRIVER_INFO *keyboard_driver = &keyboard_driver_info;
LW_DRIVER_INFO *mouse_driver = &mouse_driver_info;
LW_DRIVER_INFO *digi_driver = &digi_driver_info;
LW_DRIVER_INFO *midi_driver = &midi_driver_info;
LW_DRIVER_INFO *joystick_driver = &joystick_driver_info;

/* SDL2 audio initialized flag */
static int lw_audio_initialized = 0;

/* Screen presentation state */
static SDL_Texture *lw_screen_texture = NULL;
static int lw_screen_tex_w = 0, lw_screen_tex_h = 0;
static SDL_Surface *lw_convert_surface = NULL;

/* Keyboard modifier and GUI state */
volatile int key_shifts = 0;
int gui_mg_color = 8;
int gui_fg_color = 0;
int gui_bg_color = 255;

/* Forward declarations */
static void lw_bitmap_setup_lines (BITMAP * bmp);

/*==================================================================*/
/* File system functions                                            */
/*==================================================================*/

int
exists (const char *filename)
{
  FILE *fp = fopen (filename, "r");
  if (fp)
    {
      fclose (fp);
      return 1;
    }
  return 0;
}

int
delete_file (const char *filename)
{
  return remove (filename);
}

char *
fix_filename_case (char *path)
{
  /* No-op on modern filesystems */
  return path;
}

char *
fix_filename_slashes (char *path)
{
#ifdef WIN32
  char *p;
  for (p = path; *p; ++p)
    if (*p == '/')
      *p = '\\';
#endif
  return path;
}

int
for_each_file_ex (const char *pattern, int attrib, int not_attrib,
                  int (*callback) (const char *filename, int attrib,
                                   void *param), void *param)
{
  /* TODO: implement directory scanning */
  (void) pattern;
  (void) attrib;
  (void) not_attrib;
  (void) callback;
  (void) param;
  return 0;
}

BITMAP *
load_bitmap (const char *filename, PALETTE pal)
{
  SDL_Surface *surface;
  BITMAP *bmp;

  surface = IMG_Load (filename);
  if (!surface)
    return NULL;

  /* Extract palette from loaded surface if available */
  if (pal && surface->format->palette)
    {
      int i;
      int ncolors = surface->format->palette->ncolors;
      if (ncolors > 256)
        ncolors = 256;
      memset (pal, 0, sizeof (PALETTE));
      for (i = 0; i < ncolors; ++i)
        {
          /* Allegro palette uses 0-63 range */
          pal[i].r = surface->format->palette->colors[i].r / 4;
          pal[i].g = surface->format->palette->colors[i].g / 4;
          pal[i].b = surface->format->palette->colors[i].b / 4;
        }
    }

  bmp = (BITMAP *) calloc (1, sizeof (BITMAP));
  if (!bmp)
    {
      SDL_FreeSurface (surface);
      return NULL;
    }

  bmp->w = surface->w;
  bmp->h = surface->h;
  bmp->clip = 1;
  bmp->cl = 0;
  bmp->ct = 0;
  bmp->cr = surface->w;
  bmp->cb = surface->h;
  bmp->is_sub_bitmap = 0;
  bmp->parent = NULL;
  bmp->sdl_surface = surface;

  lw_bitmap_setup_lines (bmp);

  return bmp;
}

MIDI *
load_midi (const char *filename)
{
  if (!filename)
    return NULL;
  return (MIDI *) Mix_LoadMUS (filename);
}

SAMPLE *
load_sample (const char *filename)
{
  if (!filename)
    return NULL;
  return (SAMPLE *) Mix_LoadWAV (filename);
}

/*==================================================================*/
/* Safe string copy helper                                          */
/*==================================================================*/

static void
lw_safe_strcpy (char *dst, const char *src, size_t dstsize)
{
  if (dstsize == 0)
    return;
  size_t len = strlen (src);
  if (len >= dstsize)
    len = dstsize - 1;
  memcpy (dst, src, len);
  dst[len] = '\0';
}

/*==================================================================*/
/* Configuration file (simple INI-style key-value store)            */
/*==================================================================*/

#define LW_CONFIG_MAX_ENTRIES 512
#define LW_CONFIG_MAX_KEY 128
#define LW_CONFIG_MAX_VAL 512

static struct
{
  char section[LW_CONFIG_MAX_KEY];
  char name[LW_CONFIG_MAX_KEY];
  char value[LW_CONFIG_MAX_VAL];
} lw_config_entries[LW_CONFIG_MAX_ENTRIES];
static int lw_config_count = 0;
static char lw_config_filename[512] = "";

static int
lw_config_find (const char *section, const char *name)
{
  int i;
  for (i = 0; i < lw_config_count; ++i)
    {
      if (strcmp (lw_config_entries[i].section, section ? section : "") == 0
          && strcmp (lw_config_entries[i].name, name) == 0)
        return i;
    }
  return -1;
}

void
set_config_file (const char *filename)
{
  FILE *fp;
  char line[1024];
  char current_section[LW_CONFIG_MAX_KEY] = "";

  lw_safe_strcpy (lw_config_filename, filename, sizeof (lw_config_filename));
  lw_config_count = 0;

  fp = fopen (filename, "r");
  if (!fp)
    return;

  while (fgets (line, sizeof (line), fp))
    {
      char *p = line;
      char *eq;

      /* Strip leading whitespace */
      while (*p == ' ' || *p == '\t')
        p++;

      /* Skip comments and empty lines */
      if (*p == '#' || *p == ';' || *p == '\n' || *p == '\0')
        continue;

      /* Section header */
      if (*p == '[')
        {
          char *end = strchr (p, ']');
          if (end)
            {
              *end = '\0';
              lw_safe_strcpy (current_section, p + 1,
                       sizeof (current_section));
            }
          continue;
        }

      /* Key = value */
      eq = strchr (p, '=');
      if (eq && lw_config_count < LW_CONFIG_MAX_ENTRIES)
        {
          char *val = eq + 1;
          char *key_end = eq - 1;
          char *val_end;

          /* Trim key */
          while (key_end > p && (*key_end == ' ' || *key_end == '\t'))
            key_end--;
          *(key_end + 1) = '\0';

          /* Trim value */
          while (*val == ' ' || *val == '\t')
            val++;
          val_end = val + strlen (val) - 1;
          while (val_end > val
                 && (*val_end == '\n' || *val_end == '\r' || *val_end == ' '))
            val_end--;
          *(val_end + 1) = '\0';

          lw_safe_strcpy (lw_config_entries[lw_config_count].section,
                   current_section,
                   sizeof (lw_config_entries[0].section));
          lw_safe_strcpy (lw_config_entries[lw_config_count].name, p,
                   sizeof (lw_config_entries[0].name));
          lw_safe_strcpy (lw_config_entries[lw_config_count].value, val,
                   sizeof (lw_config_entries[0].value));
          lw_config_count++;
        }
    }

  fclose (fp);
}

static void
lw_config_save (void)
{
  FILE *fp;
  int i;
  char last_section[LW_CONFIG_MAX_KEY] = "";

  if (lw_config_filename[0] == '\0')
    return;

  fp = fopen (lw_config_filename, "w");
  if (!fp)
    return;

  for (i = 0; i < lw_config_count; ++i)
    {
      if (strcmp (lw_config_entries[i].section, last_section) != 0)
        {
          if (lw_config_entries[i].section[0])
            fprintf (fp, "[%s]\n", lw_config_entries[i].section);
          lw_safe_strcpy (last_section, lw_config_entries[i].section,
                   sizeof (last_section));
        }
      fprintf (fp, "%s = %s\n", lw_config_entries[i].name,
               lw_config_entries[i].value);
    }

  fclose (fp);
}

void
set_config_string (const char *section, const char *name, const char *val)
{
  int idx = lw_config_find (section, name);
  if (idx >= 0)
    {
      lw_safe_strcpy (lw_config_entries[idx].value, val ? val : "",
               sizeof (lw_config_entries[0].value));
    }
  else if (lw_config_count < LW_CONFIG_MAX_ENTRIES)
    {
      lw_safe_strcpy (lw_config_entries[lw_config_count].section,
               section ? section : "",
               sizeof (lw_config_entries[0].section));
      lw_safe_strcpy (lw_config_entries[lw_config_count].name, name,
               sizeof (lw_config_entries[0].name));
      lw_safe_strcpy (lw_config_entries[lw_config_count].value, val ? val : "",
               sizeof (lw_config_entries[0].value));
      lw_config_count++;
    }
  lw_config_save ();
}

void
set_config_int (const char *section, const char *name, int val)
{
  char buf[32];
  snprintf (buf, sizeof (buf), "%d", val);
  set_config_string (section, name, buf);
}

const char *
get_config_string (const char *section, const char *name, const char *def)
{
  int idx = lw_config_find (section, name);
  if (idx >= 0)
    return lw_config_entries[idx].value;
  return def;
}

int
get_config_int (const char *section, const char *name, int def)
{
  int idx = lw_config_find (section, name);
  if (idx >= 0)
    return atoi (lw_config_entries[idx].value);
  return def;
}

/*==================================================================*/
/* Internal helpers                                                 */
/*==================================================================*/

static void
lw_bitmap_setup_lines (BITMAP * bmp)
{
  int y;
  unsigned char *pixels;
  int pitch;

  if (!bmp || !bmp->sdl_surface)
    return;

  if (bmp->line)
    free (bmp->line);

  bmp->line =
    (unsigned char **) malloc (sizeof (unsigned char *) * bmp->h);
  pixels = (unsigned char *) bmp->sdl_surface->pixels;
  pitch = bmp->sdl_surface->pitch;

  for (y = 0; y < bmp->h; ++y)
    bmp->line[y] = pixels + y * pitch;

  bmp->dat = bmp->sdl_surface->pixels;
}

/*==================================================================*/
/* Initialization and shutdown                                      */
/*==================================================================*/

int
allegro_init (void)
{
  memset ((void *) key, 0, sizeof (key));
  memset (black_palette, 0, sizeof (black_palette));

  if (SDL_Init (SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) < 0)
    {
      snprintf (allegro_error, sizeof (allegro_error),
                "SDL_Init failed: %s", SDL_GetError ());
      return -1;
    }

  if (TTF_Init () < 0)
    {
      snprintf (allegro_error, sizeof (allegro_error),
                "TTF_Init failed: %s", TTF_GetError ());
      return -1;
    }

  return 0;
}

void
allegro_exit (void)
{
  int i;

  for (i = 0; i < lw_timer_count; ++i)
    {
      if (lw_timer_callbacks[i].timer_id)
        SDL_RemoveTimer (lw_timer_callbacks[i].timer_id);
    }
  lw_timer_count = 0;

  if (lw_screen_texture)
    {
      SDL_DestroyTexture (lw_screen_texture);
      lw_screen_texture = NULL;
    }
  if (lw_convert_surface)
    {
      SDL_FreeSurface (lw_convert_surface);
      lw_convert_surface = NULL;
    }

  if (screen)
    {
      destroy_bitmap (screen);
      screen = NULL;
    }

  if (lw_sdl_renderer)
    {
      SDL_DestroyRenderer (lw_sdl_renderer);
      lw_sdl_renderer = NULL;
    }

  if (lw_sdl_window)
    {
      SDL_DestroyWindow (lw_sdl_window);
      lw_sdl_window = NULL;
    }

  if (lw_audio_initialized)
    {
      Mix_CloseAudio ();
      lw_audio_initialized = 0;
    }

  TTF_Quit ();
  SDL_Quit ();
}

int
install_timer (void)
{
  /* SDL timer is already initialized in SDL_Init */
  return 0;
}

int
install_keyboard (void)
{
  /* SDL keyboard is already initialized in SDL_Init */
  return 0;
}

int
install_mouse (void)
{
  /* SDL mouse is already initialized; return number of buttons */
  return 3;
}

int
install_sound (int digi, int midi_card, const char *config)
{
  (void) config;

  if (digi == DIGI_NONE && midi_card == MIDI_NONE)
    return 0;

  if (SDL_InitSubSystem (SDL_INIT_AUDIO) < 0)
    return -1;

  if (Mix_OpenAudio (44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    return -1;

  lw_audio_initialized = 1;
  return 0;
}

void remove_keyboard (void) { }
void remove_mouse (void) { }
void remove_sound (void) { if (lw_audio_initialized) { Mix_CloseAudio (); lw_audio_initialized = 0; } }
void remove_timer (void) { }

/*==================================================================*/
/* Timer functions                                                  */
/*==================================================================*/

static Uint32
lw_sdl_timer_callback (Uint32 interval, void *param)
{
  void (*handler) (void) = (void (*)(void)) param;
  if (handler)
    handler ();
  return interval;
}

int
install_int_ex (void (*handler) (void), int speed)
{
  SDL_TimerID tid;

  if (lw_timer_count >= MAX_TIMER_CALLBACKS)
    return -1;

  if (speed <= 0)
    speed = 1;

  tid = SDL_AddTimer (speed, lw_sdl_timer_callback, (void *) handler);
  if (!tid)
    return -1;

  lw_timer_callbacks[lw_timer_count].handler = handler;
  lw_timer_callbacks[lw_timer_count].timer_id = tid;
  lw_timer_callbacks[lw_timer_count].interval_ms = speed;
  lw_timer_count++;

  return 0;
}

void
remove_int (void (*handler) (void))
{
  int i;
  for (i = 0; i < lw_timer_count; ++i)
    {
      if (lw_timer_callbacks[i].handler == handler)
        {
          SDL_RemoveTimer (lw_timer_callbacks[i].timer_id);
          lw_timer_callbacks[i] = lw_timer_callbacks[lw_timer_count - 1];
          lw_timer_count--;
          return;
        }
    }
}

/*==================================================================*/
/* Graphics mode functions                                          */
/*==================================================================*/

int
set_gfx_mode (int card, int w, int h, int v_w, int v_h)
{
  Uint32 flags;

  (void) card;
  (void) v_w;

  if (card == GFX_TEXT)
    {
      /* Text mode - destroy window if it exists */
      if (screen)
        {
          destroy_bitmap (screen);
          screen = NULL;
        }
      if (lw_sdl_renderer)
        {
          SDL_DestroyRenderer (lw_sdl_renderer);
          lw_sdl_renderer = NULL;
        }
      if (lw_sdl_window)
        {
          SDL_DestroyWindow (lw_sdl_window);
          lw_sdl_window = NULL;
        }
      SCREEN_W = 0;
      SCREEN_H = 0;
      VIRTUAL_H = 0;
      return 0;
    }

  flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
  if (card == GFX_AUTODETECT_FULLSCREEN || card == GFX_MODEX
      || card == GFX_VESA2L || card == GFX_DIRECTX || card == GFX_GP2X)
    {
      flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

  if (v_h <= 0)
    v_h = h;

  if (lw_sdl_window)
    {
      SDL_SetWindowSize (lw_sdl_window, w, h);
      if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
        SDL_SetWindowFullscreen (lw_sdl_window,
                                SDL_WINDOW_FULLSCREEN_DESKTOP);
      else
        SDL_SetWindowFullscreen (lw_sdl_window, 0);
    }
  else
    {
      lw_sdl_window = SDL_CreateWindow ("Liquid War",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        w, h, flags);
      if (!lw_sdl_window)
        {
          snprintf (allegro_error, sizeof (allegro_error),
                    "SDL_CreateWindow: %s", SDL_GetError ());
          return -1;
        }
    }

  if (!lw_sdl_renderer)
    {
      lw_sdl_renderer = SDL_CreateRenderer (lw_sdl_window, -1,
                                            SDL_RENDERER_ACCELERATED |
                                            SDL_RENDERER_PRESENTVSYNC);
      if (!lw_sdl_renderer)
        {
          lw_sdl_renderer = SDL_CreateRenderer (lw_sdl_window, -1,
                                                SDL_RENDERER_SOFTWARE);
        }
      if (!lw_sdl_renderer)
        {
          snprintf (allegro_error, sizeof (allegro_error),
                    "SDL_CreateRenderer: %s", SDL_GetError ());
          return -1;
        }
    }

  SCREEN_W = w;
  SCREEN_H = h;
  VIRTUAL_H = v_h;

  /* Set logical size so SDL maps mouse coordinates correctly on resize */
  SDL_RenderSetLogicalSize (lw_sdl_renderer, w, h);

  /* Create the screen bitmap */
  if (screen)
    destroy_bitmap (screen);
  screen = create_bitmap (w, v_h);

  return 0;
}

void
set_color_depth (int depth)
{
  /* SDL2 always uses 32-bit; this is a no-op */
  (void) depth;
}

void
set_color_conversion (int mode)
{
  /* No longer needed with SDL2 true-color */
  (void) mode;
}

void
set_palette (PALETTE pal)
{
  SDL_Color colors[256];
  int i;

  for (i = 0; i < 256; ++i)
    {
      /* Allegro palette values are 0-63, scale to 0-255 */
      colors[i].r = pal[i].r * 4;
      colors[i].g = pal[i].g * 4;
      colors[i].b = pal[i].b * 4;
      colors[i].a = 255;
    }

  /* Apply palette to the screen surface if it's 8-bit indexed */
  if (screen && screen->sdl_surface && screen->sdl_surface->format->palette)
    SDL_SetPaletteColors (screen->sdl_surface->format->palette, colors, 0,
                          256);
}

void
set_window_title (const char *title)
{
  if (lw_sdl_window)
    SDL_SetWindowTitle (lw_sdl_window, title);
}

void
set_close_button_callback (void (*proc) (void))
{
  lw_close_button_callback = proc;
}

/*==================================================================*/
/* Bitmap functions                                                 */
/*==================================================================*/

BITMAP *
create_bitmap (int w, int h)
{
  return create_bitmap_ex (8, w, h);
}

BITMAP *
create_bitmap_ex (int bpp, int w, int h)
{
  BITMAP *bmp;
  Uint32 format;

  bmp = (BITMAP *) calloc (1, sizeof (BITMAP));
  if (!bmp)
    return NULL;

  if (bpp <= 8)
    format = SDL_PIXELFORMAT_INDEX8;
  else
    format = SDL_PIXELFORMAT_ARGB8888;

  bmp->sdl_surface = SDL_CreateRGBSurfaceWithFormat (0, w, h, bpp, format);
  if (!bmp->sdl_surface)
    {
      free (bmp);
      return NULL;
    }

  /* For 8-bit surfaces, copy the palette from the screen if available */
  if (bpp <= 8 && screen && screen->sdl_surface
      && screen->sdl_surface->format->palette
      && bmp->sdl_surface->format->palette)
    {
      SDL_SetPaletteColors (bmp->sdl_surface->format->palette,
                            screen->sdl_surface->format->palette->colors,
                            0,
                            screen->sdl_surface->format->palette->ncolors);
    }

  bmp->w = w;
  bmp->h = h;
  bmp->clip = 1;
  bmp->cl = 0;
  bmp->ct = 0;
  bmp->cr = w;
  bmp->cb = h;
  bmp->is_sub_bitmap = 0;
  bmp->parent = NULL;

  lw_bitmap_setup_lines (bmp);

  return bmp;
}

BITMAP *
create_sub_bitmap (BITMAP * parent, int x, int y, int w, int h)
{
  BITMAP *bmp;
  int py;

  if (!parent)
    return NULL;

  bmp = (BITMAP *) calloc (1, sizeof (BITMAP));
  if (!bmp)
    return NULL;

  bmp->w = w;
  bmp->h = h;
  bmp->clip = 1;
  bmp->cl = 0;
  bmp->ct = 0;
  bmp->cr = w;
  bmp->cb = h;
  bmp->is_sub_bitmap = 1;
  bmp->parent = parent;
  bmp->sub_x = x;
  bmp->sub_y = y;
  bmp->sdl_surface = parent->sdl_surface;

  /* Set up line pointers into parent surface */
  bmp->line = (unsigned char **) malloc (sizeof (unsigned char *) * h);
  if (parent->sdl_surface)
    {
      int pitch = parent->sdl_surface->pitch;
      int bpp = parent->sdl_surface->format->BytesPerPixel;
      unsigned char *pixels = (unsigned char *) parent->sdl_surface->pixels;

      for (py = 0; py < h; ++py)
        bmp->line[py] = pixels + (y + py) * pitch + x * bpp;

      bmp->dat = bmp->line[0];
    }

  return bmp;
}

void
destroy_bitmap (BITMAP * bmp)
{
  if (!bmp)
    return;

  if (!bmp->is_sub_bitmap && bmp->sdl_surface)
    SDL_FreeSurface (bmp->sdl_surface);

  if (bmp->line)
    free (bmp->line);

  free (bmp);
}

void
clear_bitmap (BITMAP * bmp)
{
  if (!bmp || !bmp->sdl_surface)
    return;

  if (!bmp->is_sub_bitmap)
    {
      SDL_FillRect (bmp->sdl_surface, NULL, 0);
    }
  else
    {
      SDL_Rect r = { bmp->sub_x, bmp->sub_y, bmp->w, bmp->h };
      SDL_FillRect (bmp->parent->sdl_surface, &r, 0);
    }
}

void
clear_to_color (BITMAP * bmp, int color)
{
  if (!bmp || !bmp->sdl_surface)
    return;
  /* For 8-bit indexed surfaces, color is a palette index directly */
  if (bmp->sdl_surface->format->BitsPerPixel <= 8)
    SDL_FillRect (bmp->sdl_surface, NULL, color);
  else
    SDL_FillRect (bmp->sdl_surface, NULL,
                  SDL_MapRGBA (bmp->sdl_surface->format,
                               (color >> 16) & 0xFF,
                               (color >> 8) & 0xFF,
                               color & 0xFF, 255));
}

int
bitmap_color_depth (BITMAP * bmp)
{
  if (!bmp || !bmp->sdl_surface)
    return 8;
  return bmp->sdl_surface->format->BitsPerPixel;
}

int
is_linear_bitmap (BITMAP * bmp)
{
  (void) bmp;
  return 1;
}

int
is_memory_bitmap (BITMAP * bmp)
{
  (void) bmp;
  return 1;
}

/*==================================================================*/
/* Drawing primitives                                               */
/*==================================================================*/

void
putpixel (BITMAP * bmp, int x, int y, int color)
{
  unsigned char *pixels;
  int bpp;

  if (!bmp || !bmp->sdl_surface)
    return;

  if (x < 0 || y < 0 || x >= bmp->w || y >= bmp->h)
    return;

  bpp = bmp->sdl_surface->format->BytesPerPixel;
  pixels = bmp->line[y] + x * bpp;

  switch (bpp)
    {
    case 1:
      *pixels = (unsigned char) color;
      break;
    case 4:
      *(Uint32 *) pixels = (Uint32) color;
      break;
    default:
      break;
    }
}

int
getpixel (BITMAP * bmp, int x, int y)
{
  unsigned char *pixels;
  int bpp;

  if (!bmp || !bmp->sdl_surface)
    return 0;

  if (x < 0 || y < 0 || x >= bmp->w || y >= bmp->h)
    return 0;

  bpp = bmp->sdl_surface->format->BytesPerPixel;
  pixels = bmp->line[y] + x * bpp;

  switch (bpp)
    {
    case 1:
      return *pixels;
    case 4:
      return *(Uint32 *) pixels;
    default:
      return 0;
    }
}

void
hline (BITMAP * bmp, int x1, int y, int x2, int color)
{
  int x;
  if (x1 > x2)
    {
      int tmp = x1;
      x1 = x2;
      x2 = tmp;
    }
  for (x = x1; x <= x2; ++x)
    putpixel (bmp, x, y, color);
}

void
vline (BITMAP * bmp, int x, int y1, int y2, int color)
{
  int y;
  if (y1 > y2)
    {
      int tmp = y1;
      y1 = y2;
      y2 = tmp;
    }
  for (y = y1; y <= y2; ++y)
    putpixel (bmp, x, y, color);
}

void
rect (BITMAP * bmp, int x1, int y1, int x2, int y2, int color)
{
  hline (bmp, x1, y1, x2, color);
  hline (bmp, x1, y2, x2, color);
  vline (bmp, x1, y1, y2, color);
  vline (bmp, x2, y1, y2, color);
}

void
rectfill (BITMAP * bmp, int x1, int y1, int x2, int y2, int color)
{
  int y;
  if (y1 > y2)
    {
      int tmp = y1;
      y1 = y2;
      y2 = tmp;
    }
  for (y = y1; y <= y2; ++y)
    hline (bmp, x1, y, x2, color);
}

void
blit (BITMAP * src, BITMAP * dst, int sx, int sy, int dx, int dy,
      int w, int h)
{
  if (!src || !dst || !src->sdl_surface || !dst->sdl_surface)
    return;

  /* For 8-bit to 8-bit, do raw pixel copy to preserve palette indices */
  if (src->sdl_surface->format->BitsPerPixel == 8
      && dst->sdl_surface->format->BitsPerPixel == 8)
    {
      int row;
      for (row = 0; row < h; ++row)
        {
          int src_y = sy + row;
          int dst_y = dy + row;
          if (src_y >= 0 && src_y < src->h && dst_y >= 0 && dst_y < dst->h)
            {
              int copy_w = w;
              int src_x = sx;
              int dst_x = dx;
              if (src_x < 0) { copy_w += src_x; dst_x -= src_x; src_x = 0; }
              if (dst_x < 0) { copy_w += dst_x; src_x -= dst_x; dst_x = 0; }
              if (src_x + copy_w > src->w) copy_w = src->w - src_x;
              if (dst_x + copy_w > dst->w) copy_w = dst->w - dst_x;
              if (copy_w > 0)
                memcpy (dst->line[dst_y] + dst_x,
                        src->line[src_y] + src_x, copy_w);
            }
        }
    }
  else
    {
      SDL_Rect srect, drect;

      srect.x = sx;
      srect.y = sy;
      srect.w = w;
      srect.h = h;

      drect.x = dx;
      drect.y = dy;
      drect.w = w;
      drect.h = h;

      if (src->is_sub_bitmap)
        {
          srect.x += src->sub_x;
          srect.y += src->sub_y;
        }
      if (dst->is_sub_bitmap)
        {
          drect.x += dst->sub_x;
          drect.y += dst->sub_y;
        }

      SDL_BlitSurface (src->is_sub_bitmap ? src->parent->sdl_surface :
                       src->sdl_surface,
                       &srect,
                       dst->is_sub_bitmap ? dst->parent->sdl_surface :
                       dst->sdl_surface, &drect);
    }
}

void
stretch_blit (BITMAP * src, BITMAP * dst, int sx, int sy, int sw, int sh,
              int dx, int dy, int dw, int dh)
{
  if (!src || !dst || !src->sdl_surface || !dst->sdl_surface)
    return;

  /* For 8-bit to 8-bit, use direct pixel access for nearest-neighbor scaling */
  if (src->sdl_surface->format->BitsPerPixel == 8
      && dst->sdl_surface->format->BitsPerPixel == 8)
    {
      int x, y;
      for (y = 0; y < dh; ++y)
        {
          int src_y = sy + (y * sh) / dh;
          int dst_dy = dy + y;
          unsigned char *src_row;
          unsigned char *dst_row;

          if (src_y < 0 || src_y >= src->h || dst_dy < 0
              || dst_dy >= dst->h)
            continue;

          src_row = src->line[src_y];
          dst_row = dst->line[dst_dy];

          if (sw == dw)
            {
              /* No horizontal scaling - fast memcpy */
              int copy_w = dw;
              int s = sx, d = dx;
              if (s < 0) { copy_w += s; d -= s; s = 0; }
              if (d < 0) { copy_w += d; s -= d; d = 0; }
              if (s + copy_w > src->w) copy_w = src->w - s;
              if (d + copy_w > dst->w) copy_w = dst->w - d;
              if (copy_w > 0)
                memcpy (dst_row + d, src_row + s, copy_w);
            }
          else
            {
              for (x = 0; x < dw; ++x)
                {
                  int src_x = sx + (x * sw) / dw;
                  int dst_dx = dx + x;
                  if (src_x >= 0 && src_x < src->w && dst_dx >= 0
                      && dst_dx < dst->w)
                    dst_row[dst_dx] = src_row[src_x];
                }
            }
        }
    }
  else
    {
      SDL_Rect srect, drect;

      srect.x = sx;
      srect.y = sy;
      srect.w = sw;
      srect.h = sh;

      drect.x = dx;
      drect.y = dy;
      drect.w = dw;
      drect.h = dh;

      if (src->is_sub_bitmap)
        {
          srect.x += src->sub_x;
          srect.y += src->sub_y;
        }
      if (dst->is_sub_bitmap)
        {
          drect.x += dst->sub_x;
          drect.y += dst->sub_y;
        }

      SDL_BlitScaled (src->is_sub_bitmap ? src->parent->sdl_surface :
                      src->sdl_surface,
                      &srect,
                      dst->is_sub_bitmap ? dst->parent->sdl_surface :
                      dst->sdl_surface, &drect);
    }
}

void
draw_sprite (BITMAP * bmp, BITMAP * sprite, int x, int y)
{
  if (!bmp || !sprite)
    return;

  blit (sprite, bmp, 0, 0, x, y, sprite->w, sprite->h);
}

void
set_clip_rect (BITMAP * bmp, int x1, int y1, int x2, int y2)
{
  SDL_Rect clip;

  if (!bmp)
    return;

  bmp->cl = x1;
  bmp->ct = y1;
  bmp->cr = x2 + 1;
  bmp->cb = y2 + 1;

  if (bmp->sdl_surface)
    {
      clip.x = x1;
      clip.y = y1;
      clip.w = x2 - x1 + 1;
      clip.h = y2 - y1 + 1;
      SDL_SetClipRect (bmp->sdl_surface, &clip);
    }
}

void
scroll_screen (int x, int y)
{
  /* SDL2 doesn't have hardware scroll; this is a no-op.
   * Page flipping is handled via SDL_RenderPresent. */
  (void) x;
  (void) y;
}

int
makecol (int r, int g, int b)
{
  return (255 << 24) | (r << 16) | (g << 8) | b;
}

int
makecol8 (int r, int g, int b)
{
  /* Approximate 8-bit color from RGB */
  return ((r >> 5) << 5) | ((g >> 5) << 2) | (b >> 6);
}

void
ellipse (BITMAP * bmp, int cx, int cy, int rx, int ry, int color)
{
  /* Midpoint ellipse algorithm */
  int x, y;
  long rx2 = (long) rx * rx, ry2 = (long) ry * ry;
  long tworx2 = 2 * rx2, twory2 = 2 * ry2;
  long p, px, py;

  x = 0;
  y = ry;
  px = 0;
  py = tworx2 * y;

  putpixel (bmp, cx + x, cy + y, color);
  putpixel (bmp, cx - x, cy + y, color);
  putpixel (bmp, cx + x, cy - y, color);
  putpixel (bmp, cx - x, cy - y, color);

  p = (long) (ry2 - rx2 * ry + 0.25 * rx2);
  while (px < py)
    {
      x++;
      px += twory2;
      if (p < 0)
        p += ry2 + px;
      else
        {
          y--;
          py -= tworx2;
          p += ry2 + px - py;
        }
      putpixel (bmp, cx + x, cy + y, color);
      putpixel (bmp, cx - x, cy + y, color);
      putpixel (bmp, cx + x, cy - y, color);
      putpixel (bmp, cx - x, cy - y, color);
    }

  p = (long) (ry2 * (x + 0.5) * (x + 0.5) + rx2 * (y - 1) * (y - 1) -
              rx2 * ry2);
  while (y > 0)
    {
      y--;
      py -= tworx2;
      if (p > 0)
        p += rx2 - py;
      else
        {
          x++;
          px += twory2;
          p += rx2 - py + px;
        }
      putpixel (bmp, cx + x, cy + y, color);
      putpixel (bmp, cx - x, cy + y, color);
      putpixel (bmp, cx + x, cy - y, color);
      putpixel (bmp, cx - x, cy - y, color);
    }
}

void
ellipsefill (BITMAP * bmp, int cx, int cy, int rx, int ry, int color)
{
  int y;
  for (y = -ry; y <= ry; ++y)
    {
      double ratio = (ry > 0) ? (double) y / ry : 0;
      int xw = (int) (rx * sqrt (1.0 - ratio * ratio));
      hline (bmp, cx - xw, cy + y, cx + xw, color);
    }
}

void
line (BITMAP * bmp, int x1, int y1, int x2, int y2, int color)
{
  /* Bresenham's line algorithm */
  int dx = abs (x2 - x1), sx = x1 < x2 ? 1 : -1;
  int dy = -abs (y2 - y1), sy = y1 < y2 ? 1 : -1;
  int err = dx + dy, e2;

  for (;;)
    {
      putpixel (bmp, x1, y1, color);
      if (x1 == x2 && y1 == y2)
        break;
      e2 = 2 * err;
      if (e2 >= dy)
        {
          err += dy;
          x1 += sx;
        }
      if (e2 <= dx)
        {
          err += dx;
          y1 += sy;
        }
    }
}

void
polygon (BITMAP * bmp, int vertices, const int *points, int color)
{
  /* Simple scanline polygon fill */
  int i, j, y;
  int min_y = points[1], max_y = points[1];

  for (i = 1; i < vertices; ++i)
    {
      if (points[i * 2 + 1] < min_y)
        min_y = points[i * 2 + 1];
      if (points[i * 2 + 1] > max_y)
        max_y = points[i * 2 + 1];
    }

  for (y = min_y; y <= max_y; ++y)
    {
      int nodes[64], node_count = 0;

      j = vertices - 1;
      for (i = 0; i < vertices; ++i)
        {
          int yi = points[i * 2 + 1], yj = points[j * 2 + 1];
          int xi = points[i * 2], xj = points[j * 2];
          if ((yi < y && yj >= y) || (yj < y && yi >= y))
            {
              if (node_count < 64)
                nodes[node_count++] = xi + (y - yi) * (xj - xi) / (yj - yi);
            }
          j = i;
        }

      /* Sort nodes */
      for (i = 0; i < node_count - 1; ++i)
        for (j = i + 1; j < node_count; ++j)
          if (nodes[i] > nodes[j])
            {
              int tmp = nodes[i];
              nodes[i] = nodes[j];
              nodes[j] = tmp;
            }

      /* Fill between pairs */
      for (i = 0; i < node_count - 1; i += 2)
        hline (bmp, nodes[i], y, nodes[i + 1], color);
    }
}

void
circlefill (BITMAP * bmp, int cx, int cy, int r, int color)
{
  ellipsefill (bmp, cx, cy, r, r, color);
}

int
save_bitmap (const char *filename, BITMAP * bmp, const PALETTE pal)
{
  (void) pal;

  if (!bmp || !bmp->sdl_surface)
    return -1;

  return SDL_SaveBMP (bmp->sdl_surface, filename);
}

/*==================================================================*/
/* Text rendering                                                   */
/*==================================================================*/

void
textout_ex (BITMAP * bmp, const FONT * f, const char *str,
            int x, int y, int fg, int bg)
{
  SDL_Surface *text_surface;
  SDL_Color fg_color = { 255, 255, 255, 255 };
  int tw, th, tx, ty;
  Uint32 *src_pixels;
  int src_pitch;

  if (!bmp || !bmp->sdl_surface || !str || !str[0])
    return;

  if (!f || !f->ttf_font)
    return;

  /* Resolve foreground color for TTF rendering */
  if (fg >= 0 && bmp->sdl_surface->format->palette
      && fg < bmp->sdl_surface->format->palette->ncolors)
    {
      fg_color = bmp->sdl_surface->format->palette->colors[fg];
    }
  else if (fg >= 0)
    {
      fg_color.r = (fg >> 16) & 0xFF;
      fg_color.g = (fg >> 8) & 0xFF;
      fg_color.b = fg & 0xFF;
    }

  /* Render text to a 32-bit ARGB surface */
  text_surface = TTF_RenderText_Blended (f->ttf_font, str, fg_color);
  if (!text_surface)
    return;

  tw = text_surface->w;
  th = text_surface->h;

  /* Draw background if requested */
  if (bg >= 0)
    rectfill (bmp, x, y, x + tw - 1, y + th - 1, bg);

  /* Blit pixel-by-pixel for 8-bit targets, or use SDL_BlitSurface for 32-bit */
  if (bmp->sdl_surface->format->BitsPerPixel <= 8)
    {
      /* 8-bit target: write fg color where text alpha > 128 */
      int fg_idx = (fg >= 0) ? fg : 17;        /* default to palette entry 17 (menu fg) */
      SDL_LockSurface (text_surface);
      src_pixels = (Uint32 *) text_surface->pixels;
      src_pitch = text_surface->pitch / 4;

      for (ty = 0; ty < th; ++ty)
        for (tx = 0; tx < tw; ++tx)
          {
            Uint32 pixel = src_pixels[ty * src_pitch + tx];
            Uint8 alpha = (pixel >> 24) & 0xFF;
            if (alpha > 128)
              putpixel (bmp, x + tx, y + ty, fg_idx);
          }

      SDL_UnlockSurface (text_surface);
    }
  else
    {
      /* 32-bit target: blit directly */
      SDL_Rect dst_rect = { x, y, tw, th };
      if (bmp->is_sub_bitmap)
        {
          dst_rect.x += bmp->sub_x;
          dst_rect.y += bmp->sub_y;
        }
      SDL_BlitSurface (text_surface, NULL,
                       bmp->is_sub_bitmap ? bmp->parent->sdl_surface :
                       bmp->sdl_surface, &dst_rect);
    }

  SDL_FreeSurface (text_surface);
}

void
textout_centre_ex (BITMAP * bmp, const FONT * f, const char *str,
                   int x, int y, int fg, int bg)
{
  int w = text_length (f, str);
  textout_ex (bmp, f, str, x - w / 2, y, fg, bg);
}

int
text_height (const FONT * f)
{
  if (f && f->ttf_font)
    return TTF_FontHeight (f->ttf_font);
  return 8;
}

int
text_length (const FONT * f, const char *str)
{
  int w = 0, h = 0;
  if (f && f->ttf_font && str)
    TTF_SizeText (f->ttf_font, str, &w, &h);
  else if (str)
    w = strlen (str) * 8;
  return w;
}

void
gui_textout_ex (BITMAP * bmp, const char *str, int x, int y,
                int fg, int bg, int centre)
{
  if (centre)
    textout_centre_ex (bmp, font, str, x, y, fg, bg);
  else
    textout_ex (bmp, font, str, x, y, fg, bg);
}

BITMAP *
gui_get_screen (void)
{
  return screen;
}

int
gui_mouse_x (void)
{
  return mouse_x;
}

int
gui_mouse_y (void)
{
  return mouse_y;
}

/*==================================================================*/
/* Mouse functions                                                  */
/*==================================================================*/

void
show_mouse (BITMAP * bmp)
{
  if (bmp)
    SDL_ShowCursor (SDL_ENABLE);
  else
    SDL_ShowCursor (SDL_DISABLE);
}

void
scare_mouse (void)
{
  SDL_ShowCursor (SDL_DISABLE);
}

void
unscare_mouse (void)
{
  SDL_ShowCursor (SDL_ENABLE);
}

int
show_os_cursor (int cursor)
{
  SDL_ShowCursor (cursor ? SDL_ENABLE : SDL_DISABLE);
  return 0;
}

void
position_mouse (int x, int y)
{
  if (lw_sdl_window)
    SDL_WarpMouseInWindow (lw_sdl_window, x, y);
  mouse_x = x;
  mouse_y = y;
}

void
set_mouse_sprite (BITMAP * sprite)
{
  /* TODO: custom cursor from bitmap */
  (void) sprite;
}

/*==================================================================*/
/* Palette / fade functions                                         */
/*==================================================================*/

void
fade_out (int speed)
{
  /* Simple fade to black using SDL2 */
  int i;
  SDL_Surface *overlay;

  if (!lw_sdl_window || !lw_sdl_renderer)
    return;

  overlay = SDL_CreateRGBSurface (0, SCREEN_W, SCREEN_H, 32,
                                  0x00FF0000, 0x0000FF00, 0x000000FF,
                                  0xFF000000);

  for (i = 0; i < 64; i += speed)
    {
      int alpha = (i * 255) / 64;
      SDL_FillRect (overlay, NULL,
                    SDL_MapRGBA (overlay->format, 0, 0, 0, alpha));
      SDL_Delay (10);
    }

  if (overlay)
    SDL_FreeSurface (overlay);
}

void
fade_in (const PALETTE pal, int speed)
{
  (void) speed;
  /* Apply the target palette immediately */
  set_palette ((RGB *) pal);
}

int
bestfit_color (const PALETTE pal, int r, int g, int b)
{
  int i, best = 0, best_dist = 0x7FFFFFFF;
  for (i = 0; i < 256; ++i)
    {
      int dr = (pal[i].r * 4) - r;
      int dg = (pal[i].g * 4) - g;
      int db = (pal[i].b * 4) - b;
      int dist = dr * dr + dg * dg + db * db;
      if (dist < best_dist)
        {
          best_dist = dist;
          best = i;
        }
    }
  return best;
}

void
get_palette (PALETTE pal)
{
  int i;

  if (screen && screen->sdl_surface && screen->sdl_surface->format->palette)
    {
      SDL_Palette *sdl_pal = screen->sdl_surface->format->palette;
      for (i = 0; i < 256 && i < sdl_pal->ncolors; ++i)
        {
          pal[i].r = sdl_pal->colors[i].r / 4;
          pal[i].g = sdl_pal->colors[i].g / 4;
          pal[i].b = sdl_pal->colors[i].b / 4;
        }
    }
  else
    memset (pal, 0, sizeof (PALETTE));
}

void
hsv_to_rgb (float h, float s, float v, int *r, int *g, int *b)
{
  float c = v * s;
  float x = c * (1.0f - fabsf (fmodf (h / 60.0f, 2.0f) - 1.0f));
  float m = v - c;
  float rf, gf, bf;

  if (h < 60)
    { rf = c; gf = x; bf = 0; }
  else if (h < 120)
    { rf = x; gf = c; bf = 0; }
  else if (h < 180)
    { rf = 0; gf = c; bf = x; }
  else if (h < 240)
    { rf = 0; gf = x; bf = c; }
  else if (h < 300)
    { rf = x; gf = 0; bf = c; }
  else
    { rf = c; gf = 0; bf = x; }

  *r = (int) ((rf + m) * 255);
  *g = (int) ((gf + m) * 255);
  *b = (int) ((bf + m) * 255);
}

void
rgb_to_hsv (int r, int g, int b, float *h, float *s, float *v)
{
  float rf = r / 255.0f, gf = g / 255.0f, bf = b / 255.0f;
  float max = rf > gf ? (rf > bf ? rf : bf) : (gf > bf ? gf : bf);
  float min = rf < gf ? (rf < bf ? rf : bf) : (gf < bf ? gf : bf);
  float d = max - min;

  *v = max;
  *s = (max > 0) ? d / max : 0;

  if (d == 0)
    *h = 0;
  else if (max == rf)
    *h = 60.0f * fmodf ((gf - bf) / d, 6.0f);
  else if (max == gf)
    *h = 60.0f * ((bf - rf) / d + 2.0f);
  else
    *h = 60.0f * ((rf - gf) / d + 4.0f);

  if (*h < 0)
    *h += 360.0f;
}

/*==================================================================*/
/* Sound functions                                                  */
/*==================================================================*/

void
play_sample (const SAMPLE * spl, int vol, int pan, int freq, int loop)
{
  int channel;
  (void) pan;
  (void) freq;

  if (!spl || !lw_audio_initialized)
    return;

  channel = Mix_PlayChannel (-1, (Mix_Chunk *) spl, loop ? -1 : 0);
  if (channel >= 0)
    Mix_Volume (channel, vol * MIX_MAX_VOLUME / 255);
}

void
stop_sample (const SAMPLE * spl)
{
  (void) spl;
  /* SDL2_mixer doesn't track samples to channels easily;
   * stop all channels as a fallback */
  if (lw_audio_initialized)
    Mix_HaltChannel (-1);
}

void
adjust_sample (const SAMPLE * spl, int vol, int pan, int freq, int loop)
{
  (void) spl;
  (void) vol;
  (void) pan;
  (void) freq;
  (void) loop;
}

int
play_midi (MIDI * music, int loop)
{
  if (!music || !lw_audio_initialized)
    return -1;

  if (Mix_PlayMusic ((Mix_Music *) music, loop ? -1 : 1) < 0)
    return -1;
  midi_pos = 0;
  return 0;
}

void
set_volume (int digi_volume, int midi_volume)
{
  if (digi_volume >= 0)
    Mix_Volume (-1, digi_volume * MIX_MAX_VOLUME / 255);
  if (midi_volume >= 0)
    Mix_VolumeMusic (midi_volume * MIX_MAX_VOLUME / 255);
}

void
stop_midi (void)
{
  if (lw_audio_initialized)
    Mix_HaltMusic ();
  midi_pos = -1;
}

/*==================================================================*/
/* Joystick functions                                               */
/*==================================================================*/

int
install_joystick (int type)
{
  (void) type;

  if (SDL_InitSubSystem (SDL_INIT_JOYSTICK) < 0)
    return -1;

  num_joysticks = SDL_NumJoysticks ();
  if (num_joysticks > 8)
    num_joysticks = 8;

  return 0;
}

int
poll_joystick (void)
{
  SDL_JoystickUpdate ();
  return 0;
}

/*==================================================================*/
/* Datafile functions (stub)                                        */
/*==================================================================*/

DATAFILE *
load_datafile_object (const char *filename, const char *objectname)
{
  /* TODO: implement .dat file loading or individual file loading */
  fprintf (stderr,
           "WARNING: load_datafile_object('%s', '%s') not yet implemented\n",
           filename ? filename : "(null)",
           objectname ? objectname : "(null)");
  return NULL;
}

void
unload_datafile_object (DATAFILE * dat)
{
  if (dat)
    {
      if (dat->dat)
        free (dat->dat);
      free (dat);
    }
}

/*==================================================================*/
/* Dialog functions (stubs for compilation)                         */
/*==================================================================*/

int
d_button_proc (int msg, DIALOG * d, int c)
{
  (void) c;
  if (msg == MSG_DRAW && screen)
    {
      int hover = (d->flags & D_GOTMOUSE_FLAG) ? 1 : 0;
      int bg_col = d->bg;
      int fg_col = d->fg;

      /* Brighten on hover */
      if (hover)
        {
          bg_col = d->fg;
          fg_col = d->bg;
        }

      rectfill (screen, d->x, d->y, d->x + d->w - 1, d->y + d->h - 1,
                bg_col);
      rect (screen, d->x, d->y, d->x + d->w - 1, d->y + d->h - 1, fg_col);
      if (d->dp && font)
        {
          textout_centre_ex (screen, font, (const char *) d->dp,
                             d->x + d->w / 2,
                             d->y + d->h / 2 - text_height (font) / 2,
                             fg_col, -1);
        }
    }
  return D_O_K;
}

int
d_text_proc (int msg, DIALOG * d, int c)
{
  (void) c;
  if (msg == MSG_DRAW && screen && d->dp && font)
    {
      textout_ex (screen, font, (const char *) d->dp,
                  d->x, d->y, d->fg, d->bg);
    }
  return D_O_K;
}

int
d_ctext_proc (int msg, DIALOG * d, int c)
{
  (void) c;
  if (msg == MSG_DRAW && screen && d->dp && font)
    {
      textout_centre_ex (screen, font, (const char *) d->dp,
                         d->x + d->w / 2, d->y, d->fg, d->bg);
    }
  return D_O_K;
}

int
d_edit_proc (int msg, DIALOG * d, int c)
{
  (void) c;
  if (msg == MSG_DRAW && screen)
    {
      rectfill (screen, d->x, d->y, d->x + d->w - 1, d->y + d->h - 1,
                d->bg);
      rect (screen, d->x, d->y, d->x + d->w - 1, d->y + d->h - 1, d->fg);
      if (d->dp && font)
        textout_ex (screen, font, (const char *) d->dp,
                    d->x + 2, d->y + 2, d->fg, -1);
    }
  return D_O_K;
}

int
d_list_proc (int msg, DIALOG * d, int c)
{
  (void) c;
  if (msg == MSG_DRAW && screen)
    {
      rectfill (screen, d->x, d->y, d->x + d->w - 1, d->y + d->h - 1,
                d->bg);
      rect (screen, d->x, d->y, d->x + d->w - 1, d->y + d->h - 1, d->fg);
    }
  return D_O_K;
}

int
d_slider_proc (int msg, DIALOG * d, int c)
{
  (void) c;
  if (msg == MSG_DRAW && screen)
    {
      int pos;
      rectfill (screen, d->x, d->y, d->x + d->w - 1, d->y + d->h - 1,
                d->bg);
      rect (screen, d->x, d->y, d->x + d->w - 1, d->y + d->h - 1, d->fg);
      /* Draw slider position */
      if (d->d1 > 0)
        {
          pos = d->x + (d->d2 * (d->w - 4)) / d->d1 + 2;
          rectfill (screen, pos - 2, d->y + 1, pos + 2,
                    d->y + d->h - 2, d->fg);
        }
    }
  return D_O_K;
}

int
d_textbox_proc (int msg, DIALOG * d, int c)
{
  (void) c;
  if (msg == MSG_DRAW && screen)
    {
      rectfill (screen, d->x, d->y, d->x + d->w - 1, d->y + d->h - 1,
                d->bg);
      if (d->dp && font)
        textout_ex (screen, font, (const char *) d->dp,
                    d->x + 4, d->y + 4, d->fg, -1);
    }
  return D_O_K;
}

int
d_clear_proc (int msg, DIALOG * d, int c)
{
  (void) c;
  if (msg == MSG_DRAW && screen)
    {
      rectfill (screen, d->x, d->y, d->x + d->w - 1, d->y + d->h - 1,
                d->bg);
    }
  return D_O_K;
}

int d_box_proc (int msg, DIALOG * d, int c) { (void) c; if (msg == MSG_DRAW && screen) { rectfill(screen, d->x, d->y, d->x+d->w-1, d->y+d->h-1, d->bg); rect(screen, d->x, d->y, d->x+d->w-1, d->y+d->h-1, d->fg); } return D_O_K; }
int d_shadow_box_proc (int msg, DIALOG * d, int c) { (void) msg; (void) d; (void) c; return D_O_K; }
int d_bitmap_proc (int msg, DIALOG * d, int c) { (void) msg; (void) d; (void) c; return D_O_K; }
int d_icon_proc (int msg, DIALOG * d, int c) { (void) msg; (void) d; (void) c; return D_O_K; }
int d_keyboard_proc (int msg, DIALOG * d, int c) { (void) msg; (void) d; (void) c; return D_O_K; }
int d_check_proc (int msg, DIALOG * d, int c) { (void) msg; (void) d; (void) c; return D_O_K; }
int d_radio_proc (int msg, DIALOG * d, int c) { (void) msg; (void) d; (void) c; return D_O_K; }
int d_menu_proc (int msg, DIALOG * d, int c) { (void) msg; (void) d; (void) c; return D_O_K; }
int d_yield_proc (int msg, DIALOG * d, int c) { (void) msg; (void) d; (void) c; return D_O_K; }

static void
lw_dialog_draw_all (DIALOG * d)
{
  int i;
  for (i = 0; d[i].proc; ++i)
    {
      if (!(d[i].flags & D_HIDDEN))
        d[i].proc (MSG_DRAW, &d[i], 0);
    }
  lw_sdl_present_screen ();
}

static int
lw_dialog_find_click (DIALOG * d, int mx, int my)
{
  int i, best = -1;
  int best_area = 0x7FFFFFFF;

  /* Find the smallest (most specific) element under the cursor.
   * This ensures inner buttons take priority over outer containers. */
  for (i = 0; d[i].proc; ++i)
    {
      int area;
      if ((d[i].flags & D_HIDDEN) || (d[i].flags & D_DISABLED))
        continue;
      if (mx >= d[i].x && mx < d[i].x + d[i].w
          && my >= d[i].y && my < d[i].y + d[i].h)
        {
          area = d[i].w * d[i].h;
          if (area < best_area)
            {
              best_area = area;
              best = i;
            }
        }
    }
  return best;
}

static int
lw_dialog_find_key (DIALOG * d, int key_char)
{
  int i;
  for (i = 0; d[i].proc; ++i)
    {
      if (d[i].key && d[i].key == key_char && (d[i].flags & D_EXIT_FLAG))
        return i;
    }
  return -1;
}

DIALOG_PLAYER *
init_dialog (DIALOG * d, int focus)
{
  DIALOG_PLAYER *player;
  int i;

  player = (DIALOG_PLAYER *) calloc (1, sizeof (DIALOG_PLAYER));
  if (player)
    {
      player->dialog = d;
      player->focus = focus;
      player->obj = -1;

      /* Send MSG_START to all elements */
      for (i = 0; d[i].proc; ++i)
        d[i].proc (MSG_START, &d[i], 0);

      /* Initial draw */
      lw_dialog_draw_all (d);
    }

  return player;
}

int
update_dialog (DIALOG_PLAYER * player)
{
  DIALOG *d;
  int i, clicked;

  if (!player || !player->dialog)
    return 0;

  d = player->dialog;

  /* Pump events */
  lw_sdl_pump_events ();

  /* Check for keyboard input */
  for (i = 0; i < KEY_MAX; ++i)
    {
      if (key[i])
        {
          int key_char = 0;

          /* Map common scancodes to ASCII for key matching */
          if (i >= SDL_SCANCODE_A && i <= SDL_SCANCODE_Z)
            key_char = 'a' + (i - SDL_SCANCODE_A);
          else if (i == SDL_SCANCODE_ESCAPE)
            key_char = 27;
          else if (i == SDL_SCANCODE_RETURN)
            key_char = 13;

          if (key_char)
            {
              int idx = lw_dialog_find_key (d, key_char);
              if (idx >= 0)
                {
                  player->obj = idx;
                  key[i] = 0;
                  return 0;     /* dialog done */
                }
            }

          /* ESC closes dialog returning first element index */
          if (i == SDL_SCANCODE_ESCAPE)
            {
              player->obj = 0;
              key[i] = 0;
              return 0;
            }
        }
    }

  /* Check for mouse click */
  if (mouse_b & 1)
    {
      clicked = lw_dialog_find_click (d, mouse_x, mouse_y);
      if (clicked >= 0)
        {
          int result = d[clicked].proc (MSG_CLICK, &d[clicked], 0);
          if ((d[clicked].flags & D_EXIT_FLAG) || (result & D_CLOSE))
            {
              player->obj = clicked;
              /* Wait for mouse release */
              while (mouse_b & 1)
                lw_sdl_pump_events ();
              return 0;
            }
        }
      /* Wait for mouse release to avoid repeated clicks */
      while (mouse_b & 1)
        lw_sdl_pump_events ();
    }

  /* Update hover state */
  {
    int hi, hovered = lw_dialog_find_click (d, mouse_x, mouse_y);
    for (hi = 0; d[hi].proc; ++hi)
      {
        if (hi == hovered)
          d[hi].flags |= D_GOTMOUSE_FLAG;
        else
          d[hi].flags &= ~D_GOTMOUSE_FLAG;
      }
  }

  /* Redraw */
  lw_dialog_draw_all (d);

  /* Small delay to avoid busy loop */
  SDL_Delay (16);

  return 1;                     /* dialog still active */
}

int
shutdown_dialog (DIALOG_PLAYER * player)
{
  int ret = -1;
  int i;

  if (player)
    {
      ret = player->obj;
      if (player->dialog)
        {
          for (i = 0; player->dialog[i].proc; ++i)
            player->dialog[i].proc (MSG_END, &player->dialog[i], 0);
        }
      free (player);
    }
  return ret;
}

int
do_dialog (DIALOG * d, int focus)
{
  DIALOG_PLAYER *player = init_dialog (d, focus);
  if (!player)
    return -1;

  while (update_dialog (player))
    ;

  return shutdown_dialog (player);
}

int
popup_dialog (DIALOG * d, int focus)
{
  return do_dialog (d, focus);
}

void
broadcast_dialog_message (int msg, int c)
{
  (void) msg;
  (void) c;
  /* Brief yield to prevent busy-waiting in tracking loops */
  SDL_Delay (1);
}

void
_draw_scrollable_frame (DIALOG * d, int listsize, int offset,
                        int height, int fg_color, int bg)
{
  (void) d;
  (void) listsize;
  (void) offset;
  (void) height;
  (void) fg_color;
  (void) bg;
}

/*==================================================================*/
/* Event pump                                                       */
/*==================================================================*/

/*==================================================================*/
/* Screen presentation - upload screen surface to window            */
/*==================================================================*/

void
lw_sdl_present_screen (void)
{
  int x, y;
  Uint32 *dst_pixels;
  int dst_pitch;
  SDL_Palette *pal;

  static Uint32 lw_last_snap = 0;
  static int lw_snap_id = 0;

  if (!screen || !screen->sdl_surface || !lw_sdl_renderer)
    return;

  /* Periodic screenshot dump for debugging (every 2s, max 30) */
  {
    Uint32 now = SDL_GetTicks ();
    if (lw_snap_id < 30 && now - lw_last_snap > 2000)
      {
        char path[128];
        SDL_Surface *snap =
          SDL_ConvertSurfaceFormat (screen->sdl_surface,
                                   SDL_PIXELFORMAT_RGB24, 0);
        if (snap)
          {
            snprintf (path, sizeof (path), "/tmp/lw_frames/f%03d.bmp",
                      lw_snap_id++);
            SDL_SaveBMP (snap, path);
            SDL_FreeSurface (snap);
          }
        lw_last_snap = now;
      }
  }

  /* Recreate texture and conversion surface if screen size changed */
  if (!lw_screen_texture || lw_screen_tex_w != screen->w
      || lw_screen_tex_h != screen->h)
    {
      if (lw_screen_texture)
        SDL_DestroyTexture (lw_screen_texture);
      if (lw_convert_surface)
        SDL_FreeSurface (lw_convert_surface);

      lw_screen_texture =
        SDL_CreateTexture (lw_sdl_renderer, SDL_PIXELFORMAT_ARGB8888,
                           SDL_TEXTUREACCESS_STREAMING, screen->w,
                           screen->h);
      lw_convert_surface =
        SDL_CreateRGBSurfaceWithFormat (0, screen->w, screen->h, 32,
                                        SDL_PIXELFORMAT_ARGB8888);
      lw_screen_tex_w = screen->w;
      lw_screen_tex_h = screen->h;
    }

  if (!lw_screen_texture || !lw_convert_surface)
    return;

  /* Fast manual palette lookup - convert 8-bit indexed to 32-bit ARGB */
  pal = screen->sdl_surface->format->palette;
  dst_pixels = (Uint32 *) lw_convert_surface->pixels;
  dst_pitch = lw_convert_surface->pitch / 4;

  if (pal)
    {
      for (y = 0; y < screen->h; ++y)
        {
          unsigned char *src_row = screen->line[y];
          Uint32 *dst_row = dst_pixels + y * dst_pitch;
          for (x = 0; x < screen->w; ++x)
            {
              SDL_Color *c = &pal->colors[src_row[x]];
              dst_row[x] =
                (255u << 24) | (c->r << 16) | (c->g << 8) | c->b;
            }
        }
    }

  SDL_UpdateTexture (lw_screen_texture, NULL, lw_convert_surface->pixels,
                     lw_convert_surface->pitch);
  SDL_RenderClear (lw_sdl_renderer);
  SDL_RenderCopy (lw_sdl_renderer, lw_screen_texture, NULL, NULL);
  SDL_RenderPresent (lw_sdl_renderer);
}

/*==================================================================*/
/* Font loading                                                     */
/*==================================================================*/

static const char *lw_font_search_paths[] = {
  "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
  "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
  "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
  "/usr/share/fonts/dejavu/DejaVuSansMono.ttf",
  "/usr/share/fonts/truetype/freefont/FreeMono.ttf",
  "/Library/Fonts/Courier New.ttf",
  "C:\\Windows\\Fonts\\cour.ttf",
  NULL
};

FONT *
lw_sdl_load_font (int size)
{
  FONT *f;
  TTF_Font *ttf = NULL;
  int i;

  for (i = 0; lw_font_search_paths[i] && !ttf; ++i)
    ttf = TTF_OpenFont (lw_font_search_paths[i], size);

  if (!ttf)
    {
      fprintf (stderr, "WARNING: could not find a TTF font\n");
      return NULL;
    }

  f = (FONT *) calloc (1, sizeof (FONT));
  if (!f)
    {
      TTF_CloseFont (ttf);
      return NULL;
    }

  f->ttf_font = ttf;
  f->height = TTF_FontHeight (ttf);
  f->is_bitmap_font = 0;
  f->glyph_cache = NULL;

  return f;
}

/*==================================================================*/
/* Keyboard helper functions                                        */
/*==================================================================*/

/* Key event queue for keypressed/readkey compatibility */
#define LW_KEY_QUEUE_SIZE 64
static int lw_key_queue[LW_KEY_QUEUE_SIZE];
static int lw_key_queue_head = 0;
static int lw_key_queue_tail = 0;

void
lw_sdl_enqueue_key (int keyval)
{
  int next = (lw_key_queue_tail + 1) % LW_KEY_QUEUE_SIZE;
  if (next != lw_key_queue_head)
    {
      lw_key_queue[lw_key_queue_tail] = keyval;
      lw_key_queue_tail = next;
    }
}

int
keypressed (void)
{
  lw_sdl_pump_events ();
  return lw_key_queue_head != lw_key_queue_tail;
}

int
readkey (void)
{
  int val;

  /* Wait for a key event to arrive */
  while (lw_key_queue_head == lw_key_queue_tail)
    {
      lw_sdl_pump_events ();
      SDL_Delay (10);
    }

  val = lw_key_queue[lw_key_queue_head];
  lw_key_queue_head = (lw_key_queue_head + 1) % LW_KEY_QUEUE_SIZE;
  return val;
}

void
clear_keybuf (void)
{
  SDL_Event event;
  while (SDL_PollEvent (&event))
    ;
  memset ((void *) key, 0, sizeof (key));
  lw_key_queue_head = lw_key_queue_tail = 0;
}

/*==================================================================*/
/* GUI helper functions                                             */
/*==================================================================*/

int
gui_mouse_b (void)
{
  lw_sdl_pump_events ();
  return mouse_b;
}

void
object_message (DIALOG * d, int msg, int c)
{
  if (d && d->proc)
    {
      d->proc (msg, d, c);
      /* Present screen after draw messages so button state changes are visible */
      if (msg == MSG_DRAW)
        lw_sdl_present_screen ();
    }
}

void
rest_callback (int ms, void (*callback) (void))
{
  int start = SDL_GetTicks ();
  while ((int) SDL_GetTicks () - start < ms)
    {
      if (callback)
        callback ();
      SDL_Delay (1);
    }
}

/*==================================================================*/
/* Event pump                                                       */
/*==================================================================*/

void
lw_sdl_pump_events (void)
{
  SDL_Event event;

  while (SDL_PollEvent (&event))
    {
      switch (event.type)
        {
        case SDL_QUIT:
          if (lw_close_button_callback)
            lw_close_button_callback ();
          break;

        case SDL_KEYDOWN:
          if (event.key.keysym.scancode < KEY_MAX)
            key[event.key.keysym.scancode] = 1;
          /* Enqueue for keypressed/readkey */
          lw_sdl_enqueue_key ((event.key.keysym.scancode << 8) |
                              (event.key.keysym.sym & 0xFF));
          {
            SDL_Keymod mod = SDL_GetModState ();
            key_shifts = 0;
            if (mod & KMOD_SHIFT)
              key_shifts |= KB_SHIFT_FLAG;
            if (mod & KMOD_CTRL)
              key_shifts |= KB_CTRL_FLAG;
            if (mod & KMOD_ALT)
              key_shifts |= KB_ALT_FLAG;
          }
          break;

        case SDL_KEYUP:
          if (event.key.keysym.scancode < KEY_MAX)
            key[event.key.keysym.scancode] = 0;
          {
            SDL_Keymod mod = SDL_GetModState ();
            key_shifts = 0;
            if (mod & KMOD_SHIFT)
              key_shifts |= KB_SHIFT_FLAG;
            if (mod & KMOD_CTRL)
              key_shifts |= KB_CTRL_FLAG;
            if (mod & KMOD_ALT)
              key_shifts |= KB_ALT_FLAG;
          }
          break;

        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
          /* Use SDL logical coordinate mapping for mouse position */
          {
            float fx, fy;
            SDL_RenderWindowToLogical (lw_sdl_renderer,
                                       event.type == SDL_MOUSEMOTION
                                       ? event.motion.x : event.button.x,
                                       event.type == SDL_MOUSEMOTION
                                       ? event.motion.y : event.button.y,
                                       &fx, &fy);
            mouse_x = (int) fx;
            mouse_y = (int) fy;
          }
          if (event.type == SDL_MOUSEBUTTONDOWN)
            mouse_b |= (1 << (event.button.button - 1));
          else if (event.type == SDL_MOUSEBUTTONUP)
            mouse_b &= ~(1 << (event.button.button - 1));
          break;

        default:
          break;
        }
    }
}
