# Prompts — the side-session → main-session handoff folder

This repo has two working sessions:

- **Side session (this folder's author):** reviews the repo *itself* — it reads the code, not a
  subagent's summary of it — runs web research through read-only subagents, decides what should be
  built next, and writes the prompt files here. It does NOT write engine code by default.
- **Main session (Claude Code, `[CC]`):** the coding session. It reads a prompt file from this
  folder and executes it against the real repo.

### Working in parallel without colliding

The two sessions have twice collided in git: the side session was writing into `research/` while the
main session committed, and its in-progress documents were swept into main-session commits under
main-session messages. Nothing broke — they are additive documents — but the history is wrong about
who wrote what. The convention from now on:

- **Neither session ever runs `git add -A` / `git add .`** — stage explicit paths.
- The **side session commits its own `research/` and `Prompts/` files as soon as they are written**,
  rather than leaving them untracked across a main-session commit.
- If the main session finds untracked `research/*.md` it did not create, it leaves them alone and
  says so in its report.

## Conventions

1. **One prompt = one `.md` file in this folder**, named `NNN-<date>-<slug>.md`
   (`001-2026-09-07-gameplay-physics-world-life.md`). `NNN` is sequential, never reused.
2. Every prompt file is **self-contained**: it must boot [CC]'s context (what to read first),
   state the standing rules, define task groups with concrete **Check** criteria (the repo's
   "a goal is not done without its check" standard), and list what is explicitly out of scope.
3. Prompts reference **repo-internal paths** (`research/*.md`, `docs/goals.md`), not files
   outside the repo, so [CC] never depends on Downloads paths.
4. Research the side session produces or collects is saved into `research/` first, then cited
   from the prompt. Evidence lives in the repo, not in chat history.
5. When a prompt is completed by [CC], it updates `docs/goals.md` (new groups, marked `[x]`
   in place) and `docs/progress.md` (findings folded in) — the prompt says so explicitly each
   time; this README only tracks the prompts themselves.

## Index

| # | File | Status | Scope |
|---|------|--------|-------|
| 001 | `001-2026-09-07-gameplay-physics-world-life.md` | **partial (A, B, C1-C3, C7, E done; C4-C6, D not started)** | Player physics (walking/jump/swim, fixed timestep, micro-step smoothing), crosshair + aim UX, global wind system, trees v2 (space-colonization skeletons + pipe model + sway), grass (voxel blades + wind, overlay stretch), water surface dynamics (Gerstner visual layer) |
| 002 | `002-2026-09-06-dev-harness-and-instrumentation.md` | **done (202-224; 218a/218b/219a opened)** | **Do first.** One declarative option layer for every executable; the `dev/scenario` model + `.scn` files + `voxel_harness` runner (headless or windowed, both backends, deterministic scripted input); frame reports, JSON output, golden images with a calibrated metric; per-pass GPU timing, Tracy measured three ways, the voxel-throughput yardstick; `ctest -L scenario` and CI. Goals 202–224, Group AI |
| 003 | `003-2026-09-06-player-embodiment-and-physics.md` | **done (225-243; closes 173; 244/245/246 opened)** | No more spectator: walk by default, fly/noclip behind `--dev`; **octree-backed collision (closes goal 173)** so it has no 16 m cache edge to outrun; the `clip_stress` scenario at 1/4/10/40× speed; speed, gravity, jump, sprint and slope limits re-derived from the locomotion research; crosshair-metered exposure, veiling-luminance bloom, and motion blur decided against in writing. Goals 225–243, Group AJ |
| 004 | `004-2026-09-06-frame-time-and-gpu-architecture.md` | **done (244-275; closes 157, 158, 161, 162; 163 deferred with a reason; 272a/273a/273b and 275a-275h opened)** | **The big one.** Account for the missing half of a 13.15 ms frame first; kill the rebuild storm (a full 400 MB world rebuild every 2 m of camera motion); ESVO-style relative-within-block addressing + a grid of shallow trees; a GPU-resident node/brick cache with GPU-side LRU and ray-guided requests (GigaVoxels), never stalling a ray; a coarse start-`t` pre-pass; compute + swizzle + persistent threads; palette and DAG compression; counters, RenderDoc trigger, and a frame-time percentile gate. Closes/advances 157, 158, 161, 162, 163. Goals 244–275, Group AK. **Outcome:** the premise was falsified by the first measurement (vsync was hardcoded; the median frame was already 5.21 ms, 192 fps, with no missing 7–10 ms), so the pass became about variance and architecture. March 4.90 → 3.50 ms on vk with the backends converged; resident 543.7 → 279.5 MB; `fly_transect` p99 13.16 → 8.52 ms; 457 M voxels at 7.8 mm at 150+ fps. **Three measured negatives kept as completions**: the beam pre-pass (real 19% march win, real 0.69 ms cost, exact wash), the compute port (its prize was already banked by removing `SV_Depth`, and warp efficiency is 92–94%), and the column-grid cache (0% hit rate, deleted). RenderDoc's trigger is built and its absent path verified; its active path is UNTESTED because RenderDoc is not installed. |
| 005 | `005-2026-09-06-the-fine-grain-look.md` | issued | The moiré, measured and filtered: a validated moiré metric, bisection to the causing term, pre-filtered shading distributions (the published answer to Crassin's own open question), a filtered albedo, the shadow-lift discontinuity, the AO dither's crawl. Then the grain as a deliberate style: the reference capture measured in cycles/degree, a world-locked stipple at 3× its frequency, and the side-by-side the owner asked for. Goals 276–294, Group AL |
| 006 | `006-2026-09-06-earth-terrain-pipeline.md` | issued | Retire four octaves of Simplex. A baked tile field behind the same `height_at` interface, then the pipeline the research wrote for this engine: continents with Earth hypsometry → orographic climate → priority-flood → flow routing → implicit stream-power incision → hillslope diffusion → channel threshold → glacial/coastal/karst/aeolian stencils → 3D caves (reopens goal 80) → biomes → vegetation at measured densities. The ten real-Earth acceptance tests as a five-seed nightly gate. Goals 295–325, Group AM |
| 007 | `007-2026-09-06-view-distance-and-living-cover.md` | issued | Every distance constant derived from a stated perceptual criterion: Koschmieder's rate constant for fog, the 1-arcmin criterion for the LOD ladder, the region size against the hard V ≤ 24 float limit, depth precision, fixed foveation. Then the ground stops being empty — trees sway (closes 190–192), grass in three tiers (closes 193–195), and the hilltop shot the whole arc exists to produce. Goals 326–345, Group AN |

### The arc, and why it is in this order

002 is the foundation: every Check in 003–007 is a harness scenario, and without it the owner is
back to hand-composing 43-flag command lines. 003 is next because it is small, independent, and the
owner feels it immediately. 004 is the largest and highest-risk prompt and everything visual depends
on the headroom it creates. 005 spends that headroom on the look. 006 is independent of 004/005 and
can be interleaved. 007 is last because it consumes 004's throughput number and 006's biome field.

144 goals, numbered 202–345, groups AI through AN.

## How to request the next prompt (for the human)

Tell the side session, in any wording, what the next feature arc is. It will: re-read whatever
parts of the repo changed since the last prompt (it does its own context gathering — subagents
are used only for web research, never for repo summarization), run any web research the topic
needs, and drop `NNN-….md` here with the same structure.

## 001 — completion note (main session, 2026-09-07)

Groups **A** (player physics), **B** (wind), **E** (water motion) and **C7** (mesh-path wind
parity) are complete with their Checks recorded in `docs/goals.md` groups AD/AE/AH and goal 189.
Group **C** is partial: **C1-C3** (space-colonization skeletons, pipe-model radii, leaf mass) are
done and tested, with a viewed skeleton dump via the new `tools/tree_dump`. **C4-C6** (the sway
oscillator, skeleton voxelization, geometric canopy motion) and all of **D** (grass) were not
started — the pass ran out of budget, and `research/gameplay-pass-log.md` §8/§8b record what they
build on rather than leaving half a tree system behind. Goals: `docs/goals.md` AF (186-188 done,
190-192 open) and AG (193-195 open).

Two Checks are knowingly weaker than the prompt asked, both with the reason written down: A3 has no
capture sequence (a 4 cm effect at a 0.1 s time constant is not something a still shows honestly;
the unit test asserts the stronger claim), and E3 applies its shore fade only on the CPU (a shader
depth probe needs a water-skipping traversal variant the 7,000-ray oracle guards, and because this
implementation displaces normals rather than geometry, the artefact E3 prevents cannot occur).

177/177 tests, `--verify-frame` 34.7%/34.6% on both backends, `--autofly --walk` 0 violations.

## 002 — completion note (main session, 2026-09-06)

All of Group AI. `engine/cli` (one option layer, four executables, exactly one argv site left in the
repo), `dev/scenario` + `dev/harness` + `dev/telemetry`, ten scenarios passing on both backends
headless with 34 promoted goldens and committed baselines, per-pass GPU timing measured at 0.16% of
a frame, and the GPU-counter decision argued rather than asserted.

**Four things measurement changed, each of which would have shipped wrong:** the phase-accounting
check was circular and, once honest, found 186 ms of a 205 ms frame hiding in `capture_phase` (and
my first hypothesis about where it was, was wrong); the ImGui overlay was baked into every golden,
and removing it dropped the noise floor by an order of magnitude; `walk_shoreline`'s absolute pose
put the camera eighteen metres inside a hill; and the hand-rolled JSON writer shipped broken with a
test that checked brace balance instead of parsing.

**Three honest negatives, all with numbers:** a one-pixel change cannot be caught by the golden
metric (floor ~1,200 of 921,600 pixels); captures are not bytewise reproducible because the
animation clock is wall-clock (isolated — `--no-wind` on a still pose gives 0.000%); and the
throughput ramp crashes at `--lod-radius 32` because `build_tree` has no memory bound. Goals 218a,
218b and 219a are open with their reproductions.
