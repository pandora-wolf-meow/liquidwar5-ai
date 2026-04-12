/********************************************************************/
/* SDL2 compatibility layer for Liquid War 5                        */
/* Provides Allegro 4 API types and functions backed by SDL2        */
/********************************************************************/

#ifndef LIQUID_WAR_SDL_COMPAT_H
#define LIQUID_WAR_SDL_COMPAT_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*==================================================================*/
/* Common macros missing from non-Allegro builds                    */
/*==================================================================*/

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef MID
#define MID(a, b, c) MAX((a), MIN((b), (c)))
#endif

#define ASSERT(x) ((void)0)

/*==================================================================*/
/* Fixed-point math compatibility                                   */
/*==================================================================*/

typedef int fixed;

#define itofix(x)   ((x) << 16)
#define fixtoi(x)   ((x) >> 16)
#define fixmul(x,y) (int)(((long long)(x) * (long long)(y)) >> 16)
#define fixdiv(x,y) (int)(((long long)(x) << 16) / (y))
#define ftofix(x)   ((int)((x) * 65536.0))
#define fixtof(x)   ((double)(x) / 65536.0)

static inline fixed fixsqrt (fixed x)
{
  if (x <= 0)
    return 0;
  return ftofix (sqrt (fixtof (x)));
}

static inline fixed fixsin (fixed x)
{
  return ftofix (sin (fixtof (x) * M_PI / 128.0));
}

static inline fixed fixcos (fixed x)
{
  return ftofix (cos (fixtof (x) * M_PI / 128.0));
}

/*==================================================================*/
/* BITMAP compatibility type                                        */
/*==================================================================*/

typedef struct BITMAP
{
  int w, h;
  int clip;
  int cl, cr, ct, cb;
  void *dat;
  unsigned char **line;
  SDL_Surface *sdl_surface;
  struct BITMAP *parent;
  int is_sub_bitmap;
  int sub_x, sub_y;
} BITMAP;

/*==================================================================*/
/* Color and palette types                                          */
/*==================================================================*/

typedef struct RGB
{
  unsigned char r, g, b;
  unsigned char filler;
} RGB;

typedef RGB PALETTE[256];

extern PALETTE black_palette;

/*==================================================================*/
/* Font type                                                        */
/*==================================================================*/

typedef struct FONT
{
  TTF_Font *ttf_font;
  int height;
  int is_bitmap_font;
  SDL_Surface **glyph_cache;
} FONT;

extern FONT *font;

/*==================================================================*/
/* Sound types                                                      */
/*==================================================================*/

typedef Mix_Chunk SAMPLE;
typedef Mix_Music MIDI;

/*==================================================================*/
/* Datafile type                                                    */
/*==================================================================*/

#define DAT_END   0
#define DAT_FILE  1
#define DAT_DATA  2
#define DAT_FONT  3
#define DAT_SAMPLE 4
#define DAT_MIDI  5
#define DAT_BITMAP 6
#define DAT_PALETTE 7

typedef struct DATAFILE
{
  void *dat;
  int type;
  long size;
  void *prop;
} DATAFILE;

/*==================================================================*/
/* Dialog system types                                              */
/*==================================================================*/

struct DIALOG;

typedef int (*DIALOG_PROC) (int msg, struct DIALOG * d, int c);

typedef struct DIALOG
{
  DIALOG_PROC proc;
  int x, y, w, h;
  int fg, bg;
  int key;
  int flags;
  int d1, d2;
  void *dp, *dp2, *dp3;
} DIALOG;

typedef struct DIALOG_PLAYER
{
  DIALOG *dialog;
  int obj;
  int focus;
} DIALOG_PLAYER;

