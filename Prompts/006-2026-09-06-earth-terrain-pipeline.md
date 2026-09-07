# Prompt 006 — The terrain pipeline, rebuilt on Earth's own physics

**To:** the main coding session (`[CC]`), branch `C++-voxel`
**From:** the side session, 2026-09-06
**Read this whole file before touching anything.**

**Depends on Prompt 002** (the harness — every Check is a scenario or a statistics assertion) and
is **independent of 003/004/005**. It can run in parallel with the look/perf work in principle, but
it touches `world/generation` and `world/svo/terrain_sampler`, which 004's brick-compression work
also reads — coordinate, or sequence it after 004.

---

## 0. What this pass is and is not

**The gap.** The entire world is four octaves of Simplex noise.
`world/generation/src/heightmap_generator.cpp`, read in full: one `FastNoise::Simplex` at
`kFeatureScale = 200.0f` world units per period, wrapped in `FractalFBm` with
`kOctaveCount = 4`, `kLacunarity = 2.0`, `kGain = 0.5`, remapped from [-1,1] to
[-64, +64] m. That is the whole terrain. There is no erosion, no hydrology, no drainage, no
lithology, no climate, no biome, no tectonic structure. Sea level is a plane at y = 0 and land is
"noise above it."

It looks exactly like what it is. In `research/captures/lin_final_vk.png` and
`svo_ground_hilltop.png` — both viewed for this prompt — the terrain is a field of isotropic
noise cones: every ridge is a spike, every ridge is the same size as every other ridge, no valley
carries water anywhere, drainage is absent, and the "coastline" is a contour line rather than a
shore. `research/earth-terrain-geomorphology-research.md` Part 7 §1 names this precisely as the
statistical failure mode of noise terrain, and §9 gives ten measurable ways to prove it.

**What this pass turns it into.** The pipeline the research already wrote *for this engine*, stage
by stage: `research/earth-terrain-geomorphology-research.md` Part 7 §10.2, which was written with
this repo's constraints in hand (it names "analytic fBm heightfield today, ray-marched 7.8 mm SVO,
static deterministic world, generation-time budget ~0.6 s world-ready, so the macro pipeline can
afford hundreds of ms regionally"). Continents with Earth-like hypsometry → orographic climate →
priority-flood → flow routing → flow accumulation → implicit stream-power incision → hillslope
diffusion → channel threshold → stencil passes for glacial, coastal, karst and aeolian terrain →
3D cave density → biome map → vegetation at measured densities.

**Named outcome.** You stand at a spawn and there is a *river* below you that came from somewhere
and goes to the sea; the valley it is in has a concave profile and a characteristic spacing from
its neighbours; the ridge above you is asymmetric because the wet side eroded faster; the desert is
where the rain shadow is, not where the noise happened to be dry; and ten histograms computed from
the generated heightfield fall inside bands measured on real Earth.

**What this pass is NOT.** Not real-time erosion. Not tectonic simulation. Not a glacial LEM. Not
per-frame hydrology. Part 7 §10.1 draws that line explicitly and argues it: *"Full LEM-at-planet-
scale is out … Instead: stamp linear orogenic belts … with fake roots … and let the SPIM pass carve
real drainage through the fake mountains. The rivers will make the stamps credible; nothing else
will."* Follow that line. Everything on the "fake convincingly" side of it stays a stencil pass
validated by the acceptance suite.

---

## 1. Context to read FIRST, in this order

**The research reading here is mandatory and it is the bulk of the work's specification.** This
prompt does not restate the mathematics; it tells you which section holds it. Read the sections,
not a summary of them.

1. **`CLAUDE.md`** — build non-negotiables. Note especially the FastNoise2 SIMD note: **`SCALAR` is
   not compiled into this project's FastNoise2** and `FastNoise::New<T>` returns *null* for an
   uncompiled feature set. `heightmap_generator.cpp` pins `SSE2` deliberately for cross-machine
   bit-identity and wraps `New<T>` in a throwing helper. Preserve both properties.
2. **`docs/progress.md`** — including *"Decisions that survived contact with evidence."* One entry
   is directly in this prompt's path: **cave/3D-density terrain was decided against** (goal 80),
   described as "a full future arc touching generation+meshing+streaming, not a line item." This
   prompt *is* that arc, and the research is the new evidence that reopens it. Say so explicitly
   in the log rather than quietly contradicting the list.
3. **`docs/goals.md`** — goal 80 (3D density/caves), and Groups W (sparse-brick octree core) and V
   (chunk-generation load-time optimization). Group V matters because it measured per-chunk
   generation cost; this pass adds work to generation and must not undo it.
