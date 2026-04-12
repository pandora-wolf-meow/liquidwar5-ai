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

const char *allegro_id = "SDL2 compatibility layer for Liquid War 5";
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

  flags = SDL_WINDOW_SHOWN;
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
  /* In SDL2 mode, palette operations are handled differently.
   * For now this is a no-op; colors are direct RGBA. */
  (void) pal;
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
  SDL_Rect srect, drect;

  if (!src || !dst || !src->sdl_surface || !dst->sdl_surface)
    return;

  srect.x = sx;
  srect.y = sy;
  srect.w = w;
  srect.h = h;

  drect.x = dx;
  drect.y = dy;
  drect.w = w;
  drect.h = h;

  /* Handle sub-bitmaps by adjusting coordinates */
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

void
stretch_blit (BITMAP * src, BITMAP * dst, int sx, int sy, int sw, int sh,
              int dx, int dy, int dw, int dh)
{
  SDL_Rect srect, drect;

  if (!src || !dst || !src->sdl_surface || !dst->sdl_surface)
    return;

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
  /* TODO: implement proper text rendering with SDL2_ttf */
  (void) bmp;
  (void) f;
  (void) str;
  (void) x;
  (void) y;
  (void) fg;
  (void) bg;
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

/*==================================================================*/
/* Palette / fade functions                                         */
/*==================================================================*/

void
fade_out (int speed)
{
  /* Simple fade to black using SDL2 */
  int i;
  SDL_Surface *overlay;
  SDL_Rect fullscreen;

  if (!lw_sdl_window || !lw_sdl_renderer)
    return;

  fullscreen.x = 0;
  fullscreen.y = 0;
  fullscreen.w = SCREEN_W;
  fullscreen.h = SCREEN_H;

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
  /* Simple fade from black */
  (void) pal;
  (void) speed;
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

void
play_midi (MIDI * music, int loop)
{
  if (!music || !lw_audio_initialized)
    return;

  Mix_PlayMusic ((Mix_Music *) music, loop ? -1 : 1);
  midi_pos = 0;
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
  (void) msg;
  (void) d;
  (void) c;
  return D_O_K;
}

int
d_text_proc (int msg, DIALOG * d, int c)
{
  (void) msg;
  (void) d;
  (void) c;
  return D_O_K;
}

int
d_ctext_proc (int msg, DIALOG * d, int c)
{
  (void) msg;
  (void) d;
  (void) c;
  return D_O_K;
}

int
d_edit_proc (int msg, DIALOG * d, int c)
{
  (void) msg;
  (void) d;
  (void) c;
  return D_O_K;
}

int
d_list_proc (int msg, DIALOG * d, int c)
{
  (void) msg;
  (void) d;
  (void) c;
  return D_O_K;
}

int
d_slider_proc (int msg, DIALOG * d, int c)
{
  (void) msg;
  (void) d;
  (void) c;
  return D_O_K;
}

int
d_textbox_proc (int msg, DIALOG * d, int c)
{
  (void) msg;
  (void) d;
  (void) c;
  return D_O_K;
}

int
d_clear_proc (int msg, DIALOG * d, int c)
{
  (void) msg;
  (void) d;
  (void) c;
  return D_O_K;
}

DIALOG_PLAYER *
init_dialog (DIALOG * d, int focus)
{
  DIALOG_PLAYER *player;
  (void) focus;

  player = (DIALOG_PLAYER *) calloc (1, sizeof (DIALOG_PLAYER));
  if (player)
    player->dialog = d;

  return player;
}

int
update_dialog (DIALOG_PLAYER * player)
{
  (void) player;
  return 1;                     /* return non-zero to signal "dialog still active" */
}

int
shutdown_dialog (DIALOG_PLAYER * player)
{
  int ret = 0;
  if (player)
    {
      ret = player->obj;
      free (player);
    }
  return ret;
}

int
do_dialog (DIALOG * d, int focus)
{
  (void) d;
  (void) focus;
  return -1;
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
          break;

        case SDL_KEYUP:
          if (event.key.keysym.scancode < KEY_MAX)
            key[event.key.keysym.scancode] = 0;
          break;

        case SDL_MOUSEMOTION:
          mouse_x = event.motion.x;
          mouse_y = event.motion.y;
          break;

        case SDL_MOUSEBUTTONDOWN:
          mouse_b |= (1 << (event.button.button - 1));
          break;

        case SDL_MOUSEBUTTONUP:
          mouse_b &= ~(1 << (event.button.button - 1));
          break;

        default:
          break;
        }
    }
}
