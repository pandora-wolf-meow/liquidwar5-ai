# Tier 3 Execution Plan

Engine-level upgrades that unlock modern rendering techniques. This tier is **weeks of work** and should only be started when Tier 2 is complete and there's concrete motivation (commercial release, Steam launch, ambitious visual overhaul).

## Prerequisites

- Tier 1 complete (see `TIERS_STATUS.md`)
- Tier 2 complete, especially **2.3 32-bit game area compositing** — Tier 3 assumes the game area is rendered in 32-bit so shaders can consume it as a texture.
- A working 60fps baseline on commodity hardware with all Tier 2 effects enabled.
- Build system flexibility to link new libraries (OpenGL, SDL3, or both).

## Why Tier 3 is a separate tier

Tier 1 and Tier 2 work within the existing CPU-driven post-processing pipeline. Tier 3 moves work to the GPU via shader-based rendering. This is a qualitative jump: instead of writing effects as nested pixel loops in C, you write them as fragment shaders in GLSL. Effects that are impractical on CPU (Gaussian bloom, proper HDR tone mapping, screen-space distortion, depth of field) become cheap. In exchange, the build becomes more complex, distribution adds GPU driver requirements, and debugging is harder.

## Execution order

Tier 3 items have strong dependencies: **3.1 or 3.2 must happen first**, then 3.3/3.4/3.5 become tractable. The two "gate" options are:

### Step 1 — Tier 3.1: GPU rendering via SDL_Renderer geometry + limited shaders (1-2 weeks)

**What:** Use SDL2's existing `SDL_Renderer` with `SDL_RenderGeometry` to upload per-fighter or per-team geometry, and use custom fragment shaders where possible (SDL2's shader support is limited — mostly through `SDL_GL_BindTexture` workarounds or metal shaders on macOS).

**Reality check:** SDL2 is not designed for custom shader pipelines. You'd be fighting the library. This step is documented here for completeness but **Step 2 is probably the better path** unless you have a specific reason to stay on SDL2.

**Kickoff prompt:**
> Investigate SDL2's options for custom fragment shaders via `SDL_RenderGeometry` and direct OpenGL/Metal context access. Prototype a single effect (Gaussian bloom) to confirm whether the SDL2 path is viable. If the prototype works, proceed with a full GPU migration on SDL2. If it hits fundamental limits, document the blockers and recommend Tier 3.2 instead.

**Decision point:** After the prototype, decide between 3.1 (stay on SDL2, work around limits) or 3.2 (migrate to SDL3 or direct GL). Going forward, this doc assumes 3.2 is chosen.

### Step 2 — Tier 3.2: SDL3 or direct OpenGL/Vulkan migration (2-3 weeks)

**What:** Replace the SDL2 presentation layer with either SDL3 (which has proper GPU pipelines via `SDL_GPUShader`) or with direct OpenGL 3.3+ / Vulkan. Keep SDL2 for window/input/audio if migrating to GL/Vulkan, or go all-SDL3.

**Decision needed at start:**

- **Option A: SDL3 full migration.** Everything becomes SDL3. Gets the new GPU API. Downside: SDL3 is newer, less mature documentation, some extensions may not work on every platform. Build scripts have to detect SDL3.
- **Option B: SDL2 + direct OpenGL.** Keep SDL2 for window/input/audio. Use `SDL_GL_CreateContext`. Write OpenGL 3.3 core profile code for rendering. Most flexible, most portable.
- **Option C: SDL2 + Vulkan.** Maximum performance ceiling but major complexity. Not recommended for a 2D game.

**Recommendation:** Option B (SDL2 + OpenGL). Stays on a stable library, leverages the compatibility layer work, and OpenGL 3.3 is universally supported.

**Files affected (Option B):**
- `src/sdl_compat.c:lw_sdl_present_screen` — replace `SDL_RenderCopy` path with GL texture upload + shader draw call
- New `src/gl_render.c` / `.h` — GL context setup, shader compilation, uniform management, VBO/VAO setup
- New `shaders/` directory — GLSL source files for each effect (base display, vignette, bloom, chromatic aberration, etc.)
- `configure.ac` and `src/Makefile.in` — link `-lGL` or `-lGLEW`
- `src/postfx.c` — existing CPU post-processing becomes legacy; shader versions take over

**Subtasks (one commit per subtask):**
1. Wire up an OpenGL context via `SDL_GL_CreateContext` and `SDL_GL_SetSwapInterval(1)`.
2. Write a minimal shader pair (vertex passthrough, fragment texture sample) that displays the 32-bit frame via a full-screen quad. Verify parity with the SDL_RenderCopy path.
3. Port `lw_postfx_vignette` to a fragment shader. Remove the CPU version.
4. Port `lw_postfx_liquid_ripple_masked` to a shader using a displacement map or direct sine computation. Remove CPU version.
5. Port `lw_postfx_battle_glow` to a shader (Gaussian blur + additive blend).
6. Add new effects that were impractical on CPU: bloom, chromatic aberration, film grain, HDR tone mapping.
7. Delete `src/postfx.c` CPU implementation once all effects have shader equivalents.

**Risk:** high. Multi-week project. Budget double the estimate for unexpected portability issues.

**Kickoff prompt:**
> Implement Tier 3.2 from `doc/GRAPHICS_STATE_AND_ROADMAP.md` Option B — SDL2 + direct OpenGL 3.3 core profile. Start by adding the GL context via `SDL_GL_CreateContext`, writing a minimal pair of shaders to display the existing frame buffer as a textured quad, and confirming visual parity with the current `SDL_RenderCopy` path. Commit that as `[tier3] wire up opengl context`. Then port effects one by one as listed in `TIER3_PLAN.md` Step 2. After each port, run `graphics-reviewer` and `perf-profiler` to verify.

