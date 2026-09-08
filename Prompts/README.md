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
| 005 | `005-2026-09-06-the-fine-grain-look.md` | **done (276-294; 285a/285b opened as follow-ups)** | The moiré, measured and filtered: a validated moiré metric, bisection to the causing term, pre-filtered shading distributions (the published answer to Crassin's own open question), a filtered albedo, the shadow-lift discontinuity, the AO dither's crawl. Then the grain as a deliberate style: the reference capture measured in cycles/degree, a world-locked stipple at 3× its frequency, and the side-by-side the owner asked for. Goals 276–294, Group AL. **Outcome:** the metric was built first and falsified twice before it was trusted (9.51x separation on the owner's own two captures). All SIX candidate causes the prompt named measured at zero; the real one was discrete sampling of the per-hit MATERIAL, at 59% of the excess. A per-node average albedo -- zero bytes, zero measurable GPU time -- took `stress_pose` 3.814 -> 1.800, with three rejected versions on the way, each killed by a viewed capture or a test. A deliberate directional stipple followed, measured off the reference (10.67 px period, 641x anisotropy, stone only). Goal 288's side-by-side against his own reference IS committed and viewed: the stone hatch is reproduced on the right material at a visible amplitude, and what separates the two images now is FORM, not grain -- which hands off cleanly to 006. **Two follow-ups opened**: 285a (a fixed-slope instrument, so --stipple-period becomes an absolute knob) and 285b (the grain applied after the TAA resolve, so the owner's 3x survives the neighbourhood clamp). **Two honest limits**: with TAA on the filter's benefit is 6% not 53%, and the owner's 3x grain is achievable optically but not through this engine's TAA. |
| 006 | `006-2026-09-06-earth-terrain-pipeline.md` | **done (295-325; closes 80; 302a/315a/315b/316a/320a opened)** | Retire four octaves of Simplex. A baked 8 km macro field behind the same `height_at`, then the pipeline the research wrote for this engine: continents with Earth hypsometry -> orographic climate -> priority-flood -> flow routing -> implicit stream-power incision -> hillslope diffusion -> channel threshold -> glacial/coastal/karst/aeolian stencils -> 3D caves (reopens goal 80) -> biomes -> vegetation at measured densities. The ten real-Earth acceptance tests as a five-seed gate. Goals 295-325, Group AM. **Outcome:** seven of the ten acceptance tests pass on the shipped seed, up from an untested world; unfilled internal basins 3,536 -> 80; a real dendritic network with 1,160 reaches that all terminate and 170 lakes that all spill. **Goal 80 reopened and closed the other way** -- caves ship, enterable at 15.5 m mean passage width, via Part 7 SS7.5's heightfield-first hybrid, with a conservative band bound and a 10,000-box classification safety check. **And the pass's largest finding was not on the list**: for the whole of Groups AM-A and AM-B **the pipeline was never rendering** -- the bulk column path ignored the macro field, `macro_field` defaulted off, and the playable region was in the sea. All three found by opening a PNG, which is rule 2 vindicating itself. It also exposed a long-standing bug: FastNoise2's FBm is unnormalised, so the detail term delivered 2.82x its stated amplitude -- a 37 degree slope everywhere, which is the "57-71 degree hillsides" Prompt 003 recorded. **The new terrain is CHEAPER than the noise it replaced**: -35% bricks, -37% memory, -19% build time, despite caves costing exactly the +35% sampler time predicted. **The instrument was wrong more often than the terrain** -- fifteen times against roughly four, tabulated in the log SS16. |
| 007 | `007-2026-09-06-view-distance-and-living-cover.md` | **done (326-345; closes 190-195; reopens and answers 40)** | Every distance constant derived from a stated perceptual criterion: Koschmieder's rate constant for fog, the 1-arcmin criterion for the LOD ladder, the region size against the hard V ≤ 24 float limit, depth precision, fixed foveation. Then the ground stops being empty — trees sway (closes 190–192), grass in three tiers (closes 193–195), and the hilltop shot the whole arc exists to produce. Goals 326-345, Group AN. **Outcome:** the renderer's distances stopped being round numbers and became perceptual quantities with citations -- `render/lod/perceptual.hpp` reproduces every worked value in the eye research and pins three errors that were already there (20/20 is 60 ppd not 120; the centre pixel subtends 10.3% more than deg/px; WMO prints 3/sigma and 3.912 appears nowhere in it). **The region grew 256x in area for 2.27x the bricks** -- the exact inverse of the LOD radius, which costs 5.6x the bricks for 2.24x the radius, so the eye's own 1-arcmin limit is an order of magnitude outside the budget. Trees sway as BRANCH CHAINS at 0.049 ms/frame for 129 trees (a per-segment version rang at 1.03 Hz against a predicted 0.26, because a serial chain's mass matrix is dense); their skeletons voxelize for **+0.17% memory**; grass ships in three tiers that share one `grass_blade` function, so the boundary is a density ramp with no measurable ring. **Three honest negatives shipped as completions**: foveation saves a real 23.8% and is visibly degraded, so it is off; the canopy domain warp cannot move a silhouette without warping traversal, so it is rejected; and the region's 2 km boundary IS visible at clear air, because hiding it needs V = 2.67 km. **And the pass's largest finding was not on the list**: `tools/svo_render` and the harness's own `pose_ground` resolver had been working in the pre-Prompt-006 noise world -- a surface differing by a **mean of 35.5 m** -- so the CPU reference had been a reference for nothing and every ground pose in the library was resolving against ground that is not there. Found by putting a CPU frame and a GPU frame of the same coordinates side by side. |

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


## 007 — completion note (main session, 2026-09-08)

All of Group AN, 326–345, closing 190–195 in place and reopening goal 40 to the opposite answer.

**The spend, accounted (goal 341).** Turning this prompt's default-on features off one at a time at
`stress_pose`: shipped 845,260 bricks / vk march 5.83 / d3d12 8.77; minus voxel grass 5.80 / 8.55;
minus skeletons too 5.69 / 8.37; minus sway and back to a 512 m region **4.69 / 6.92**. **The region
is the spend** — everything else is at or below this machine's 18% run-to-run noise floor. vk's whole
GPU frame is **6.32 ms (158 fps), inside the 150 fps target**; d3d12 is 9.19 ms (109 fps), outside —
**and d3d12 with every one of this prompt's features off and the old region is still 7.35 ms (136
fps)**, so nothing comes off the default on its account.

**Four things measurement changed, each of which would have shipped wrong:** a per-segment sway model
that rang at 4× the right frequency, because a serial chain's mass matrix is dense; a canopy warp
that boiled the ground, because grass is `wind_responsive` on purpose; horizontal branches that could
not move sideways, because the rotation was projected onto the wrong plane; and a grass overlay that
drew nothing, because `mul(v, M)` compiles fine and silently transposes.

**Three honest negatives kept as completions, each with both numbers and a capture:** foveation
(23.8% saved, visibly degraded, off); the canopy domain warp (17.9–28.2% of pixels change and none of
it reads as motion, rejected); and the region boundary (visible at clear air, and hiding it needs
V = 2.67 km, which would throw away the derived fog).

**And the finding that was not on the list:** the macro-field bake lived in the app alone, so
`tools/svo_render` and the harness's `pose_ground` resolver had been working in a world whose surface
differs by a **mean of 35.5 m and a worst of 107.1 m**. `dev/scenarios/macro_tree.scn` — "camera two
metres from a trunk" — has no tree within 38 m of its pose. `valley_far`'s moiré gate had been
failing at 2.304 for the same reason and now measures 1.872.
