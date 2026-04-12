# Liquid War 5 - Graphics State and Modernization Roadmap

## Executive summary

Liquid War 5 was recently migrated from Allegro 4 to SDL2 (see `SDL2_MIGRATION_SPEC.md`). The migration preserved the original 8-bit indexed rendering pipeline to keep game logic untouched, then added a 32-bit post-processing stage where modern effects can run. The result is a game that **runs on a modern, capable engine but still looks like a late-90s game** — because the rendering pipeline wasn't redesigned, only ported.

This document captures the current state of the graphics pipeline and recommends concrete next steps to modernize the visual fidelity.

---

## Current graphics architecture

### Rendering pipeline (data flow)

```
Game logic (fighter.c, grad.c, mesh.c)
        │
        ▼
  CURRENT_AREA_DISP          ← 8-bit indexed SDL_Surface
  (single-pixel fighters,    ← putpixel() per fighter per frame
   one per fighter)
        │
        ▼
  display_area() / disp.c    ← stretch_blit: nearest-neighbor upscale
        │                       to viewport size
        ▼
  NEXT_SCREEN                ← 8-bit sub-bitmap of `screen`
  (viewport in game window)  ← particles drawn here via putpixel
        │
        ▼
  screen (8-bit indexed)     ← full window surface
        │                    ← UI, dialogs, text blit here
        ▼
  lw_sdl_present_screen()    ← direct palette lookup
        │                       8-bit index → 32-bit ARGB
        ▼
  lw_convert_surface         ← 32-bit ARGB in CPU memory
  (32-bit ARGB)
        │
        ▼
  postfx.c passes            ← liquid ripple (army-only mask)
        │                    ← battle glow (high-threshold edges)
        ▼
  SDL_UpdateTexture          ← upload to persistent streaming texture
        │
        ▼
  SDL_RenderCopy             ← GPU bilinear filter (linear scale quality)
  (with logical size)        ← scales logical 640×480 to window size
        │
        ▼
  SDL_RenderPresent          ← vsync-timed present to window
```

### What's implemented

| Subsystem | Status | File(s) | Notes |
|---|---|---|---|
| Window/renderer | SDL2 | `sdl_compat.c` | Resizable, `SDL_RenderSetLogicalSize` handles mouse/render mapping |
| Palette conversion | CPU manual | `sdl_compat.c:lw_sdl_present_screen` | Per-pixel index→ARGB lookup, 307k pixels/frame |
| Game area rendering | 8-bit nearest | `disp.c:disp_stretch_area` | `stretch_blit` with raw memcpy for equal-size rows |
| Wave distortion (legacy) | Disabled CPU path | `distor.c` | Allegro-era; `CONFIG_WAVE_ON` gate; mostly unused |
| Liquid ripple (new) | 32-bit post-processing | `postfx.c:lw_postfx_liquid_ripple_masked` | Two sine waves, masked to army pixels via palette index ≥ 128 |
| Battle glow | 32-bit post-processing | `postfx.c:lw_postfx_battle_glow` | Additive white-yellow glow at high-diff boundaries, 2px radius, pulsing |
| Particle system | 8-bit viewport drawing | `particles.c` | 2048 particles; SPARK, SPLASH, GLOW, DISSOLVE types |
| Team color gradients | Quadratic ramp | `palette.c:set_team_color` | `t²` from dark to pure saturated base color |
| Text rendering | SDL2_ttf | `sdl_compat.c:textout_ex` | Per-glyph alpha-keyed blit to 8-bit target, fg palette index |
| Menu/dialogs | 8-bit filled rects | `alleg2.c:my_button_proc`, `sdl_compat.c:d_*_proc` | Hover highlight, 3D shadow edges, click targeting by smallest-area match |
| Mouse cursor | OS cursor | `SDL_ShowCursor` | No custom rendered cursor |
| Combat particle triggers | Event-driven | `fighter.c` | DISSOLVE spawns at each fighter capture (3 spots: p0/p1/p2) |
| Asset loading | Direct files | `disk_sdl.c` | PCX/BMP via SDL2_image, WAV via Mix_LoadWAV, MIDI via Mix_LoadMUS |

### What's disabled or stub