### Step 3 — Tier 3.3: Procedural animated environments (1 week per style)

**Depends on:** 3.2.

**What:** Replace static map backgrounds with fragment-shader-driven procedural environments. With a GPU path this is essentially free per frame.

**Techniques to implement:**
- **Flowing water:** layered sine waves + procedural noise offset + caustic highlights (similar to Tier 2.2 but richer)
- **Rippling surface:** normal-mapped fake reflections of an off-screen gradient
- **Depth fog:** distance-based alpha blend toward a fog color
- **Animated mist particles:** GPU particle system for ambient atmosphere

**Files:**
- New `shaders/env_water.frag`, `shaders/env_mist.frag`, etc.
- `src/gl_render.c` — environment pass dispatch
- `src/maptex.c` — mark maps as "environment-enabled" so the shader knows what to draw

**Risk:** medium. Each environment style is self-contained.

**Kickoff prompt:**
> Implement Tier 3.3 from `doc/GRAPHICS_STATE_AND_ROADMAP.md` — procedural animated environments. Start with a flowing water shader for maps that have a "water" theme (check `maptex.c` for metadata). Commit each environment style as its own `[tier3-env]` commit.

### Step 4 — Tier 3.4: Dynamic lighting with army self-glow (3-5 days)

**Depends on:** 3.2.

**What:** Each army's brightest (highest-health) fighters emit light onto the surrounding map. The map background gets lit according to where armies are. Battle lines cast warm glow onto nearby terrain.

**Technique:** Render armies to a light accumulation texture (sum of emissive * falloff), then sample it in the environment shader and add to the base color.

**Files:**
- New shader `shaders/light_accum.frag` and `shaders/light_apply.frag`
- `src/gl_render.c` — two-pass lighting path (accumulate, then apply)

**Risk:** medium. Performance depends on accumulation texture resolution; start at 1/2 resolution and scale up.

**Kickoff prompt:**
> Implement Tier 3.4 — dynamic lighting. Render armies into a low-res light accumulation texture, then sample it in the environment shader to light the terrain. Confirm the battle lines produce visible warm lighting on nearby map pixels. Run `perf-profiler` to measure the lighting pass cost.

### Step 5 — Tier 3.5: Resolution-independent vector UI (2 weeks)

**Depends on:** 3.2. Parallel-safe with 3.3/3.4.

**What:** Ship the UI as vector-style shaders that render crisp at any resolution from 640×480 to 4K, instead of 8-bit palette-indexed rectangles. Includes SDF (signed distance field) text rendering for proper typography.

**Techniques:**
- SDF-based button shapes (rounded rect with any corner radius, crisp at all scales)
- SDF text rendering via pre-baked glyph atlases
- Vector gradient fills computed in the shader
- Anti-aliased edges automatically via SDF falloff

**Files:**
- New `shaders/sdf_shape.frag`, `shaders/sdf_text.frag`
- New `src/sdf_glyph_atlas.c` — offline glyph atlas generation or runtime via SDL2_ttf + SDF computation
- `src/alleg2.c` — `my_button_proc` and friends rewritten to emit geometry + SDF draw calls instead of raster blits

**Risk:** high. SDF text rendering is a significant engineering effort on its own. Consider using an existing library like msdfgen for the atlas generation.

**Kickoff prompt:**
> Implement Tier 3.5 — resolution-independent vector UI. Start with SDF button shapes (no text yet) as a proof of concept. Build an SDF rendering pipeline in `src/gl_render.c` that consumes normalized quad coordinates and a distance field. Once buttons look crisp at 4K, tackle SDF text rendering via a glyph atlas generated from the existing TTF font. Use `ui-designer` agent for the dialog rewrites.

## Cross-cutting concerns

### Build system impact

Tier 3.2 requires OpenGL (or SDL3). The build system must:
- Detect `-lGL` and GLSL compilation tools
- Handle missing OpenGL gracefully (fall back to the SDL_RenderCopy path? fail clean?)
- Bundle shader source files or compile them into the binary via `xxd -i` or similar

### Testing burden

Every shader change needs visual verification across at least 2-3 GPU vendors (Intel, NVIDIA, AMD). Use `graphics-reviewer` after each shader edit. For CI, consider software rendering via `llvmpipe` or `swrast`.

### Backward compatibility

If a user's GPU is too old (pre-OpenGL 3.3), the game should:
- Detect and warn at startup
- Fall back to the legacy SDL2 presentation path (keep it around as a fallback even after Tier 3.2)
- Or refuse to start and ask for an upgrade

Option: keep the full Tier 2 CPU pipeline intact and selectable via `-legacy-render` command-line flag.

## Timeline expectation

- Tier 3.1 prototype: 2-3 days (may be abandoned in favor of 3.2)
- Tier 3.2 migration: 2-3 weeks
- Tier 3.3 environments: 1 week per style
- Tier 3.4 lighting: 3-5 days
- Tier 3.5 vector UI: 2 weeks

**Total realistic budget: 6-8 weeks of focused work.**

This is a serious investment. Only start Tier 3 when:
- Tier 2 has shipped and looks great
- You have a concrete reason to push further (release target, portfolio piece, etc.)
- You're willing to maintain the new rendering path long-term

## Guardrails (unchanged from Tier 1/2)

- Don't break the 8-bit CPU fallback until Tier 3.2 is rock solid
- Screenshot before and after every shader change
- Profile hot shaders with RenderDoc or gl-timer queries
- Ship shaders as source files (not just compiled) so they can be inspected
- Don't introduce GPU-only behavior that game logic depends on