/* Dialog messages */
#define MSG_START    1
#define MSG_END      2
#define MSG_DRAW     3
#define MSG_CLICK    4
#define MSG_DCLICK   5
#define MSG_KEY      6
#define MSG_CHAR     7
#define MSG_WANTFOCUS 8
#define MSG_GOTFOCUS  9
#define MSG_LOSTFOCUS 10
#define MSG_GOTMOUSE  11
#define MSG_LOSTMOUSE 12
#define MSG_IDLE     13
#define MSG_RADIO    14
#define MSG_WHEEL    15
#define MSG_LPRESS   16
#define MSG_LRELEASE 17
#define MSG_MPRESS   18
#define MSG_MRELEASE 19
#define MSG_RPRESS   20
#define MSG_RRELEASE 21
#define MSG_WANTMOUSE 22
#define MSG_USER     25

/* Dialog return values */
#define D_O_K       0
#define D_CLOSE     1
#define D_REDRAW    2
#define D_WANTFOCUS 4
#define D_USED_CHAR 8
#define D_REDRAWME  16
#define D_GOTFOCUS  32
#define D_GOTMOUSE  64
#define D_EXIT      128

/* Dialog flags */
#define D_EXIT_FLAG    1
#define D_SELECTED     2
#define D_GOTFOCUS_FLAG 4
#define D_GOTMOUSE_FLAG 8
#define D_HIDDEN       16
#define D_DISABLED     32
#define D_DIRTY        64
#define D_INTERNAL     128
#define D_USER         256

/*==================================================================*/
/* Driver info types                                                */
/*==================================================================*/

typedef struct LW_DRIVER_INFO
{
  const char *ascii_name;
  int id;
} LW_DRIVER_INFO;

extern LW_DRIVER_INFO *gfx_driver;
extern LW_DRIVER_INFO *timer_driver;
extern LW_DRIVER_INFO *keyboard_driver;
extern LW_DRIVER_INFO *mouse_driver;
extern LW_DRIVER_INFO *digi_driver;
extern LW_DRIVER_INFO *midi_driver;
extern LW_DRIVER_INFO *joystick_driver;

/*==================================================================*/
/* Graphics mode constants                                          */
/*==================================================================*/

#define GFX_TEXT                    0
#define GFX_AUTODETECT             1
#define GFX_AUTODETECT_FULLSCREEN  2
#define GFX_AUTODETECT_WINDOWED    3
#define GFX_MODEX                  4
#define GFX_VESA2L                 5
#define GFX_DIRECTX                6
#define GFX_DIRECTX_WIN            7
#define GFX_GP2X                   8

#define COLORCONV_REDUCE_TO_256    1

/*==================================================================*/
/* Sound constants                                                  */
/*==================================================================*/

#define DIGI_AUTODETECT  -1
#define DIGI_NONE         0
#define MIDI_AUTODETECT  -1
#define MIDI_NONE         0

/*==================================================================*/
/* Keyboard constants and state                                     */
/*==================================================================*/