4. **`research/earth-terrain-geomorphology-research.md`** — **the specification.** 3,700+ lines,
   seven parts. Read, at minimum, and in this order:
   - **Part 7 §10 "The honest ceiling and the recommended pipeline"** — read this FIRST. §10.2 is
     the seven-stage pipeline this prompt implements, written for this engine.
   - **Part 7 §9 "Validation: acceptance tests against real-Earth statistics"** — the ten tests
     with their measured Earth bands. **These are this prompt's Checks.** Read before writing code,
     not after.
   - **Part 2 §11 "Practical simulation synthesis — the opinionated pipeline"** — the fluvial core
     in seven numbered stages with "what breaks if you skip it" for each, plus the
     literature-grounded parameter table ($m$, $n$, $\theta$, $K$, $D$, $A_c$, bankfull width,
     meander wavelength, talus repose, $\tau_c$, $L_c$, knickpoint celerity, hypsometric integral).
     **Every constant this pass introduces should come from that table or be justified against it.**
   - **Part 2 §1** (the stream power incision model), **§3** (drainage-network anatomy), **§4**
     (hillslopes), **§5** (landscape evolution models), **§8** (longitudinal profile and base
     level). This is the mathematics of stage 3.
   - **Part 1 §10** (tectonics, practical simulation synthesis) and **§2–§3** (isostasy, the height
     limit of mountains) for the continental skeleton and why mountains have a ceiling.
   - **Part 3 §1–§3 and §10** (glacial erosion mechanics, alpine landforms quantitatively, the
     glacial buzzsaw, synthesis) and **§6–§7** (coastal erosion and deposition) for the stencils.
   - **Part 4 §11** ("a cave-generation recipe for a voxel engine" — it is written as a recipe) and
     **§4** (cave geometry and scale statistics) and **§8** (karst surface terrain).
   - **Part 5 §1–§3, §6, §10** (wind-blown sand physics, the dune-morphology phase diagram, dune
     dynamics, desert climate geography, synthesis).
   - **Part 6 §1–§3, §6, §10** (tree density by biome, the self-thinning law, spatial point
     processes, the environmental-limit biome-mask table, synthesis).
5. **`research/procedural-terrain-generation-survey.md`** — the algorithms-side companion (it is
   Part 7 standalone). §1 (noise's statistical failures), §2 (domain warping), §3 (erosion
   algorithms), §4 (hydrologically-correct terrain), §5 (shipped systems), §6 (biome maps), §7
   (voxel-specific terrain — **§7.5 argues for the heightfield-first hybrid this pass should use**),
   §8 (vegetation placement).
6. **`research/caves-karst-and-underground-terrain.md`** — the standalone cave document.
7. **`research/micro-voxel-pivot-log.md`** §2.5 — **why `TerrainSampler` exists and what contract
   it holds**: "the world as a resolution-independent material field," byte-identical to
   `fill_terrain` at 1 m voxels. That equivalence test is the tightest constraint in this prompt.
8. **`research/chunk-generation-optimization-log.md`** — what generation already costs and where.
9. **The C++ skill reference files** (`~/.claude/skills/cpp-heavy-templates/references/`) —
   **mandatory:**
   - `modular-architecture.md` §1–§2 — this pass adds several stages to `world/generation`; each is
     a module boundary decision, not a file dump. **§2's dependency-inversion section decides
     whether stages know about each other or about an interface.**
   - `templates-and-metaprogramming.md` §1 (concepts), §3 (**policy-based design** — a pipeline of
     independently-swappable stages is the worked case for this), §5 (type erasure — read before
     deciding whether the stage list is a tuple of types or a vector of handles).
   - `memory-and-performance.md` — the erosion solvers work over large float grids. Read the
     SoA/AoS and cache-friendliness section, and the `std::pmr` section, before allocating a
     dozen `std::vector<float>` per tile.
   - `concurrency-and-parallelism.md` — flow accumulation and the implicit solver have real
     dependency structure. Read the parallel-STL section (rule 36: check for a
     `std::execution::par` overload before reaching for the pool) and the "don't hand-roll a
     lock-free MPMC" rule.
   - `compile-time-performance.md` — explicit instantiation at the choke points (rule 13); this
     pass will add templates that only ever instantiate over two or three grid types.

---

## 2. The systems you are building on — verified file map

Checked against the files on 2026-09-06.

- **`world/generation/include/world/generation/heightmap_generator.hpp`** — the whole public
  surface is four functions: `generate_column_heights(int32 xOff, int32 zOff, int32 w, int32 d,
  float* out)` (row-major, X innermost), `generate_column_heights_spaced(float xStart, float zStart,
  int32 w, int32 d, float step, float* out)` (the sub-voxel variant added for the svo path — a
  7.8 mm-spaced 8×8 grid is one SIMD call), `height_at(float x, float z)` (single column, the
  analytic ground query), and construction from a seed. PIMPL, so FastNoise2 does not leak.
  **`height_at` is the load-bearing interface of the entire engine.** Its callers, all verified:
  `TerrainSampler` (classify + fill_brick + material_at), `HeightField`, `TerrainCollider`
  (its cache build and `ground_height`), `world/generation/tree_placement`, the aim query's
  `TreeLookup`, `SvoWorld::ground_height`, `tools/svo_render`, `tools/tree_dump`,
  `tools/mesh_dump`, the mesh path's `terrain_fill`, and the walk-mode backstop.
  **This is the crux of the pass**: a hydrologically-correct heightfield is a *global iterative
  computation over a grid*, not a pure function of (x, z). It cannot be an analytic noise call.
  See Group AM-A.
- **`world/generation/src/heightmap_generator.cpp`** — the four-octave node tree described above;
  `kPinnedFeatureSet = FastSIMD::FeatureSet::SSE2` with the throwing `new_pinned_node<T>()` helper
  and the comment explaining why (bit-identical worlds across machines, and SCALAR is not
  compiled).
