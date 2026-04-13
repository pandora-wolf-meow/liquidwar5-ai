# Documentation index

Entry points into the Liquid War 5 documentation. Start here when picking up the project.

## Modernization effort (read in this order)

1. **[GRAPHICS_STATE_AND_ROADMAP.md](GRAPHICS_STATE_AND_ROADMAP.md)** — current rendering pipeline inventory and the 3-tier modernization roadmap. **Read this first** for architectural context.
2. **[TIERS_STATUS.md](TIERS_STATUS.md)** — live per-item checklist across all three tiers. Shows what's shipped, what's next, and which items were reworked or descoped.
3. **[TIER2_PLAN.md](TIER2_PLAN.md)** — execution plan for Tier 2 (caustic backgrounds → 32-bit compositing → depth shading → modern dialog theme) with ready-to-paste subagent kickoff prompts.
4. **[TIER3_PLAN.md](TIER3_PLAN.md)** — execution plan for Tier 3 (GPU rendering, SDL3/OpenGL migration, dynamic lighting, procedural environments, vector UI). Only start after Tier 2 is complete.

## Engine migration history

- **[SDL2_MIGRATION_SPEC.md](SDL2_MIGRATION_SPEC.md)** — original spec for the Allegro 4 → SDL2 port. Describes what the compatibility layer needs to provide and the migration phases. Historical; the migration is complete.

## Build and distribution

- `README` — original project README (Allegro-era text, some details out of date; the Makefile layout and build instructions still apply)
- `Makefile.in` — documentation build system (Python-based, generates txt/html/man/info/pdf/ps from shared sources)

## Agents

Specialized subagents for the modernization live in `../.claude/agents/`:

| Agent | Focus |
|---|---|
| `graphics-polisher` | General visual improvements across the pipeline |
| `ui-designer` | Menus, dialogs, buttons, score screens |
| `graphics-reviewer` | Visual QA via screenshot capture |
| `graphics-code-reviewer` | Diff review + optimization partner for polisher |
| `perf-profiler` | CPU measurement and hot-path optimization |

They load at session start from `.claude/agents/`.

## Resuming work in a new session

Paste this to pick up where the last session left off:

```
Continue the Liquid War 5 graphics modernization. Read doc/INDEX.md
and doc/TIERS_STATUS.md to see where we are. The next unchecked Tier 2
item in TIER2_PLAN.md is the one to tackle. Specialized subagents
(graphics-polisher, ui-designer, graphics-reviewer, graphics-code-reviewer,
perf-profiler) are loaded from .claude/agents/. Use the kickoff prompt
from the plan for the next step.
```