#define KEY_A          SDL_SCANCODE_A
#define KEY_B          SDL_SCANCODE_B
#define KEY_C          SDL_SCANCODE_C
#define KEY_D          SDL_SCANCODE_D
#define KEY_E          SDL_SCANCODE_E
#define KEY_F          SDL_SCANCODE_F
#define KEY_G          SDL_SCANCODE_G
#define KEY_H          SDL_SCANCODE_H
#define KEY_I          SDL_SCANCODE_I
#define KEY_J          SDL_SCANCODE_J
#define KEY_K          SDL_SCANCODE_K
#define KEY_L          SDL_SCANCODE_L
#define KEY_M          SDL_SCANCODE_M
#define KEY_N          SDL_SCANCODE_N
#define KEY_O          SDL_SCANCODE_O
#define KEY_P          SDL_SCANCODE_P
#define KEY_Q          SDL_SCANCODE_Q
#define KEY_R          SDL_SCANCODE_R
#define KEY_S          SDL_SCANCODE_S
#define KEY_T          SDL_SCANCODE_T
#define KEY_U          SDL_SCANCODE_U
#define KEY_V          SDL_SCANCODE_V
#define KEY_W          SDL_SCANCODE_W
#define KEY_X          SDL_SCANCODE_X
#define KEY_Y          SDL_SCANCODE_Y
#define KEY_Z          SDL_SCANCODE_Z
#define KEY_0          SDL_SCANCODE_0
#define KEY_1          SDL_SCANCODE_1
#define KEY_2          SDL_SCANCODE_2
#define KEY_3          SDL_SCANCODE_3
#define KEY_4          SDL_SCANCODE_4
#define KEY_5          SDL_SCANCODE_5
#define KEY_6          SDL_SCANCODE_6
#define KEY_7          SDL_SCANCODE_7
#define KEY_8          SDL_SCANCODE_8
#define KEY_9          SDL_SCANCODE_9
#define KEY_0_PAD      SDL_SCANCODE_KP_0
#define KEY_1_PAD      SDL_SCANCODE_KP_1
#define KEY_2_PAD      SDL_SCANCODE_KP_2
#define KEY_3_PAD      SDL_SCANCODE_KP_3
#define KEY_4_PAD      SDL_SCANCODE_KP_4
#define KEY_5_PAD      SDL_SCANCODE_KP_5
#define KEY_6_PAD      SDL_SCANCODE_KP_6
#define KEY_7_PAD      SDL_SCANCODE_KP_7
#define KEY_8_PAD      SDL_SCANCODE_KP_8
#define KEY_9_PAD      SDL_SCANCODE_KP_9
#define KEY_F1         SDL_SCANCODE_F1
#define KEY_F2         SDL_SCANCODE_F2
#define KEY_F3         SDL_SCANCODE_F3
#define KEY_F4         SDL_SCANCODE_F4
#define KEY_F5         SDL_SCANCODE_F5
#define KEY_F6         SDL_SCANCODE_F6
#define KEY_F7         SDL_SCANCODE_F7
#define KEY_F8         SDL_SCANCODE_F8
#define KEY_F9         SDL_SCANCODE_F9
#define KEY_F10        SDL_SCANCODE_F10
#define KEY_F11        SDL_SCANCODE_F11
#define KEY_F12        SDL_SCANCODE_F12
#define KEY_ESC        SDL_SCANCODE_ESCAPE
#define KEY_TILDE      SDL_SCANCODE_GRAVE
#define KEY_MINUS      SDL_SCANCODE_MINUS
#define KEY_EQUALS     SDL_SCANCODE_EQUALS
#define KEY_BACKSPACE  SDL_SCANCODE_BACKSPACE
#define KEY_TAB        SDL_SCANCODE_TAB
#define KEY_OPENBRACE  SDL_SCANCODE_LEFTBRACKET
#define KEY_CLOSEBRACE SDL_SCANCODE_RIGHTBRACKET
#define KEY_ENTER      SDL_SCANCODE_RETURN
#define KEY_COLON      SDL_SCANCODE_SEMICOLON
#define KEY_QUOTE      SDL_SCANCODE_APOSTROPHE
#define KEY_BACKSLASH  SDL_SCANCODE_BACKSLASH
#define KEY_COMMA      SDL_SCANCODE_COMMA
#define KEY_STOP       SDL_SCANCODE_PERIOD
#define KEY_SLASH      SDL_SCANCODE_SLASH
#define KEY_SPACE      SDL_SCANCODE_SPACE
#define KEY_INSERT     SDL_SCANCODE_INSERT
#define KEY_DEL        SDL_SCANCODE_DELETE
#define KEY_HOME       SDL_SCANCODE_HOME
#define KEY_END        SDL_SCANCODE_END
#define KEY_PGUP       SDL_SCANCODE_PAGEUP
#define KEY_PGDN       SDL_SCANCODE_PAGEDOWN
#define KEY_LEFT       SDL_SCANCODE_LEFT
#define KEY_RIGHT      SDL_SCANCODE_RIGHT
#define KEY_UP         SDL_SCANCODE_UP
#define KEY_DOWN       SDL_SCANCODE_DOWN
#define KEY_SLASH_PAD  SDL_SCANCODE_KP_DIVIDE
#define KEY_ASTERISK   SDL_SCANCODE_KP_MULTIPLY
#define KEY_MINUS_PAD  SDL_SCANCODE_KP_MINUS
#define KEY_PLUS_PAD   SDL_SCANCODE_KP_PLUS
#define KEY_ENTER_PAD  SDL_SCANCODE_KP_ENTER
#define KEY_PRTSCR     SDL_SCANCODE_PRINTSCREEN
#define KEY_PAUSE      SDL_SCANCODE_PAUSE
#define KEY_LSHIFT     SDL_SCANCODE_LSHIFT
#define KEY_RSHIFT     SDL_SCANCODE_RSHIFT
#define KEY_LCONTROL   SDL_SCANCODE_LCTRL
#define KEY_RCONTROL   SDL_SCANCODE_RCTRL
#define KEY_ALT        SDL_SCANCODE_LALT
#define KEY_ALTGR      SDL_SCANCODE_RALT
#define KEY_LWIN       SDL_SCANCODE_LGUI
#define KEY_RWIN       SDL_SCANCODE_RGUI
#define KEY_MENU       SDL_SCANCODE_MENU
#define KEY_SCRLOCK    SDL_SCANCODE_SCROLLLOCK
#define KEY_NUMLOCK    SDL_SCANCODE_NUMLOCKCLEAR
#define KEY_CAPSLOCK   SDL_SCANCODE_CAPSLOCK
#define KEY_MAX        SDL_NUM_SCANCODES

