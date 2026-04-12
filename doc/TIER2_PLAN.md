# Tier 2 Execution Plan

This document hands off the graphics modernization work to the next session. It captures what's already done, what's next, which agents to use, and the exact kickoff prompts.

## Status as of handoff

### Merged PRs (in order)

1. **#1** — Allegro 4 → SDL2 migration with visual enhancements
2. **#2** — `graphics-polisher` subagent
3. **#3** — `doc/GRAPHICS_STATE_AND_ROADMAP.md` (3-tier roadmap)
4. **#4** — Tier 1 polish pass (all 6 items)
5. **#5** — Four more subagents (`ui-designer`, `graphics-reviewer`, `graphics-code-reviewer`, `perf-profiler`)

### Tier 1 items shipped (PR #4)

- **1.3** Radial gradient vignette (`postfx.c:lw_postfx_vignette`)
- **1.4** Drop shadows on UI buttons and text (`alleg2.c`)
- **1.5** Eased button hover transition via `d->d2` intensity ramp
- **1.1** Cursor trail GLOW particles spawned from `apply_all_cursor`
- **1.2** Screen shake (`lw_sdl_trigger_shake`) on team elimination and throttled direct hits
- **1.6** More dissolve particles on direct hits (2 → 5, with ±2 px spread)

Known follow-up from PR #4: screen shake on team elimination was never verified in a live match — the timed screenshot run only exercised the main menu.

## Agents available in the next session

These are defined in `.claude/agents/` and load at session start:

| Agent | Focus | When to use |
|---|---|---|
| `graphics-polisher` | General visual improvements | Post-processing, particles, shading, gameplay visuals |
| `ui-designer` | Dialogs, menus, buttons, score screens | Tier 2.5 work, anything touching `alleg2.c` dialog procs |
| `graphics-reviewer` | Visual QA via screenshots | After every visual change; run independently to catch regressions |
| `graphics-code-reviewer` | Diff review + optimization | Partners with polisher; reviews visual feature PRs |
| `perf-profiler` | CPU measurement + optimization | Before/after Tier 2 effects stack up |

## Tier 2 execution order

The roadmap prescribes a specific sequence because each item unlocks the next. **Do not reorder.**

### Step 1 — Tier 2.2: Caustic map backgrounds (half day)

**What:** Add a new post-processing function in `src/postfx.c` that overlays a procedurally animated caustic pattern (two overlapping sine grids) on the non-playable map area. Makes static backgrounds feel alive.

**Files:**
- `src/postfx.c` — new `lw_postfx_caustic_background(pixels, w, h, pitch, time, strength, index_map, index_pitch)`
- `src/postfx.h` — declaration
- `src/sdl_compat.c:lw_sdl_present_screen` — call it before vignette, masked to palette indices < 64 (non-army, non-texture, i.e. map background pixels)

**Technical notes:**
- Two sine grids at different frequencies rotated ~30° apart, combined additively
- Displacement is NOT the effect — additive brightness modulation is
- Amplitude ~15-25 brightness units, cycle time ~4 seconds
- Mask via the 8-bit `index_map` that we already pass around (the original screen surface pixels)
- Only lighten (additive), never darken — caustics are bright spots

**Risk:** low; isolated to post-processing.

**Kickoff prompt for `graphics-polisher`:**
> Implement Tier 2.2 from `doc/GRAPHICS_STATE_AND_ROADMAP.md` — caustic map background animation. Add `lw_postfx_caustic_background` to `src/postfx.c` that overlays two-sine-grid procedural caustics on pixels with palette index < 64. Call it from `lw_sdl_present_screen` before vignette, before the existing ripple and glow. Additive blending only (brightness modulation, never darken). Use strength around 0.15-0.2 as a starting point — the effect should be noticeable but not distracting. Commit with `[tier2]` prefix. After implementation, run the `graphics-reviewer` agent to confirm the effect is visible on a gameplay screen (start a human game with arrow keys and let the agent screenshot at frame 500).

### Step 2 — Tier 2.3: 32-bit game area compositing (2-3 days) — PIVOT POINT

**What:** Stop rendering the game area through the 8-bit palette path. Instead, render fighters directly into the 32-bit `lw_convert_surface` with full RGB per fighter. Keep the 8-bit screen for UI overlays only, then composite UI on top of the 32-bit game layer.