- **`world/svo/include/world/svo/terrain_sampler.hpp`** — the occupancy rule verbatim: *"a voxel is
  solid iff its BOTTOM face height <= the column's surface height (sampled at the voxel's
  min-corner (x,z)), so at voxel size 1 the sampler reproduces every shipped chunk exactly"*, plus
  the band rules in metres (surface / soil / stone / water), `classify(Box)`,
  `fill_brick(origin, voxelEdge, Brick&)`, `material_at(voxelMin, voxelEdge)`, a region-wide
  `HeightField` at 0.5 m cells, `set_focus(center, radius)` building finer tiers (1/16 m), and a
  16 m tree grid. `static_assert(VoxelSampler<TerrainSampler>)`.
  **`test_terrain_sampler.cpp` proves byte-equality against `fill_terrain` itself.** That test is
  the thing that ties the new representation to the old world, and it must survive this pass.
- **`world/svo/include/world/svo/height_field.hpp`** + `src/height_field.cpp` (161 lines) — the
  cached height grid with min/max bounds per cell, which is how `classify` bounds sub-cell
  variation. A baked heightfield changes what this should be; read it before designing the tile
  store.
- **`world/materials/`** — Group AC's one-def-file-per-material registry: Air, Stone, Dirt, Water,
  Wood, Leaves, Sand, Grass, each with `Phase` (Gas/Solid/Liquid/Foliage), `Shading`
  (Lit/Water/Foliage), `LiquidPhysics`, tree flags and `fills()`. `world/materials/terrain_query.hpp`
  holds `TerrainBands { beach_band, soil_depth, grass_max_slope }` — **one copy, shared by
  `fill_terrain`, `TerrainSampler` and the aim query.** Adding a material is three small edits.
  This pass needs several (lithologies, ice, snow, limestone, gravel, clay) — use that mechanism,
  never an ID literal.
- **`world/generation/tree_placement`** — deterministic jittered-grid placement masked by
  height/slope, `TreeShape { Round, Conifer, Shrub }`, implicit box trunk + octahedron lobes,
  `tree_material_at`, `tree_intersects_box`. Plus the new, currently-untracked
  `world/generation/tree_skeleton.{hpp,cpp}` from last pass (space-colonization skeletons, pipe
  model, LAI leaf mass) and `tools/tree_dump`.
- **`world/svo/include/world/svo/tree_builder.hpp`** — `BuildParams { lod_center, lod_radius 4.0,
  uniform_lod, parallel_split_level 5 }`, `BuildStats` with per-level histograms, and the
  documented reason the split level is 5 (a coarser split left 4 jobs running for seconds:
  measured 12.4 s vs the ~1 s CPU-time total predicted).