extern volatile char key[KEY_MAX];
extern volatile int mouse_x, mouse_y, mouse_b;

/*==================================================================*/
/* Global screen state                                              */
/*==================================================================*/

extern BITMAP *screen;
extern int SCREEN_W, SCREEN_H, VIRTUAL_H;
extern char *allegro_id;
extern char allegro_error[256];

extern SDL_Window *lw_sdl_window;
extern SDL_Renderer *lw_sdl_renderer;

/*==================================================================*/
/* Timer macros (now no-ops or SDL2 equivalents)                    */
/*==================================================================*/

#define LOCK_FUNCTION(f)   ((void)0)
#define LOCK_VARIABLE(v)   ((void)0)
#define END_OF_FUNCTION(f)
#define MSEC_TO_TIMER(ms)  (ms)

/* Allegro uses BPS_TO_TIMER and SECS_TO_TIMER too */
#define BPS_TO_TIMER(bps)  (1000 / (bps))
#define SECS_TO_TIMER(s)   ((s) * 1000)

/*==================================================================*/
/* Unicode / text format                                            */
/*==================================================================*/

#define U_ASCII  0
#define U_UTF8   1

static inline void set_uformat (int format) { (void) format; }

/*==================================================================*/
/* Initialization and shutdown                                      */
/*==================================================================*/

int allegro_init (void);
void allegro_exit (void);

int install_timer (void);
int install_keyboard (void);
int install_mouse (void);
int install_sound (int digi, int midi, const char *config);
int install_int_ex (void (*handler) (void), int speed);
void remove_int (void (*handler) (void));

void remove_keyboard (void);
void remove_mouse (void);
void remove_sound (void);
void remove_timer (void);

/*==================================================================*/
/* Graphics mode functions                                          */
/*==================================================================*/

int set_gfx_mode (int card, int w, int h, int v_w, int v_h);
void set_color_depth (int depth);
void set_color_conversion (int mode);
void set_palette (PALETTE pal);
void set_window_title (const char *title);
void set_close_button_callback (void (*proc) (void));

/*==================================================================*/
/* Bitmap functions                                                 */
/*==================================================================*/

