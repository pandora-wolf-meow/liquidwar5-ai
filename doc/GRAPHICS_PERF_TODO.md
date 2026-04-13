# Graphics engine performance TODO

First-pass review by `graphics-code-reviewer` on 2026-04-12 against
`src/postfx.c` and the present path in `src/sdl_compat.c`. Items are
ordered by expected impact / effort ratio. None of these have been
measured yet — step one of each item is to add `SDL_GetPerformanceCounter`
timing and record the before number.

Pipeline context (see `.claude/agents/graphics-polisher.md` and
`.claude/agents/graphics-code-reviewer.md` for the full architecture):

1. 8-bit `putpixel` into `CURRENT_AREA_DISP`
2. `display_area()` stretch_blit to `NEXT_SCREEN`
3. `lw_sdl_present_screen()` 8→32 palette conversion
4. `postfx.c` liquid ripple + battle glow on 32-bit buffer
5. `SDL_UpdateTexture` + `SDL_RenderCopy`

Budget: 60 fps on 640×480 = 307,200 pixels/frame per pass.

## Punch list

### 1. Palette conversion has no LUT *(biggest single win)*
`src/sdl_compat.c:2264-2274` does a per-pixel `pal->colors[src_row[x]]`
lookup plus shifts/ors — 307k pointer indirections per frame. Build a
`uint32_t lut[256]` once inside `set_palette()` (`src/sdl_compat.c:731`)
and collapse the inner loop to a single load+store. Zero visual change.

### 2. Post-FX runs in menus
`src/sdl_compat.c:2283-2290` calls ripple + glow unconditionally. In
menus there are no palette-128+ pixels, so ripple is a no-op loop over
307k pixels and glow still scans the buffer twice. Gate both passes on
a "gameplay active" flag, or early-out if a cheap pre-scan finds no
army pixels. Coordinate with `graphics-polisher` in case menu effects
are planned.

### 3. Battle glow kernel is NxN, not separable
`src/postfx.c:102-136` writes up to 25 pixels per edge pixel with a
diamond (Manhattan-distance) falloff. Equivalent to two 1-D dilation
passes (horizontal then vertical) — O(N) instead of O(N²) per edge
pixel. Visual-parity check required: naive separation gives a square
falloff; weight the passes to preserve the diamond. Also `memset
(edge_map, 0, w*h)` every frame touches 307k bytes that pass 1
overwrites anyway — track edge pixel coords in a list instead.

### 4. Ripple calls `sinf` / `cosf` ~4× per army pixel
`src/postfx.c:171-176, 227-232`. No LUT. Replace with a 1024-entry
sine table indexed by fixed-point phase. Additionally, terms like
`sinf(y*0.03f + time*1.5f)` are constant across a row — hoist out of
the inner x loop. Both changes are visual-parity if LUT resolution
≥ 1024.

### 5. Masked ripple `memcpy`s the full buffer
`src/postfx.c:212-213` copies 100% of the 32-bit buffer even when the
army-pixel mask covers maybe 5%. Compute a bounding box of palette-128+
pixels (reuse from item 2's pre-scan) and memcpy only that region.
Better: share a single persistent scratch buffer across all post-FX
passes — currently `edge_map`, `lw_postfx_liquid_ripple` `temp_buf`,
and `lw_postfx_liquid_ripple_masked` `temp_buf` are three separate
`static` allocations with identical lifetimes (`src/postfx.c:52, 147, 197`).

### 6. `fx_time += 0.016f` is frame-counted, not wall-clock
`src/sdl_compat.c:2280`. Ripple animation speed drifts with frame rate.
Use an `SDL_GetTicks()` delta.

## Lower-impact / cleanup

- **Dead code**: `lw_postfx_liquid_ripple` (unmasked, `src/postfx.c:143`)
  appears unused — only the masked variant is called from the present
  path. Confirm and delete.
- **`SDL_UpdateTexture` uploads the full 640×480×4 = 1.2 MB every
  frame** (`src/sdl_compat.c:2293`). Dirty-rect tracking is possible
  but probably not the bottleneck yet — measure first.
- **Linear filter hint** at `src/sdl_compat.c:2242` is a hint, not a
  texture property. Consider `SDL_SetTextureScaleMode` explicitly so
  it survives driver quirks.

## Process

Each item becomes a separate commit with a `[perf]` prefix including
the hot path, the before/after measurement, and the speedup, e.g.
`[perf] Precompute palette LUT on set_palette (1.8ms → 0.3ms/frame)`.
Never land an optimization that changes visible pixels without user
approval — capture a frame before and after via `SDL_SaveBMP` in
`lw_sdl_present_screen` and compare.