**Why:** Every downstream visual improvement (depth shading, proper anti-aliasing, soft army edges, alpha blending, modern post-processing) needs full RGB per pixel. The 8-bit pipeline is the ceiling on what's achievable visually. Breaking through this ceiling is the single most important structural change for the whole modernization effort.

**Files:**
- `src/disp.c` — rewrite `disp_stretch_area` to render directly into a new 32-bit buffer
- `src/sdl_compat.c:lw_sdl_present_screen` — change compositing order: 32-bit game layer first, then 8-bit UI layer keyed on non-black
- `src/fighter.c` — `disp_fighter` writes into 32-bit buffer using precomputed team RGB, or keeps 8-bit write and conversion happens in disp_stretch_area
- `src/game.c` — check that gradient display paths (`display_gradient`, `display_mesh`) still work
- Possibly `src/grad.c` and `src/decal.c` — verify nothing else reads back `CURRENT_AREA_DISP` palette indices for game logic

**Architecture decision needed at start:**

Option A: **Dual-buffer approach** — keep 8-bit `CURRENT_AREA_DISP` for game logic reads, render it to a new 32-bit `CURRENT_AREA_DISP32` each frame in disp.c, composite from there. Safer, minimal game logic changes.

Option B: **Full 32-bit game area** — convert `CURRENT_AREA_DISP` to a 32-bit surface, update all putpixel/getpixel call sites. More invasive but cleaner.

**Recommendation:** Option A. The game area is small (usually ~280x240) so the 32-bit copy is cheap (~270 KB), and we avoid touching fighter movement code which is hot-path and carefully tuned.

**Risk:** medium-high. This is the most architectural change of the whole Tier 2 sweep. Budget 2-3 solid days and plan for a revert if it goes sideways.

**Workflow:**
1. Start with the `Plan` agent to design the compositing pipeline. Read `disp.c`, `sdl_compat.c:lw_sdl_present_screen`, and trace where palette conversion happens now.
2. Implement in small steps with screenshots between each.
3. After each step, run `graphics-reviewer` to confirm no regressions on menu and gameplay.
4. Run `perf-profiler` at the end to confirm the new pipeline stays under 3ms/frame total.

**Kickoff prompt for `Plan` agent (first):**
> Design the implementation plan for Tier 2.3 from `doc/GRAPHICS_STATE_AND_ROADMAP.md`. Read `src/disp.c`, `src/sdl_compat.c:lw_sdl_present_screen` (line ~2214 region), `src/fighter.c:disp_fighter` and `erase_fighter`, and `src/grad.c:create_gradient_bitmap` to understand what currently reads or writes `CURRENT_AREA_DISP`. Return a step-by-step plan for moving the game area rendering to 32-bit compositing without breaking game logic. Recommend Option A (dual-buffer) vs Option B (full replacement) with reasoning. List the files that need changes and estimated effort per step.

Then hand the plan to `graphics-polisher` for implementation.

### Step 3 — Tier 2.4: Depth-shaded armies (2-3 days)

**Depends on:** Tier 2.3 must be done first.

**What:** Compute a virtual height field over each army body (distance from the nearest non-army pixel), shade with a fake directional light (top-left by convention, soft shadow bottom-right). Makes armies look like 3D liquid mounds instead of flat blobs. This is **the** transformative visual change.

**Files:**
- New `src/shading.c` / `src/shading.h` — height field computation, lighting evaluation
- `src/disp.c` — call shading before final composite
- `src/postfx.c` — optionally use the height field for rim lighting

**Technical notes:**
- Height field: single-pass distance transform scanning from army→non-army pixels. Cap at ~8 pixels.
- Lighting: simple Lambertian using a gradient of the height field. Light direction (0.7, 0.7, 0.3) normalized.
- The shading modulates the base team color's brightness — no per-pixel color change, just multiply the base RGB by (0.6 + 0.4 * NdotL).
- Needs 32-bit game area (Tier 2.3) — cannot modulate brightness on 8-bit palette indices.
- Cache the height field; only recompute when the army has moved significantly.

**Risk:** medium. Performance-sensitive due to per-pixel distance transform, but the area is small.

