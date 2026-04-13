# Graphics Modernization Status

Per-item checklist across all three tiers from `GRAPHICS_STATE_AND_ROADMAP.md`. Update this when items land or are descoped.

Legend: ✅ shipped · 🚧 in progress · ⬜ not started · ⏭ skipped · 🔁 reworked

## Tier 1 — Quick wins (hours, noticeable impact)

| # | Item | Status | Shipped in | Notes |
|---|---|---|---|---|
| 1.1 | Animated cursor trails | ✅ | PR #4 | GLOW particles from `apply_all_cursor` in `src/game.c` with prev-position tracking. GLOW particle parameters retuned for trails (0.25-0.40s life, size 1-2). |
| 1.2 | Screen shake on territory shifts | ✅ | PR #4 | `lw_sdl_trigger_shake` in `sdl_compat.c`, offsets `SDL_RenderCopy` dest rect, 15%/frame decay. Triggered at 8.0 from `check_loose_team`, throttled 2.0 every 30 direct kills in `fighter.c`. Not yet verified in a live match — only exercised in menu screenshots. |
| 1.3 | Radial gradient vignette | ✅ | PR #4 | `lw_postfx_vignette` in `postfx.c`, t² falloff, strength 0.35. Called from `lw_sdl_present_screen` after ripple and glow. |
| 1.4 | Soft drop shadows on UI | ✅ | PR #4 | 2px offset shadow rect in `my_button_proc`, +1/+1 text shadow in `my_button_proc`, `my_text_proc`, `my_ctext_proc`. |
| 1.5 | Button hover animation | ✅ | PR #4 | `d->d2` stores 0-255 hover intensity, ramped +25/-20 per frame in `update_dialog`. Button proc consumes it as a 3-step color pick at 64/192 thresholds. Not a true RGB lerp (would need a temp palette slot, guardrails forbid) — known limitation. |
| 1.6 | More dissolve particles on direct hits | ✅ | PR #4 | p0 spawn bumped 2 → 5 with ±2 px spread; p1/p2 unchanged. |

**Tier 1 complete.** All 6 items shipped. Known follow-up: screen shake on team elimination not verified mid-game.

## Tier 2 — Meaningful upgrades (days, transformative impact)

| # | Item | Status | Target PR | Notes |
|---|---|---|---|---|
| 2.1 | Supersampled game area (2x/4x) | ⏭ | — | Descoped in favor of 2.3 (32-bit compositing), which solves the same problem more cleanly. |
| 2.2 | Caustic map background animation | ⬜ | `tier2-caustics` | Next up. `graphics-polisher` agent. See `TIER2_PLAN.md` Step 1. |
| 2.3 | Real-time 32-bit game area compositing | ⬜ | `tier2-32bit-compositing` | **PIVOT POINT.** Required by 2.4. `Plan` agent first, then `graphics-polisher`. See `TIER2_PLAN.md` Step 2. |
| 2.4 | Depth-shaded armies with lighting | ⬜ | `tier2-depth-shading` | Depends on 2.3. Single most transformative change. `graphics-polisher` agent. See `TIER2_PLAN.md` Step 3. |
| 2.5 | Modern dialog theme | ⬜ | `tier2-dialog-theme` | `ui-designer` agent. Touches every menu. See `TIER2_PLAN.md` Step 4. |

**Tier 2 status: not started.** Execution plan in `TIER2_PLAN.md`.

## Tier 3 — Engine-level upgrades (weeks, transformative impact)

| # | Item | Status | Target PR | Notes |
|---|---|---|---|---|
| 3.1 | GPU rendering via SDL_Renderer geometry + shaders | ⬜ | — | SDL2 shader support is limited; consider jumping to 3.2 instead. See `TIER3_PLAN.md` Step 1. |
| 3.2 | SDL3 or direct OpenGL/Vulkan presentation layer | ⬜ | — | Unlocks modern post-processing via fragment shaders. Major migration. See `TIER3_PLAN.md` Step 2. |
| 3.3 | Procedural animated environments | ⬜ | — | Depends on 3.1 or 3.2. See `TIER3_PLAN.md` Step 3. |
| 3.4 | Dynamic lighting with army self-glow | ⬜ | — | Depends on 3.1 or 3.2. See `TIER3_PLAN.md` Step 4. |
| 3.5 | Resolution-independent vector UI | ⬜ | — | Depends on 3.1 or 3.2. See `TIER3_PLAN.md` Step 5. |

**Tier 3 status: not started.** Execution plan in `TIER3_PLAN.md`. Only tackle after Tier 2 is complete and you have concrete motivation (commercial release, Steam launch, etc.).

## Update procedure

When an item lands:
1. Flip its status to ✅
2. Add the PR reference
3. Note anything the ship deviated from in the "Notes" column
4. If a follow-up emerged, add a row for it or note it at the bottom of the section

When an item is descoped or reworked:
1. Flip to ⏭ or 🔁
2. Explain in "Notes" what replaced it or why it's gone