- **Allegro wave distortion** (`distor.c`): still compiles but the C-only path is slow and the ASM path is removed. Effectively replaced by the post-processing liquid ripple.
- **ASM fast paths**: all disabled (`ASM=no` in configure). No performance regression since they were only worthwhile on late-90s CPUs.
- **`smooth_army_edges`** (`disp.c`): present but unreachable (`__attribute__((unused))`). Was causing blur.
- **3×3 Gaussian filter in `lw_sdl_present_screen`**: removed after user feedback ("blurry as shit").
- **Motion blur (frame blending)**: removed after user feedback ("persistent soft blur").

### Performance budget

At 640×480 × 60fps the pipeline processes ~18.4M pixels/second in the palette conversion alone, plus ~0.6M pixels/second in masked ripple plus ~0.3M pixels/second in battle glow (only fires near edges). Particles are cheap (≤2048 × a few bytes). Total CPU budget at 60fps on a modern machine is well under 5ms/frame, leaving headroom for more effects.

The GPU is barely used — only for the final `SDL_RenderCopy` bilinear upscale. Everything else happens on CPU.

---

## Visual issues that still make the game look dated

Ranked by impact on perceived quality:

### 1. Armies are flat colored blobs

Each fighter is one pixel with a single palette index per frame. Armies look like painted blobs with hard edges where different teams meet, and flat internal color since fighters of the same team at similar health all use the same index. The quadratic palette ramp gives some depth, but the variance is small because gradient position mostly tracks a narrow health range around the team's peak color.

**Why it looks dated**: modern games have soft army silhouettes, subsurface scattering, depth-of-field cues. Here the army is 1 bpp blobs at native resolution.

### 2. Map backgrounds are static, low-resolution textures

Maps are PCX files rendered once into `CURRENT_AREA_BACK` and never change. The "liquid" theme is barely present in the environment — it's just a bitmap behind the armies.

**Why it looks dated**: environment art in modern games has ambient animation (caustics, flickering, particles) that makes the world feel alive.

### 3. UI is 1-pixel-border rectangles with TTF text

Menu buttons are `rectfill` + `rect` with centered text. No gradients, no shadows, no rounded corners, no easing on hover. The score screen's progress bars are solid fill rectangles.

**Why it looks dated**: every modern game UI has at minimum soft drop shadows, gradient fills, rounded shapes, and animation on state transitions.

### 4. Post-processing is spatial-only and conservative

`lw_postfx_battle_glow` fires at a high color-difference threshold (3000) to avoid washing out colors, so most of the frame has no glow at all. `lw_postfx_liquid_ripple_masked` has 1.5px amplitude which is barely visible except in large army bodies.

**Why it looks dated**: the effects are present but subtle enough that many players won't notice them.

### 5. No camera effects during combat

Nothing reacts to intense combat — no screen shake, no zoom, no vignette, no chromatic aberration, no flash on major territory changes.

**Why it looks dated**: even minimalist modern games use camera-level feedback for moment-to-moment intensity.

### 6. Particles are small and short-lived

Single-pixel dissolve particles that fade quickly. Readable and tasteful but not showy.

### 7. Fixed 640×480 logical resolution

Everything is authored for 640×480. Resizing the window just bilinear-upscales the same 640×480 frame. No resolution-independent UI, no vector shapes, no retina-class assets.

### 8. No lighting or shading

The rendering has no concept of light direction, surface normals, or ambient occlusion. Everything is lit flat.

---

## Modernization roadmap

Organized by effort (hours/days/weeks) and impact (subtle/noticeable/transformative).

### Tier 1: Quick wins (hours, noticeable impact)

These are small additions within the existing pipeline that immediately raise fidelity.

#### 1.1 Animated cursor trails
When a player cursor moves, drop a fading GLOW particle at each step. Gives the cursor a comet-like trail that makes movement feel snappy.
- **Files**: `cursor.c` (where positions update), `particles.c`
- **Effort**: 1-2 hours
- **Risk**: low; particles already exist

#### 1.2 Screen shake on territory shifts
When `ACTIVE_FIGHTERS[team]` drops by >5% in one frame, shake the screen for 200ms by offsetting the `SDL_RenderCopy` destination rect.
- **Files**: `sdl_compat.c:lw_sdl_present_screen`, `game.c` (emit shake events)
- **Effort**: 2-3 hours
- **Risk**: low; cosmetic only

#### 1.3 Radial gradient vignette
Darken screen corners in the 32-bit post-processing pass. Applied multiplicatively, ~70% brightness at corners, 100% in center. Classic cinematic technique that makes cheap art look better.
- **Files**: `postfx.c` (new function), `sdl_compat.c` (call it)
- **Effort**: 1 hour
- **Risk**: zero; pure post-process