**Kickoff prompt:**
> Implement Tier 2.4 from `doc/GRAPHICS_STATE_AND_ROADMAP.md` — depth-shaded armies with simulated directional lighting. You require Tier 2.3 (32-bit game compositing) to already be done. Add `src/shading.c` with a distance-transform-based height field over army pixels and a Lambertian-style lighting pass. Use it from `src/disp.c` to shade the game area before compositing. Then call `perf-profiler` to confirm the shading pass stays under 1.5ms/frame. Then call `graphics-reviewer` to capture before/after gameplay screenshots.

### Step 4 — Tier 2.5: Modern dialog theme (3-4 days)

**What:** Rewrite `my_button_proc`, `my_text_proc`, `my_ctext_proc`, `my_edit_proc`, `my_textbox_proc` for a modern look: gradient fills, 1-2 px rounded corner approximation, larger fonts, better spacing, icons. Also update `src/info.c` info bar styling.

**Files:**
- `src/alleg2.c` — all `my_*_proc` drawing logic
- `src/dialog.c` — maybe helpers for gradient fills, rounded rect
- `src/info.c` — in-game info bar
- `src/score.c` — score screen bar chart visuals
- Possibly new assets in `data/ui/` if icons are added (optional)

**Risk:** medium. Touches every menu, every screen. Layouts may shift slightly.

**Agent:** Use **`ui-designer`** for this, not `graphics-polisher`. It has specific context about the DIALOG system conventions.

**Kickoff prompt for `ui-designer`:**
> Implement Tier 2.5 from `doc/GRAPHICS_STATE_AND_ROADMAP.md` — modern dialog theme. Rewrite `my_button_proc`, `my_text_proc`, `my_ctext_proc`, `my_edit_proc`, and `my_textbox_proc` in `src/alleg2.c` with: vertical gradient button fills (light-to-dark), 1-2 px corner pixel masking for rounded appearance, drop shadow (already partial from Tier 1.4 — enhance it), larger TTF font where legibility allows. Also update the info bar in `src/info.c` with cleaner styling. Do NOT change DIALOG array element counts or indexing — just their drawing. Do NOT overwrite palette entries 0-17. Commit each sub-screen as its own `[tier2-ui]` commit. Run `graphics-reviewer` after each to verify no menu regressions.

## Session kickoff sequence

When you start the new session, paste this to resume:

```
Continue the Liquid War 5 graphics modernization. Read doc/TIER2_PLAN.md
for the handoff plan. We just merged Tier 1 (PRs 1-5) and the next
session should work through Tier 2 in the documented order: 2.2
caustic backgrounds, 2.3 32-bit game compositing (the pivot point),
2.4 depth-shaded armies, 2.5 modern dialog theme. The graphics-polisher,
ui-designer, graphics-reviewer, graphics-code-reviewer, and perf-profiler
subagents are available. Start with Step 1 (2.2 caustic backgrounds)
using the graphics-polisher agent with the kickoff prompt from the plan.
After 2.2 lands, run graphics-reviewer to verify, then move to 2.3.
```

## Branch discipline

- Each Tier 2 item gets its own branch and PR: `tier2-caustics`, `tier2-32bit-compositing`, `tier2-depth-shading`, `tier2-dialog-theme`
- Use `[tier2]` or `[tier2-ui]` commit prefixes
- Merge each PR before starting the next (they have dependencies)
- After Tier 2.3 merges, capture a gameplay screenshot as the baseline for 2.4 depth shading — you'll want to compare against flat-shaded armies when the shading work lands

## Guardrails (unchanged from Tier 1)

- Don't blur UI/text — keep effects masked or off the UI
- Don't overwrite palette entries 0-17 at runtime
- Mouse clicks must stay edge-triggered (no blocking wait loops)
- Screenshot before and after every visual change — the user has specific aesthetic preferences that don't always match textbook graphics theory
- Keep the 8-bit pipeline functional until 2.3 is done; some game logic reads palette indices back from the game area bitmap

## Timeline expectation

- Tier 2.2: 1-2 agent turns
- Tier 2.3: 4-8 agent turns (the pivot; will need iteration)
- Tier 2.4: 3-6 agent turns
- Tier 2.5: 4-6 agent turns per screen × ~4 screens

Total: roughly a session's worth of work if things go smoothly, two sessions if Tier 2.3 hits unexpected complexity in the game-logic palette reads.