BITMAP *create_bitmap (int w, int h);
BITMAP *create_bitmap_ex (int bpp, int w, int h);
BITMAP *create_sub_bitmap (BITMAP * parent, int x, int y, int w, int h);
void destroy_bitmap (BITMAP * bmp);
void clear_bitmap (BITMAP * bmp);
void clear_to_color (BITMAP * bmp, int color);
int bitmap_color_depth (BITMAP * bmp);
int is_linear_bitmap (BITMAP * bmp);
int is_memory_bitmap (BITMAP * bmp);

/*==================================================================*/
/* Drawing primitives                                               */
/*==================================================================*/

void putpixel (BITMAP * bmp, int x, int y, int color);
int getpixel (BITMAP * bmp, int x, int y);
void hline (BITMAP * bmp, int x1, int y, int x2, int color);
void vline (BITMAP * bmp, int x, int y1, int y2, int color);
void rect (BITMAP * bmp, int x1, int y1, int x2, int y2, int color);
void rectfill (BITMAP * bmp, int x1, int y1, int x2, int y2, int color);
void blit (BITMAP * src, BITMAP * dst, int sx, int sy, int dx, int dy,
           int w, int h);
void stretch_blit (BITMAP * src, BITMAP * dst, int sx, int sy, int sw,
                   int sh, int dx, int dy, int dw, int dh);
void draw_sprite (BITMAP * bmp, BITMAP * sprite, int x, int y);
void set_clip_rect (BITMAP * bmp, int x1, int y1, int x2, int y2);
void scroll_screen (int x, int y);
int makecol (int r, int g, int b);
int makecol8 (int r, int g, int b);
int save_bitmap (const char *filename, BITMAP * bmp, const PALETTE pal);
void ellipse (BITMAP * bmp, int cx, int cy, int rx, int ry, int color);
void ellipsefill (BITMAP * bmp, int cx, int cy, int rx, int ry, int color);
void line (BITMAP * bmp, int x1, int y1, int x2, int y2, int color);
void polygon (BITMAP * bmp, int vertices, const int *points, int color);
void circlefill (BITMAP * bmp, int cx, int cy, int r, int color);

/*==================================================================*/
/* Text rendering                                                   */
/*==================================================================*/

void textout_ex (BITMAP * bmp, const FONT * f, const char *str,
                 int x, int y, int fg, int bg);
void textout_centre_ex (BITMAP * bmp, const FONT * f, const char *str,
                        int x, int y, int fg, int bg);
int text_height (const FONT * f);
int text_length (const FONT * f, const char *str);

void gui_textout_ex (BITMAP * bmp, const char *str, int x, int y,
                     int fg, int bg, int centre);
BITMAP *gui_get_screen (void);
int gui_mouse_x (void);
int gui_mouse_y (void);

/*==================================================================*/
/* Mouse functions                                                  */
/*==================================================================*/

void show_mouse (BITMAP * bmp);
void scare_mouse (void);
void unscare_mouse (void);
int show_os_cursor (int cursor);
void position_mouse (int x, int y);
void set_mouse_sprite (BITMAP * sprite);

/*==================================================================*/
/* Palette / fade functions                                         */
/*==================================================================*/

void fade_out (int speed);
void fade_in (const PALETTE pal, int speed);
void get_palette (PALETTE pal);
void hsv_to_rgb (float h, float s, float v, int *r, int *g, int *b);
void rgb_to_hsv (int r, int g, int b, float *h, float *s, float *v);
int bestfit_color (const PALETTE pal, int r, int g, int b);

/*==================================================================*/
/* Sound functions                                                  */
/*==================================================================*/

void play_sample (const SAMPLE * spl, int vol, int pan, int freq, int loop);
void stop_sample (const SAMPLE * spl);
void adjust_sample (const SAMPLE * spl, int vol, int pan, int freq, int loop);
int play_midi (MIDI * music, int loop);
void set_volume (int digi_volume, int midi_volume);
void stop_midi (void);
extern int midi_pos;