#### 1.4 Soft drop shadows on UI elements
In `my_button_proc`, draw a 2-3px offset dark rectangle before the button fill. Do the same for text via a second `textout_ex` call at offset +1,+1 with color 0.
- **Files**: `alleg2.c:my_button_proc`, `sdl_compat.c:textout_ex` (optional helper)
- **Effort**: 2 hours
- **Risk**: low

#### 1.5 Button hover animation
Interpolate the hover highlight color over 100-150ms instead of snapping to it. Store per-button animation state in the DIALOG's `d1`/`d2` fields.
- **Files**: `alleg2.c:my_button_proc`, `sdl_compat.c:update_dialog`
- **Effort**: 2-3 hours
- **Risk**: low

#### 1.6 More dissolve particles
Bump spawn count from 2 per capture to 4-6 at p0 (direct hit), add a small spread variance. Cheap since the pool is 2048.
- **Files**: `fighter.c`
- **Effort**: 30 minutes
- **Risk**: zero

### Tier 2: Meaningful upgrades (days, transformative impact)

These require more work but significantly change how the game feels.

#### 2.1 Dual-resolution rendering: 2× or 4× supersampled game area
Render `CURRENT_AREA_DISP` at 2× its current dimensions, with each fighter occupying a 2×2 block. When stretched to the viewport, army boundaries have natural anti-aliasing from the supersample.
- **Files**: `area.c`, `fighter.c`, `game.c`, `disp.c`
- **Effort**: 1-2 days
- **Risk**: medium; doubles memory for the game area and halves fighter density per screen pixel unless you rescale the map too. Alternative: keep the game area resolution but composite it at 2× in a new 32-bit intermediate buffer.
- **Payoff**: transformative — army edges become soft and liquid-like instead of pixel-chunky

#### 2.2 Caustic map background animation
The map backgrounds are static. Generate a procedural animated caustic pattern (two sine-based overlapping grids) and blend it at low opacity over the non-playable map areas in the post-processing stage. Makes the "water" theme come alive.
- **Files**: `postfx.c` (new function), `sdl_compat.c` (call + mask by palette index < 64)
- **Effort**: 4-6 hours
- **Risk**: low; purely post-process
- **Payoff**: environment feels alive instead of painted

#### 2.3 Real-time 32-bit game area compositing
Right now `lw_sdl_present_screen` converts the entire 8-bit screen to 32-bit via direct lookup. Instead:
- Keep the 8-bit screen surface for UI only.
- Render the game area directly into the 32-bit `lw_convert_surface` with full alpha-blended fighters.
- Composite UI layer over the top.

This decouples game rendering from palette indices and lets you use any color, do alpha blending for soft army edges, and apply effects per-element.
- **Files**: `disp.c`, `sdl_compat.c:lw_sdl_present_screen`, `fighter.c`
- **Effort**: 2-3 days
- **Risk**: medium-high; game logic reads `CURRENT_AREA_DISP` palette indices in some places (check `grad.c`, `decal.c`)
- **Payoff**: all downstream effects become easier and look better; the 8-bit pipeline stops limiting visual quality

