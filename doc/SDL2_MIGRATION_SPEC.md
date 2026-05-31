# Liquid War 5 - Allegro 4 to SDL2 Migration Specification

## Overview

Migrate Liquid War 5's graphics, input, audio, and timing subsystems from
Allegro 4 to SDL2 to enable modern rendering capabilities including true-color
output, hardware-accelerated blitting, alpha blending, texture filtering, and
GPU shader support.

## Goals

1. Replace Allegro 4 dependency with SDL2 + SDL2_image + SDL2_mixer + SDL2_ttf
2. Move from 256-color indexed palette to 32-bit RGBA rendering
3. Use hardware-accelerated SDL_Renderer for all blitting/scaling
4. Preserve all existing game logic (mesh, gradient spreading, fighters) unchanged
5. Maintain the wave distortion effect with improved visual quality
6. Keep the game compilable on Linux, Windows, and macOS

## Non-Goals

- Rewriting game logic (mesh, AI, networking, config)
- Adding 3D rendering or OpenGL shaders (future phase)
- Changing the map format or data file structure

---

## Architecture

### Current State (Allegro 4)

```
init.c          -> allegro_init(), install_keyboard/mouse/timer/sound
gfxmode.c       -> set_gfx_mode() with platform-specific drivers
viewport.c      -> page flipping via sub-bitmaps of screen, scroll_screen()
disp.c          -> stretch_blit() game area to NEXT_SCREEN
distor.c        -> CPU wave distortion with ASM fast path (putpixel loop)
grad.c          -> create_gradient_bitmap() via putpixel per mesh cell
palette.c       -> 256-color palette with per-team color ranges
alleg2.c        -> custom GUI dialog procs wrapping Allegro dialog system
disk.c          -> load_datafile_object() for .dat asset bundles
sound.c/music.c -> Allegro SAMPLE/MIDI playback
ticker.c        -> install_int_ex() interrupt-driven timing
```

### Target State (SDL2)

```
init.c          -> SDL_Init(), SDL_CreateWindow(), SDL_CreateRenderer()
gfxmode.c       -> SDL_SetWindowSize/Fullscreen
viewport.c      -> SDL_Texture render targets, SDL_RenderPresent()
disp.c          -> SDL_RenderCopy() with SDL_Texture
distor.c        -> SDL surface pixel manipulation + SDL_CreateTextureFromSurface
grad.c          -> write to SDL_Surface pixels, convert to texture
palette.c       -> direct RGBA color computation (no palette indirection)
alleg2.c        -> replaced with custom SDL2 UI rendering
disk.c          -> SDL2_image for image loading, custom .dat loader or migration
sound.c/music.c -> SDL2_mixer for WAV/OGG + MIDI
ticker.c        -> SDL_GetTicks() / SDL_AddTimer()
```

---

## Migration Phases

### Phase 1: Abstraction Layer + Build System

Create a thin compatibility layer (`src/sdl_compat.h`) that maps Allegro
concepts to SDL2, allowing incremental migration.

**Key type mappings:**
| Allegro 4              | SDL2 Equivalent                     |
|------------------------|-------------------------------------|
| `BITMAP *`             | `SDL_Surface *` or `SDL_Texture *`  |
| `PALETTE` / `RGB`      | Direct `Uint32` RGBA colors         |
| `screen`               | `SDL_Renderer *` + `SDL_Window *`   |
| `FONT *`               | `TTF_Font *`                        |
| `SAMPLE *`             | `Mix_Chunk *`                       |
| `MIDI *`               | `Mix_Music *`                       |
| `DATAFILE *`           | Individual file loading              |
| `DIALOG`               | Custom SDL2 UI (no equivalent)      |

**Build system changes:**
- Update `configure.ac`: replace `allegro-config` with `sdl2-config` / `pkg-config`
- Link: `-lSDL2 -lSDL2_image -lSDL2_mixer -lSDL2_ttf`
- Remove Allegro dat compiler dependency

**Files modified:** `configure.ac`, `Makefile.in`, new `src/sdl_compat.h`