/*==================================================================*/
/* Joystick functions                                               */
/*==================================================================*/

int install_joystick (int type);
int poll_joystick (void);

#define JOY_TYPE_AUTODETECT  -1

typedef struct JOYSTICK_AXIS_INFO
{
  int pos;
  int d1, d2;
} JOYSTICK_AXIS_INFO;

typedef struct JOYSTICK_STICK_INFO
{
  int flags;
  int num_axis;
  JOYSTICK_AXIS_INFO axis[4];
  const char *name;
} JOYSTICK_STICK_INFO;

typedef struct JOYSTICK_BUTTON_INFO
{
  int b;
  const char *name;
} JOYSTICK_BUTTON_INFO;

typedef struct JOYSTICK_INFO
{
  int flags;
  int num_sticks;
  int num_buttons;
  JOYSTICK_STICK_INFO stick[8];
  JOYSTICK_BUTTON_INFO button[32];
} JOYSTICK_INFO;

extern int num_joysticks;
extern JOYSTICK_INFO joy[8];

/*==================================================================*/
/* Datafile functions                                                */
/*==================================================================*/

DATAFILE *load_datafile_object (const char *filename, const char *objectname);
void unload_datafile_object (DATAFILE * dat);

/*==================================================================*/
/* Dialog functions                                                 */
/*==================================================================*/

int d_button_proc (int msg, DIALOG * d, int c);
int d_text_proc (int msg, DIALOG * d, int c);
int d_ctext_proc (int msg, DIALOG * d, int c);
int d_edit_proc (int msg, DIALOG * d, int c);
int d_list_proc (int msg, DIALOG * d, int c);
int d_slider_proc (int msg, DIALOG * d, int c);
int d_textbox_proc (int msg, DIALOG * d, int c);
int d_clear_proc (int msg, DIALOG * d, int c);
int d_box_proc (int msg, DIALOG * d, int c);
int d_shadow_box_proc (int msg, DIALOG * d, int c);
int d_bitmap_proc (int msg, DIALOG * d, int c);
int d_icon_proc (int msg, DIALOG * d, int c);
int d_keyboard_proc (int msg, DIALOG * d, int c);
int d_check_proc (int msg, DIALOG * d, int c);
int d_radio_proc (int msg, DIALOG * d, int c);
int d_menu_proc (int msg, DIALOG * d, int c);
int d_yield_proc (int msg, DIALOG * d, int c);

DIALOG_PLAYER *init_dialog (DIALOG * d, int focus);
int update_dialog (DIALOG_PLAYER * player);
int shutdown_dialog (DIALOG_PLAYER * player);
int do_dialog (DIALOG * d, int focus);
int popup_dialog (DIALOG * d, int focus);
void broadcast_dialog_message (int msg, int c);

void _draw_scrollable_frame (DIALOG * d, int listsize, int offset,
                             int height, int fg_color, int bg);

/*==================================================================*/
/* Event pump - call once per frame                                 */
/*==================================================================*/

void lw_sdl_pump_events (void);
void lw_sdl_present_screen (void);
FONT *lw_sdl_load_font (int size);

/*==================================================================*/
/* Allegro Unicode string functions (simplified ASCII versions)     */
/*==================================================================*/

static inline int usetc (char *s, int c)
{
  *s = (char) c;
  return 1;
}

static inline int ugetc (const char *s)
{
  return (unsigned char) *s;
}

static inline int ugetx (char **s)
{
  int c = (unsigned char) **s;
  (*s)++;
  return c;
}

static inline int uwidth (const char *s)
{
  (void) s;
  return 1;
}

static inline int ustrlen (const char *s)
{
  return (int) strlen (s);
}

static inline int uisspace (int c)
{
  return isspace (c);
}

static inline int uisok (int c)
{
  return (c >= 0 && c < 256);
}

static inline int uoffset (const char *s, int idx)
{
  (void) s;
  return idx;
}

static inline int ugetat (const char *s, int idx)
{
  return (unsigned char) s[idx];
}