- **Measured current costs**, for the budget: svo world ready **0.56 s**; a tree build **0.6–1.3 s**
  for 657k bricks / **395 MB**; the sampler's own share of that is reported separately in the
  `svo tree #N` log line. The mesh path's full-world load is 29.9 s. `research/
  chunk-generation-optimization-log.md` has the breakdown.
- **177 tests pass.** `world/generation` and `world/svo` hold a large share of them, including the
  equivalence test.

---

## 3. Standing rules for this pass

Prompt 002 §3 applies in full. The ones that bite hardest here:

1. **Determinism is not negotiable.** Same seed → byte-identical world, on any machine, across
   renderers and tools. The FastNoise2 SIMD pin exists for this. **An erosion solver that iterates
   to a tolerance is a determinism hazard**: floating-point summation order must be fixed, and any
   parallel reduction must produce the same result as the serial one or be made serial. Test it —
   see 300.
2. **The `TerrainSampler` ⇄ `fill_terrain` byte-equivalence test must pass at the end of this
   pass.** If the new terrain makes it meaningless (because `fill_terrain` reads the old noise),
   that is a real decision: either both read the new field, or the test is rewritten to assert the
   same *property* against the new source. Do not delete it. Say which you did and why.
3. **A visual change is verified by a viewed capture** on both backends, plus the statistics.
4. **Performance measured.** Generation-time budget for the whole macro pipeline, per region:
   **state a budget before you build it** and hold to it. §10.2's own framing allows "hundreds of
   ms regionally" against a 0.56 s world-ready. Anything that pushes world-ready past ~2 s needs
   the tiling/caching answer from AM-A, not a shrug.
5. **Tests green; new systems get new tests.** 177 is the floor.
6. **Materials are components.**
7. **No new dependencies without a written case.** This pass has real candidates (an FFT for
   Smith–Barstad orographic precipitation; a priority-queue/flow-routing library). Prefer: write
   the 20-line priority-flood yourself (Part 2 §11 says it is 20 lines for the integer variant),
   and for the FFT, check what is already available before adding one — a single 2D real FFT over a
   1–4 k grid is a well-understood ~200-line problem, and this repo's culture is to write the
   choice down.
8. **Read the goals.md group notes** before closing anything, and reopen goal 80 explicitly.
9. **Use your skills** — the reference files in §1.9, by name. The pipeline-of-stages design is a
   policy-based-design decision and the skill has the pattern.
10. **Delegate web research to one or two read-only subagents.** Good candidates for this pass:
    (a) reference implementations and numerical stability of the Braun–Willett implicit
    stream-power solver and Cordonnier's basin-graph depression routing; (b) practical 2D real-FFT
    implementations suitable for vendoring, and the Smith–Barstad transfer function's exact form.
    Give a specific question, keep coding while it runs, persist to `research/` and cite it.
11. **Commit and push per group.** Seven or eight commits here, not one. MSVC has no UBSan and this
    pass writes a lot of index arithmetic over large grids — exactly the class of code the Linux CI
    leg catches and this machine cannot.
12. **`git add` explicit paths, never `git add -A`.**

---

## 4. Task groups

### Group AM-A — The architecture change: a baked field behind an analytic interface (goals 295–299)

**This group is the whole risk of the pass.** Get it right and the remaining stages are
well-specified numerical work. Get it wrong and every downstream module breaks.

**295. State the problem and rank the options in writing.**
`height_at(x, z)` is a pure function called from twelve places including a per-tick collision query
and a per-brick SIMD grid call. A hydrologically-correct height requires priority-flood + flow
accumulation + an implicit solver over a *whole tile* — a global computation. Options:
 (a) **Baked tiles behind the same interface**: the generator owns a tile store; `height_at`
     resolves the tile (generating it on first touch, or from a prebuilt cache), then reconstructs
     the height by bicubic (or bilinear) interpolation of the tile's grid. Interface unchanged;
     cost per call becomes a lookup plus interpolation.
 (b) **Two-tier**: a cheap analytic macro field (continents, warped ridges) *plus* a baked
     residual (the erosion delta) that is zero until a tile is generated. Same interface, graceful
     degradation.
 (c) **Change the interface** to explicitly region-scoped and update all twelve callers.
Rank them, with the losers' specific reasons. My read, to check rather than accept: **(a) wins**,
with the tile store built at world-load time for the region the world covers, because the world is
already *static and bounded* (Group S) and the region is already known at startup
(`SvoWorld::geometry_for`). (b) is a good fallback if tile generation turns out too slow to do
eagerly, and its "degrades to today's terrain" property is genuinely useful during development.
(c) loses: twelve callers including a hot collision path, for no gain over (a).
**Check**: the ranking and reasoning in the log. The decision is implemented. If you rank
differently, one paragraph of why.

**296. `world/generation/field`: the tile store.**
A tile is a square of terrain at a fixed cell size, holding at minimum: elevation, and whatever
later stages need to be *queryable* (flow accumulation, precipitation, temperature, lithology
index, biome index, ice/snow mask, sediment thickness). Decide cell size and tile size from the
numbers, not by feel: Part 7 §9 computes its statistics on "a 512²-or-larger patch," the drainage
density band (2–12 km/km²) is quoted at 30 m-equivalent resolution, and the channel-head threshold
$A_c$ is 0.1–5 km². **Those three numbers together determine your minimum tile extent and maximum
cell size** — a tile smaller than a few $A_c$ cannot have a drainage network in it at all. Do that
arithmetic explicitly in the log and pick from it.
Layout: read `memory-and-performance.md`'s SoA section. Several parallel `float` planes over one
tile is SoA and is right; a struct-of-eleven-floats per cell is not.
**Check**: the arithmetic that chose cell size and tile extent is in the log with the three
research numbers it came from. A tile round-trips through serialization byte-identically. Memory
per tile is measured and reported, and the total for the shipped region is inside a stated budget.

**297. Reconstruction, and the derivative that everything else needs.**
`height_at` must be C¹-ish: the slope is used by the material banding (`grass_max_slope`), by tree
placement, and by the collider. Bilinear interpolation has discontinuous derivatives at cell
boundaries, which will show up as banding artefacts along a grid. Choose the reconstruction filter
deliberately (bicubic / Catmull-Rom / bilinear-plus-smoothstep) and state the cost, because this
runs in the per-brick SIMD path.
**Also provide an explicit `slope_at(x, z)`** rather than leaving every caller to finite-difference
`height_at` with its own epsilon — grep shows several already do, with different epsilons.
**Check**: a test asserts continuity of `height_at` and of `slope_at` across tile and cell
boundaries to a stated tolerance; a capture of a large flat-ish region at grazing incidence shows
no grid-aligned banding (viewed, both backends). The measured cost of `height_at` before and after
is in the log — it is now a memory lookup rather than a noise evaluation, so it may well be
*faster*; report what it actually is.

**298. Keep the grid-call fast path.**
`generate_column_heights_spaced` exists because a 7.8 mm-spaced 8×8 grid was one SIMD call instead
of 64. With a baked field it becomes 64 interpolations — unless you write the grid path to walk the
tile coherently. This is the hottest generation path in the engine (`TerrainSampler::fill_brick`).
**Check**: `fill_brick`'s measured cost per brick before and after, and the total build time for
the default region before and after, both in the log. A regression larger than 25% on build time
is a failure of this task; fix the walk, do not accept it. `TerrainSampler::debug_grid_calls()` and
`debug_grid_cache_hits()` already exist — use them.

**299. Eager vs lazy tile generation, and the loading experience.**
The world is static and bounded and the region is known at startup. Generating the region's tiles
eagerly at load is simplest and matches Group S's static-world decision; it also adds to the 0.56 s
world-ready. Measure it, and if it is more than ~2 s, use the existing loading-screen path (there
already is one: `render_loading_message("Building sparse voxel tree...")`) and report progress per
stage so the owner can see which stage costs what.
**Check**: measured world-ready time with the full pipeline, broken down per stage, in the log and
in the harness report. The loading screen names the current stage. `--frames 8` still completes.

---

### Group AM-B — Continents, climate and the fluvial core (goals 300–309)

This is Part 7 §10.2 stages 1–3 and Part 2 §11 stages 1–6. **The mathematics is in those sections;
do not re-derive it and do not substitute a remembered version of it.**

**300. Determinism harness for the solvers, first.**
Before writing an iterative solver, write the test that will keep it honest: the same seed and tile
produce a byte-identical elevation plane, run serially and in parallel, and on a re-run. Fix
summation order explicitly. **Do this before stage 1, not after stage 6** — retrofitting
determinism onto a parallel reduction is far more work than designing for it.
**Check**: the test exists and passes for a trivial stage; it then gates every stage added below.
State the parallelisation strategy you chose and why it is order-independent (or why it is serial).

**301. Stage 1 — continents with Earth-like hypsometry.**
Part 7 §10.2(1): low-frequency fBm plus plate-velocity domain warp (§2.1), with bimodal hypsometry
*by construction* — separate ocean and land crust fields, sea level cut so land is ~29%. Part 1 §2
(isostasy) and §3 (the height limit of mountains) bound the amplitude; Part 1 §10 has the synthesis.
Also stamp linear orogenic belts along plate-boundary curves with a low-pass "fake root" flexure
under them, per §10.1.
**Check**: acceptance test 3 (hypsometric curve) — the land-area-vs-elevation distribution is
bimodal with the land peak within a few hundred metres of sea level and land fraction near 29%
(state your measured value); the distribution is not Gaussian. Acceptance test 2 (power spectrum)
— 1D angle-integrated slope β in [1.6, 2.5], target ≈ 2, no spurious periodic peaks. Both numbers
in the log. Viewed capture of a continental-scale height dump (a false-colour PNG from a new
`tools/terrain_dump` is the natural artefact — see 318).

**302. Stage 2 — orographic climate.**
Part 7 §10.2(2): Smith–Barstad linear orographic precipitation as one FFT pair over the
continental heights → P(x,z); T(x,z) from latitude plus a 6.5 °C/km lapse rate; continentality as
a distance field. Part 5 §6 has the climate geography of deserts, which is the acceptance case:
rain shadows must land where the research says they land.
**Check**: a false-colour precipitation map, viewed, with a rain shadow visibly downwind of a
stamped orogen and a spillover pattern; the wet/dry ratio across a range is stated and compared to
the research's band. Temperature falls with both latitude and elevation at the stated lapse rate —
assert the lapse rate numerically. Cost of the FFT pair measured.

**303. Stage 3a — priority-flood depression filling.**
Part 2 §11(2): flood inward from the tile edges with a priority queue, raising pit cells to their
outlet level, so every cell drains; the +epsilon variant resolves flats with infinitesimal
gradients so flow directions are defined. O(n log n) float, O(n) integer.
**Check**: **zero internal basins after the pass, by construction** — acceptance test 9's
structural half, asserted mechanically over the whole tile, not sampled. Every flat has a defined
flow direction. Runtime measured and reported against the tile size.

**304. Stage 3b — flow routing and accumulation.**
Part 2 §11(3): D8 vs D∞ (Tarboton). Choose, and record the reason — the research gives it:
D8 is cheap and river-like in aggregate but has visible diagonal artefacts; D∞ partitions between
the two steepest downslope neighbours and gives smoother accumulation on hillslopes. Accumulate in
a single pass over cells sorted by decreasing filled elevation (the FastScape stack).
**Check**: accumulation is exactly conservative — the sum of contributions equals the cell count
(assert to the last unit for the integer form, or to a stated float tolerance). A false-colour
log-accumulation map, viewed, showing a dendritic network. The choice of D8/D∞ and its reason in
the log; if D8, show the diagonal artefact you accepted and say why it is acceptable.

**305. Stage 3c — implicit stream-power incision (Braun–Willett).**
Part 2 §1 (the SPIM) and §11(4): order nodes by decreasing elevation and solve each node's implicit
update in one sweep — stable at large timesteps, O(n) per step. Parameters from the §11 table:
start $m = 0.5$, $n = 1$, $\theta = m/n$ in 0.45–0.5, $K$ in the quoted band, uplift $U$, run to
equilibrium or to a chosen "age."
**Check**: acceptance test 7 (slope–area scaling) — log-log channel slope vs drainage area with the
exponent in [-0.6, -0.35], measured and stated. Acceptance test 1 (slope distribution) —
unimodal, with the skew-vs-mean-slope trend the research describes. Longitudinal profiles are
concave (Part 2 §8): plot one and view it. The solver's step count, wall time, and stability at the
chosen timestep in the log.

**306. Stage 3d — hillslope diffusion.**
Part 2 §4 and §11(5): linear diffusion (or Roering-nonlinear for steep terrain) with $D$ from the
table, setting valley spacing via $L_c = (D/K)^{1/(2m+2)}$.
The research is blunt that skipping this is *"the single most recognizable 'procedural terrain'
tell"* — knife-sharp noise ridges with no characteristic valley wavelength. That is exactly what
`lin_final_vk.png` shows today.
**Check**: the measured characteristic valley spacing matches $L_c$ from your chosen $D$ and $K$
within a stated factor; ridge curvature is finite (assert a bound on the second derivative along
ridge transects); acceptance test 8 (variogram log-log linearity over the intended scale band with
local $H$ in 0.46–0.77). Viewed before/after captures of the same seed and pose — **this is the
single most visually convincing capture in the whole prompt; make it a good one.**

**307. Stage 3e — the channel-head threshold.**
Part 2 §11(6): erode only where $A > A_c$ (0.1–5 km²), which sets drainage density.
**Check**: acceptance test 4 (drainage density 2–12 km/km² at 30 m-equivalent resolution) —
measured, stated, inside the band. The research notes this one test catches both "no rivers" (≈0)
and "noise gullies everywhere" (≫12); report which side your first attempt landed on, because that
is the finding.

**308. Stage 3f — the constant-drop check.**
Acceptance test 6: extract the network by constant-drop analysis and run the same t-test TauDEM
uses on real DEMs — mean first-order Strahler drop vs higher-order drop, $|t| < 2$. The research
calls it cheap and notes it ties network extraction to physics.
**Check**: the measured $|t|$, stated, under 2.

**309. Stage 3g — post-passes: meanders and base level.**
Part 2 §7 (meandering) and §11(7): meander the large-river centrelines at λ = 10–14 W with
migration as a time-jittered displacement; bankfull width $W = 3.0$–$4.0 \times Q^{0.5}$;
Part 2 §8 for the longitudinal profile and base level, §9 for deltas (choose the delta shape by
process regime — river/wave/tide — at each coastline sink).
**Check**: a measured meander wavelength-to-width ratio inside 10–14 for the largest rivers,
stated; every river polyline ends at sea level or a lake and every lake has a spill path
(acceptance test 9's remaining half, asserted). Viewed capture of a meandering river reaching a
delta.

---

### Group AM-C — The stencil passes (goals 310–316)

Part 7 §10.1's "fake convincingly" side of the line. Each is a stencil validated by a
cross-section or density statistic, not a simulation.

**310. Glacial carving above the snowline.**
Part 3 §1–§3 and Part 7 §10.1: select basins above the climate field's snowline, trace flow lines
downvalley, carve parabolic cross-sections with $b = 1.5$–$2$, width scaled to an ice-flux proxy
(drainage area), overdeepen below base level at confluences, truncate tributaries into hanging
valleys. Part 3 §3 (the glacial buzzsaw) bounds peak heights above the snowline.
**Check**: acceptance test 5 — fit $y = ax^b$ to ridge-to-ridge transects: fluvial $b \approx 1$
(V), glacial $b \to 1.5$–$2$ (U); or the V-index. Report measured $b$ for both populations
separately. Montgomery's magnitude check: glaciated valleys > 50 km² reach 2–4× the cross-sectional
area of fluvial neighbours — measure the ratio and state it. Viewed capture of a U-valley with a
hanging tributary.

**311. Coastal erosion and deposition.**
Part 3 §6–§7: cliff-retreat profiles, shore platforms, and depositional forms. Today the coastline
is a contour line.
**Check**: a viewed capture of a coast showing a cliff with a shore platform and a beach where
deposition should be; the beach material band still comes from `world/materials`'
`TerrainBands::beach_band`, not from a new hardcoded rule.

**312. Karst, on a lithology mask.**
Part 4 §8 (karst surface terrain) and §11 (the cave recipe): doline fields from clustered noise
with drainage-sink enforcement, on a carbonate-lithology mask.
**Check**: doline density and diameter distributions inside the research's bands, stated. Dolines
are drainage sinks — assert that the priority-flood pass is *re-run or locally exempted* so they do
not become the internal basins test 9 forbids. Say which you did.

**313. Caves — reopening goal 80, with the recipe.**
Part 4 §11 is written as a recipe for a voxel engine; Part 4 §4 has the geometry and scale
statistics (passage width/height distributions, branching, vertical extent); Part 7 §7.5 argues for
the heightfield-first hybrid: $d = h - y + N_3$ with cheese/spaghetti/noodle cave channels, cave
density suppressed near the water table.
**This is the one place this pass genuinely changes the occupancy rule**: the sampler's current
rule is "solid iff bottom ≤ column surface," which is a 2.5D world by construction. A 3D density
field makes solidity a function of (x, y, z). That touches `TerrainSampler::classify` (its
`BoxClassification` fast paths reason about columns), `fill_brick`, `material_at`, `HeightField`'s
bounding role, `TerrainCollider`, and the `fill_terrain` equivalence test. **Plan this before
writing it, and write the plan in the log.**
**Check**: caves exist and are enterable — a harness scenario walks into one and reports the body
inside a void below the surface, with zero inside-solid events (this needs Prompt 003's octree
collision; if 003 is not done, state that dependency and defer this goal rather than shipping
caves you can fall through). Passage width/height distributions inside the research's bands,
stated. Cave density suppressed below the water table — asserted. `classify`'s fast paths still
bound correctly (the classification must never say "uniform" for a box containing a cave wall —
assert over 10,000 random boxes against pointwise truth). Viewed captures: a cave mouth, a passage
interior, and a surface pose that must look unchanged where there are no caves.

**314. Deserts and dunes.**
Part 5 §1–§3 (wind-blown sand physics, the dune-morphology phase diagram, dune dynamics) and §10:
place dune fields where the climate field says desert, with the morphology chosen from the phase
diagram (sand supply × wind directional variability) rather than one dune type everywhere.
**Check**: dune wavelength and height inside the research's bands for the chosen morphology,
stated; the morphology map varies with the two phase-diagram axes (show two different dune types in
two different places, viewed); the dune field sits where the rain shadow is, which is a check on
302 as much as on this task.

**315. Lithology, stratigraphy and the erosion look.**
Part 7 §10.2(5) mentions "cliff stratigraphy from a warped layer field with per-layer hardness
feeding the erosion look," and Part 2's $K$ spans nine orders of magnitude across lithologies. Add
a lithology field with per-layer hardness that modulates $K$ in the SPIM pass, so hard layers form
benches and soft layers form slopes.
This needs new materials (limestone, sandstone/gravel, clay at minimum) — add them through
`world/materials/defs/`, three small edits each, no ID literals.
**Check**: a viewed capture of a cliff with visible differential erosion (benches on hard layers);
the material count moved through the registry with no consumer edits beyond the registry (the
Group AC property — assert it by grepping for material literals and finding none).

**316. Biomes and vegetation densities.**
Part 6 §1 (tree density by biome and successional stage), §2 (the self-thinning law), §3 (spatial
point processes — where trees actually are), §6 (the environmental-limit biome-mask table), §10
(synthesis); Part 7 §6 (biome maps, Whittaker on (T,P) plus altitude belts and ecotone sharpness)
and §8 (vegetation placement).
Replace the current height/slope mask with: density = f(slope, moisture, biome), a
jittered/Poisson-with-slack base plus cluster noise, and deterministic per-position IDs that
cascade across LOD bands.
**Check**: acceptance test 10 — stems/ha per biome inside Part 6's measured bands (the research
names 400–700 for temperate); state the measured value per biome. Boundary sharpness distribution
per §6.4. Determinism preserved (same seed → same trees, and the same tree ID at every LOD band).
Viewed captures of two adjacent biomes across an ecotone.

---

### Group AM-D — The acceptance suite, as a permanent gate (goals 317–320)

**317. `world/generation/validation`: the ten tests as code.**
Implement Part 7 §9's ten tests as a library: slope histogram (unimodality + skew-vs-mean trend),
power spectrum β, hypsometric curve, drainage density, valley cross-section $b$ / V-index,
constant-drop t-test, slope–area exponent, variogram/fractal dimension, hydrological coherence
(structural), and biome stem density. Each returns a measured value and a pass/fail against the
research band, with the band and its citation *in the code as a named constant with a comment*, so
the number and its source live together.
**Check**: all ten run on a fixed-seed tile and report. Every band constant carries its research
section reference in a comment. Unit tests feed each metric a synthetic input with a known answer
(a plane, a cone, a sine, a known-fractal surface) and assert the metric computes it correctly —
**test the instrument before trusting its reading.**

**318. `tools/terrain_dump`: the false-colour inspector.**
A CPU tool, in the same shape as `tools/svo_render` and `tools/tree_dump`, using Prompt 002's
shared option table: dump any tile plane (elevation, accumulation, precipitation, temperature,
lithology, biome, ice, sediment) as a false-colour PNG, plus hillshade for elevation, plus the ten
statistics as text. This is how every capture in this prompt gets made.
**Check**: it runs for every plane; the PNGs are re-saved through PIL before committing (the
`svo_render` stored-PNG lesson); the ten statistics printed match the library's.

**319. The nightly golden-seed statistics suite.**
Part 7 §9's closing recommendation: *"Run as a nightly golden-seed suite: fixed seeds, dumped
statistics, threshold assertions — the same discipline as the engine's existing `--verify-frame`
and slope-bound regression tests, applied to geomorphology."* Wire it as a harness scenario plus a
`ctest` label, over at least five seeds (one seed can be lucky — Prompt 001 learned this when a
single-seed test hid sevenfold variance in tree tip counts).
**Check**: five seeds × ten tests all pass, and the table is committed as a baseline. Add it to CI's
core leg (it needs no GPU). Report the *spread* across seeds for each metric, not just the mean —
that spread is what tells you whether a band is being met robustly or narrowly.

**320. Statistics for the *old* terrain, for the record.**
Run the ten tests against the current four-octave noise terrain and record the results before
replacing it. This is the before column, and it is the honest measure of how much this pass bought.
**Check**: both columns (noise vs pipeline) for all ten metrics, five seeds, in the log. Expect
noise to fail 4, 7, 9 outright and to sit at the wrong end of 1 and 8; if it unexpectedly passes
one, that is interesting and worth a sentence.

---

### Group AM-E — Integration and cost (goals 321–325)

**321. The equivalence test's fate.**
Decide and implement: does `fill_terrain` (the mesh path) read the new field too, keeping the
byte-equivalence test meaningful, or is the test rewritten as a property assertion? The mesh path
is the documented fallback renderer and is still tested.
**Check**: the decision and its reason in the log; the test passes in whichever form you chose; the
mesh path still renders (viewed capture, `--renderer mesh`).

**322. Collision agrees with the new terrain, including caves.**
`TerrainCollider` reimplements the old occupancy rule analytically. After 313 the world is 3D.
If Prompt 003 is done, its octree query already handles this and this task is a verification; if
not, this task must at minimum ensure the body cannot walk on a cave roof it should fall through.
**Check**: a harness scenario walks a path crossing a cave and reports zero inside-solid events and
zero standing-on-nothing events. State which collision path was in use.

**323. Trees and the new world.**
Tree placement now reads biome, moisture and slope from the field rather than height and slope from
noise; the skeleton work from last pass (`tree_skeleton.hpp/cpp`, untracked — **commit it as part
of this pass or explicitly leave it alone; do not let it be swept into an unrelated commit again**)
consumes species presets that should now come from the biome.
**Check**: determinism (same seed → same trees, same species, same skeleton bytes); species
distribution matches the biome map (assert per-biome species mix); viewed captures at two biomes.

**324. Cost, measured end to end.**
World-ready time, per stage, for the shipped region; memory for the tile store; `height_at` and
`fill_brick` costs before and after; tree-build time and brick count before and after (caves add
surface area and therefore bricks — the research warns vegetation and caves are the worst-case SVO
content classes). Compare against the budget stated in 296/299.
**Check**: the full table in the log and in the harness report. Any regression outside the stated
budget is named, with either a fix or a written reason it is accepted.

**325. What this pass deliberately did not do.**
Record, with a one-line reason each and a follow-up goal: real-time erosion; tectonic simulation;
glacial LEM; vegetation succession; planet-scale worlds; rivers as flowing *water* rather than as
carved geometry; sediment transport as a live system; seasons.
**Check**: the list is in `docs/goals.md` as open goals with Checks "to be defined by that pass,"
and in the pass log with reasons.

---

## 5. Sequencing and risk

- **AM-A first and completely.** Nothing else is meaningful until `height_at` resolves from a baked
  field.
- **AM-B in numerical order.** Each stage's acceptance test gates the next: there is no point
  tuning $K$ before priority-flood produces a drainable surface.
- **AM-C stencils are independent of each other** and can be done in any order after AM-B. **313
  (caves) is the highest-risk task in the prompt** because it changes the occupancy rule; consider
  doing it last, and consider deferring it if Prompt 003's octree collision is not in place — a
  cave you fall through is worse than no cave.
- **AM-D can start as soon as AM-A exists** — build 317 and 318 early, because they are how you
  will see everything else. Doing them last means debugging blind, which is how the last two passes
  lost time.
- **Commit and push per group**, five commits minimum.
- **Legitimate negative results**: if a stage cannot reach its acceptance band inside the cost
  budget, report the measured value, the band, the cost, and open a goal. Do not widen the band to
  pass, and do not blow the budget silently. Part 7 §10.1 is explicit that some of this is meant to
  be a "convincing costume" rather than the real thing — a stencil that passes the cross-section
  test is a success, not a compromise.

---

## 6. Explicitly out of scope this pass

- **Renderer, shading, LOD, GPU architecture.** Prompts 004 and 005.
- **Player physics and collision *design*.** Prompt 003. This pass consumes whatever collision
  exists and verifies against it.
- **Trees C4–C6 (190–192) and grass (193–195).** Prompt 007. This pass changes *where* vegetation
  goes, not how it is built or animated.
- **Water as a simulated fluid** (goal 199: shoaling, refraction, breaking, foam, currents). This
  pass carves river channels and lake basins; it does not make water flow.
- **Editing (goal 160), HashDAG, incremental rebuild (158), palette compression (157), DAG dedup
  (163).** Named in Prompt 004.
- **Planet-scale or streaming worlds.** The world is static and bounded; Group S decided that and
  it holds.

---

## 7. When you are done

1. Write **`research/earth-terrain-pipeline-log.md`**: the AM-A ranking; the tile-size arithmetic
   with its three research numbers; the D8/D∞ choice; every parameter you took from Part 2 §11's
   table and every one you departed from, with the reason; the ten-metric before/after table across
   five seeds; the cave occupancy-rule plan and what it cost; the full cost table; and every
   decided-against.
2. Add **Group AM** to `docs/goals.md`, goals 295–325 numbered exactly as above, each `[x]` with
   its Check recorded as performed (measured values, not intentions). **Mark goal 80 `[x]` in
   place** and note that the research reopened it, per the reopening rule.
3. Refresh `docs/progress.md`: the architecture map gains `world/generation/field`,
   `world/generation/validation`, the stage modules and `tools/terrain_dump`; the current-state
   paragraph describes the pipeline; add this pass's entries to *"Decisions that survived contact
   with evidence"* — including the reopening of goal 80 and its justification.
4. Add **`docs/terrain-pipeline.md`**: the seven stages, what each produces, which research section
   specifies it, the parameters and their sources, and how to run the validation suite. This is the
   document a future pass reads instead of re-reading 3,700 lines of research.
5. Update `CLAUDE.md` with operational deltas only (new tool, new `ctest` label, new world-ready
   time and what it is made of).
6. Update `Prompts/README.md`'s index row for 006.
7. Full suite green, five-seed validation suite green, both backends, and the before/after
   hillslope-diffusion capture viewed and named — that is the one the owner will look at first.

---

*Provenance: written by the side session on 2026-09-06 after reading
`world/generation/{include/world/generation/heightmap_generator.hpp,src/heightmap_generator.cpp}`
in full, `world/svo/include/world/svo/{terrain_sampler,height_field,tree_builder,brick_tree,
tree_layout}.hpp`, `world/materials/terrain_query.hpp`'s role via `terrain_sampler.hpp`,
`app/src/svo_world.{hpp,cpp}`, `docs/goals.md` (goal 80 and Groups S/V/W),
`docs/progress.md`'s decided-against list, and — as the specification — the section structure and
the synthesis sections of `research/earth-terrain-geomorphology-research.md` (Part 1 §10, Part 2
§11 including its full parameter table, Part 3 §10, Part 4 §11, Part 5 §10, Part 6 §10, Part 7 §9
and §10) plus `research/procedural-terrain-generation-survey.md` §§1–10. The captures
`lin_final_vk.png` and `svo_ground_hilltop.png` were viewed to characterise what the current
noise terrain actually looks like.*