### Phase 2: Core Initialization + Window Management

Replace Allegro init/shutdown with SDL2.

**`src/init.c` changes:**
- `allegro_init()` -> `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)`
- Create `SDL_Window` + `SDL_Renderer` (with `SDL_RENDERER_ACCELERATED`)
- `set_color_depth(8)` -> removed (SDL2 defaults to 32-bit)
- `install_keyboard()` -> handled by SDL_Init (SDL_INIT_EVENTS)
- `install_mouse()` -> handled by SDL_Init
- `install_timer()` -> `SDL_INIT_TIMER`
- `install_sound()` -> `Mix_OpenAudio()`
- `set_close_button_callback()` -> `SDL_WINDOWEVENT_CLOSE` event handling

**`src/gfxmode.c` changes:**
- `set_gfx_mode()` -> `SDL_SetWindowSize()` + `SDL_SetWindowFullscreen()`
- Remove platform-specific driver selection (DOS, DirectX, GP2X)
- `SCREEN_W/SCREEN_H` -> query from `SDL_GetWindowSize()`

**`src/exit.c` changes:**
- Cleanup: `SDL_DestroyRenderer()`, `SDL_DestroyWindow()`, `SDL_Quit()`

### Phase 3: Bitmap Operations + Rendering Pipeline

This is the largest phase. Replace all BITMAP operations.

**New global state (replaces `screen` + `NEXT_SCREEN`):**
```c
SDL_Window   *LW_WINDOW;
SDL_Renderer *LW_RENDERER;
SDL_Texture  *LW_GAME_TEXTURE;   // replaces NEXT_SCREEN
```

**`src/viewport.c` changes:**
- Remove page flipping entirely (SDL2 handles vsync/present)
- `page_flip()` -> `SDL_RenderPresent(LW_RENDERER)`
- `NEXT_SCREEN = create_sub_bitmap(screen, ...)` -> `SDL_CreateTexture()` as render target
- `scroll_screen()` -> removed
- `set_clip_rect()` -> `SDL_RenderSetClipRect()`

**`src/disp.c` changes:**
- `stretch_blit(src, dst, ...)` -> `SDL_RenderCopy()` with `SDL_Rect` src/dst
- Game area rendered to an `SDL_Texture` render target, then scaled to window

**Drawing primitives (throughout codebase):**
- `putpixel(bmp, x, y, color)` -> direct `SDL_Surface` pixel access via `pixels` pointer
- `getpixel(bmp, x, y)` -> direct pixel read
- `rectfill()` -> `SDL_FillRect()` on surface or `SDL_RenderFillRect()`
- `rect()` -> `SDL_RenderDrawRect()`
- `hline()/vline()` -> `SDL_RenderDrawLine()`
- `draw_sprite()` -> `SDL_RenderCopy()` with alpha
- `clear_bitmap()` -> `SDL_FillRect()` with 0 or `SDL_RenderClear()`
- `blit()` -> `SDL_BlitSurface()` or `SDL_RenderCopy()`

**`src/alleg2.c` overhaul:**
- `my_create_bitmap()` -> `SDL_CreateRGBSurfaceWithFormat()` (32-bit ARGB)
- Remove Allegro DIALOG system entirely
- Reimplement menu/UI with direct SDL2 rendering calls

### Phase 4: Color System Overhaul

Move from indexed 256-color to direct RGBA.

**`src/palette.c` changes:**
- Remove `PALETTE`, `RGB` types
- Replace `set_palette()` with no-op (colors are direct now)
- `COLOR_FIRST_ENTRY[team]` + offset -> precomputed `Uint32 TEAM_COLORS[team][gradient_level]`
- `COLORS_PER_TEAM` gradients -> RGBA gradient lookup table per team
- `fade_in()/fade_out()` -> render full screen with decreasing/increasing alpha overlay