static inline void usetat (char *s, int idx, int c)
{
  s[idx] = (char) c;
}

static inline void uinsert (char *s, int idx, int c)
{
  int len = (int) strlen (s);
  memmove (s + idx + 1, s + idx, len - idx + 1);
  s[idx] = (char) c;
}

static inline void uremove (char *s, int idx)
{
  int len = (int) strlen (s);
  memmove (s + idx, s + idx + 1, len - idx);
}

static inline char *
ustrzcpy (char *dest, int size, const char *src)
{
  strncpy (dest, src, size - 1);
  dest[size - 1] = '\0';
  return dest;
}

/*==================================================================*/
/* Keyboard modifier flags                                          */
/*==================================================================*/

#define KB_SHIFT_FLAG   0x0001
#define KB_CTRL_FLAG    0x0002
#define KB_ALT_FLAG     0x0004
#define KB_LWIN_FLAG    0x0008
#define KB_RWIN_FLAG    0x0010
#define KB_MENU_FLAG    0x0020
#define KB_SCROLOCK_FLAG 0x0100
#define KB_NUMLOCK_FLAG  0x0200
#define KB_CAPSLOCK_FLAG 0x0400
#define KB_NORMAL        0x0000
#define KB_EXTENDED      0x0800

extern volatile int key_shifts;

int keypressed (void);
int readkey (void);
void clear_keybuf (void);

/*==================================================================*/
/* File system functions                                            */
/*==================================================================*/

#define FA_RDONLY  1
#define FA_HIDDEN  2
#define FA_SYSTEM  4
#define FA_LABEL   8
#define FA_DIREC   16
#define FA_ARCH    32

int exists (const char *filename);
int delete_file (const char *filename);
char *fix_filename_case (char *path);
char *fix_filename_slashes (char *path);
int for_each_file_ex (const char *pattern, int attrib, int not_attrib,
                      int (*callback) (const char *filename, int attrib,
                                       void *param), void *param);

BITMAP *load_bitmap (const char *filename, PALETTE pal);
MIDI *load_midi (const char *filename);
SAMPLE *load_sample (const char *filename);

/*==================================================================*/
/* Configuration file functions (Allegro INI-style)                 */
/*==================================================================*/

void set_config_file (const char *filename);
void set_config_string (const char *section, const char *name,
                        const char *val);
void set_config_int (const char *section, const char *name, int val);
const char *get_config_string (const char *section, const char *name,
                               const char *def);
int get_config_int (const char *section, const char *name, int def);

/*==================================================================*/
/* GUI helper variables and functions                               */
/*==================================================================*/

extern int gui_mg_color;
extern int gui_fg_color;
extern int gui_bg_color;

#define MSG_UCHAR  26

int gui_mouse_b (void);
void object_message (DIALOG * d, int msg, int c);
void rest_callback (int ms, void (*callback) (void));

/*==================================================================*/
/* Miscellaneous Allegro macros                                     */
/*==================================================================*/

#define END_OF_MAIN()

#define SYSTEM_NONE 0
#define install_allegro(system, errno_ptr, atexit_ptr) allegro_init()

/* DOS driver list macros (no-ops) */
#define BEGIN_GFX_DRIVER_LIST
#define END_GFX_DRIVER_LIST
#define BEGIN_COLOR_DEPTH_LIST
#define COLOR_DEPTH_8
#define END_COLOR_DEPTH_LIST
#define BEGIN_DIGI_DRIVER_LIST
#define END_DIGI_DRIVER_LIST
#define BEGIN_MIDI_DRIVER_LIST
#define END_MIDI_DRIVER_LIST
#define BEGIN_JOYSTICK_DRIVER_LIST
#define END_JOYSTICK_DRIVER_LIST

static inline void rest (int ms)
{
  SDL_Delay (ms);
}

#endif /* LIQUID_WAR_SDL_COMPAT_H */