#### 2.4 Depth-shaded armies with normal-based lighting
With 32-bit compositing in place (#2.3), compute a virtual height field over each army body (e.g., distance from the boundary), shade it with a fake directional light (top-left by convention). Armies look like 3D liquid mounds instead of flat blobs.
- **Files**: new `src/shading.c`, `postfx.c`
- **Effort**: 2-3 days
- **Depends on**: 2.3
- **Payoff**: transformative — this alone will make screenshots look modern

#### 2.5 Better dialog theme: gradients, rounded corners, typography
Rewrite `my_button_proc` to use a 32-bit render target for UI. Buttons get:
- Vertical gradient fill (light to dark)
- 1px rounded corner approximation via corner pixel masking
- Drop shadow + inner highlight
- Larger TTF font with proper anti-aliasing
- Icons next to labels via SDL2_image PNG loading

Modernizes the menu look without changing layouts.
- **Files**: `alleg2.c`, `sdl_compat.c`, `dialog.c`, new icon assets
- **Effort**: 3-4 days
- **Risk**: medium; touches every menu
- **Payoff**: menus no longer feel like a 90s shareware game

### Tier 3: Engine-level upgrades (weeks, transformative impact)

These require structural changes but unlock modern rendering techniques.

#### 3.1 OpenGL/GPU rendering path via SDL_Renderer + custom shaders
Create an `SDL_Texture` per army team with `SDL_TEXTUREACCESS_STREAMING`, write army density into it each frame, then use `SDL_RenderGeometry` + a fragment shader to blend with smooth edges, add rim lighting, glow, caustics — all on GPU.
- **Files**: new `src/gpu_render.c`, major changes to `sdl_compat.c` presentation path
- **Effort**: 1-2 weeks
- **Risk**: high; SDL2's shader support is limited. Consider skipping to #3.2 directly.
- **Payoff**: frees the CPU; opens the door to all post-processing techniques

#### 3.2 Switch to SDL3 or direct OpenGL/Vulkan for the presentation layer
SDL3 has proper render pipelines and shader support. Alternatively, keep SDL2 for window/input but use OpenGL directly for rendering. Then you can write actual fragment shaders for:
- Gaussian bloom
- Chromatic aberration
- Film grain
- Proper HDR tone mapping
- Screen-space reflections (not useful here but possible)
- GPU-accelerated distortion
- Particle rendering on the GPU

- **Effort**: 2-3 weeks initial migration, then each effect is ~1 day
- **Risk**: high; another build/distribution change
- **Payoff**: modern AAA-style post-processing becomes feasible

#### 3.3 Procedural animated environments
Replace static map backgrounds with procedurally animated ones: flowing water, rippling surfaces, particle-based mist, depth fog. With a GPU path this is essentially free per frame.
- **Depends on**: 3.1 or 3.2
- **Effort**: 1 week per environment style
- **Payoff**: the game world feels like a living body of water rather than a painted backdrop

#### 3.4 Dynamic lighting with army self-glow
Each team's brightest fighters emit light onto the map. The map background gets lit according to where armies are. Bright battle lines cast warm glow onto nearby terrain.
- **Depends on**: 3.1 or 3.2
- **Effort**: 3-5 days
- **Payoff**: ties armies and environment together visually

#### 3.5 Resolution-independent vector UI
Ship SDL_ttf fonts at high resolution plus vector-style UI elements (shapes composed of gradients and anti-aliased edges) that render to any output resolution sharply. Means the game scales gracefully from 640×480 to 4K.
- **Effort**: 2 weeks
- **Payoff**: the UI looks as sharp on a 4K monitor as it does at native res

---

## Recommended next steps

A pragmatic sequence that maximizes visual return on engineering effort:

1. **Do Tier 1 items 1.3, 1.4, 1.5** in one pass (~1 day total). These are almost free and immediately make the UI feel less dated.

2. **Tier 2 item 2.2 (caustic map backgrounds)** (half a day). Single biggest "wow" per hour of work — makes the environment feel alive.

3. **Tier 2 item 2.3 (32-bit game area compositing)** (2-3 days). This is the pivotal architectural change. Every subsequent effect becomes easier once the game area is no longer locked to 8-bit indices.

4. **Tier 2 item 2.4 (depth-shaded armies)** (2-3 days). With 2.3 in place, this is the single most visually transformative change — armies become 3D-looking liquid pools with actual form.

5. **Tier 1 item 1.1 (cursor trails) + 1.2 (screen shake)** (half a day). Now that the foundation is better, small juice effects read clearly.

6. **Tier 2 item 2.5 (modern dialog theme)** (3-4 days). The menus have been the weakest part since the migration. A proper theme pass brings them up to the rest of the game.

**Total for this plan: ~2 weeks of work** for a dramatic visual upgrade without rewriting the engine. After this, the game would no longer look like a 90s port — it would look like a modern indie title using the classic Liquid War gameplay.

Tier 3 items only become worthwhile if you want to push further (e.g., for a commercial release or a Steam launch).

---

## Guardrails

Lessons from the recent migration work that any future graphics changes should respect:

- **Don't blur UI/text**. The user rejects any global smoothing that softens menus. Keep effects masked to the game viewport or to specific palette ranges.
- **Don't overwrite palette entries 0-17 during gameplay**. Menu colors must stay valid.
- **Mouse clicks must be edge-triggered**. No blocking wait loops in `update_dialog`.
- **Performance is not yet a problem, but watch for it**. Full-screen per-pixel passes should stay under 2-3ms each.
- **Screenshot before and after every visual change**. The user's aesthetic preferences are specific and don't always match textbook graphics theory — iterate visually.
- **Keep the 8-bit pipeline working until 2.3 is done**. Some game logic reads palette indices back from the game area bitmap.