**`src/grad.c` changes:**
- `create_gradient_bitmap()`:
  - Create `SDL_Surface` (32-bit)
  - Lock surface, write pixels directly (much faster than putpixel)
  - Map gradient values to RGBA via `TEAM_COLORS[team][grad_level]`
  - Return surface (caller converts to texture)

**Visual improvement opportunity:** With true-color, gradient transitions
between armies can be smooth rather than stepping through 256-color bands.

### Phase 5: Wave Distortion Effect

Reimplement the wave distortion without ASM dependency.

**`src/distor.c` changes:**
- Remove all `#ifdef ASM` code paths and `draw_distor_line()` ASM function
- `disp_distorted_area()`:
  - Lock source `SDL_Surface` (`CURRENT_AREA_DISP`)
  - Create destination `SDL_Surface` (`DISTORSION_TARGET`)
  - Perform pixel-level distortion using direct pointer arithmetic on `surface->pixels`
  - Convert result to `SDL_Texture` via `SDL_CreateTextureFromSurface()`
  - Render with `SDL_RenderCopy()`
- `fixsqrt()` / `fixsin()` / `fixcos()` -> standard `<math.h>` `sqrt()`, `sin()`, `cos()`
  with double precision (modern CPUs handle this fine)

**`src/glouglou.h` / `src/asm.c`:**
- Remove entirely (ASM bitmap manipulation no longer needed)

### Phase 6: Text Rendering

Replace Allegro font system with SDL2_ttf.

**Changes:**
- `FONT *`, `BIG_FONT *`, `SMALL_FONT *` -> `TTF_Font *` at different sizes
- `textout_ex()` -> `TTF_RenderText_Blended()` + `SDL_RenderCopy()`
- `text_height()` -> `TTF_FontHeight()`
- `text_width()` -> `TTF_SizeText()`
- Bundle a TTF font file (e.g., a libre pixel font) or convert existing bitmap font

### Phase 7: Input System

Replace Allegro input with SDL2 event polling.

**`src/keyboard.c` changes:**
- `key[]` array -> SDL event loop with `SDL_KEYDOWN/SDL_KEYUP`, maintain own key state array
- `KEY_*` constants -> `SDL_SCANCODE_*` or `SDLK_*`

**`src/mouse.c` changes:**
- `mouse_x`, `mouse_y` -> `SDL_GetMouseState(&x, &y)`
- `show_mouse()/scare_mouse()` -> `SDL_ShowCursor()`

**`src/joystick.c` changes:**
- `install_joystick()` -> `SDL_JoystickOpen()` / `SDL_GameControllerOpen()`

### Phase 8: Audio

Replace Allegro sound with SDL2_mixer.

**`src/sound.c` + `src/music.c` changes:**
- `install_sound()` -> `Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048)`
- `SAMPLE *` -> `Mix_Chunk *`
- `play_sample()` -> `Mix_PlayChannel()`
- `MIDI *` -> `Mix_Music *` (convert MIDI to OGG/MP3 or use SDL2_mixer MIDI support)
- Remove `LOCK_FUNCTION()`, `LOCK_VARIABLE()`, `END_OF_FUNCTION()` (DOS relics)

### Phase 9: Timing

Replace Allegro timer with SDL2 timing.

**`src/ticker.c` changes:**
- `install_int_ex()` -> `SDL_AddTimer()` or poll-based with `SDL_GetTicks()`
- `TICKER_VALUE` -> `SDL_GetTicks()` (millisecond resolution)
- `MSEC_TO_TIMER()` -> direct millisecond values
- Remove `LOCK_FUNCTION/LOCK_VARIABLE` interrupt safety markers
- Game loop: use `SDL_GetTicks()` for delta timing or fixed timestep

### Phase 10: Asset Loading

Replace Allegro datafile system.

**`src/disk.c` changes:**
- `load_datafile_object()` -> individual file loading:
  - Images: `IMG_Load()` (SDL2_image) from PNG/BMP files
  - Sounds: `Mix_LoadWAV()` from WAV/OGG files
  - Music: `Mix_LoadMUS()` from OGG/MIDI files
  - Fonts: `TTF_OpenFont()` from TTF files
- Extract existing `.dat` contents to individual files (one-time conversion)
- Or write a minimal `.dat` reader that produces `SDL_Surface *` etc.

---

## File Impact Summary

| File          | Change Level | Description                              |
|---------------|-------------|------------------------------------------|
| `configure.ac`| Heavy       | SDL2 detection replaces Allegro          |
| `Makefile.in` | Medium      | Link flags, remove dat compiler          |
| `init.c`      | Heavy       | Full rewrite of init sequence            |
| `exit.c`      | Medium      | SDL2 cleanup                             |
| `gfxmode.c`   | Heavy       | Window/resolution management             |
| `viewport.c`  | Heavy       | Render target + present replaces flipping|
| `disp.c`      | Medium      | SDL_RenderCopy replaces stretch_blit     |
| `distor.c`    | Heavy       | Surface pixel manipulation, remove ASM   |
| `grad.c`      | Medium      | Surface pixel writes, RGBA colors        |
| `palette.c`   | Heavy       | Complete rewrite to RGBA color system    |
| `alleg2.c`    | Heavy       | Remove Allegro dialog, rewrite UI        |
| `disk.c`      | Heavy       | New asset loading pipeline               |
| `sound.c`     | Medium      | SDL2_mixer API                           |
| `music.c`     | Medium      | SDL2_mixer API                           |
| `ticker.c`    | Medium      | SDL_GetTicks/SDL_AddTimer                |
| `keyboard.c`  | Medium      | SDL event-based input                    |
| `mouse.c`     | Light       | SDL_GetMouseState                        |
| `joystick.c`  | Medium      | SDL_GameController                       |
| `glouglou.h`  | Remove      | ASM distortion no longer needed          |
| `asm.c`       | Remove      | ASM struct checks no longer needed       |
| `capture.c`   | Light       | SDL_SaveBMP or IMG_SavePNG               |

**New files:**
- `src/sdl_compat.h` - compatibility typedefs and wrapper macros
- `src/sdl_render.c` - centralized renderer state and helpers

---

## Visual Improvements Enabled by Migration

Once on SDL2, these enhancements become straightforward:

1. **Smooth gradient transitions** - true-color eliminates palette banding
2. **Bilinear scaling** - `SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear")`
3. **Alpha-blended army boundaries** - soft edges where armies meet
4. **Fullscreen at any resolution** - SDL2 logical rendering handles scaling
5. **VSync** - `SDL_RENDERER_PRESENTVSYNC` for tear-free display
6. **Future: GPU shaders** - SDL2 + OpenGL context for bloom, glow effects

---

## Risk Assessment

| Risk                                    | Mitigation                                    |
|-----------------------------------------|-----------------------------------------------|
| Allegro dialog system has no SDL2 equiv | Rewrite menus with direct SDL2 rendering      |
| .dat datafile format is Allegro-specific| Extract assets to individual files             |
| ASM code assumes Allegro BITMAP layout  | Remove ASM paths, rely on compiler optimization|
| 256-color game logic tied to palette    | Build RGBA lookup tables matching old palette  |
| Fixed-point math (fixsqrt etc.)         | Replace with standard math.h floating point    |
| DOS/GP2X platform support               | Drop (already non-functional/obsolete)         |

---

## Dependencies

```
SDL2        >= 2.0.20
SDL2_image  >= 2.0.5
SDL2_mixer  >= 2.0.4
SDL2_ttf    >= 2.0.18
```

## Migration Order

Recommended order to minimize breakage (each phase should produce a compilable state):

1. Phase 1 (build system + compat header)
2. Phase 2 (init/shutdown + window)
3. Phase 9 (timing - needed early for game loop)
4. Phase 7 (input - needed for any interaction)
5. Phase 4 (color system - foundation for rendering)
6. Phase 3 (bitmap ops + rendering pipeline)
7. Phase 5 (wave distortion)
8. Phase 6 (text rendering)
9. Phase 8 (audio)
10. Phase 10 (asset loading)
