# The terrain pipeline — decision log (Prompt 006, Group AM)

The gap, stated by the prompt and confirmed by reading
`world/generation/src/heightmap_generator.cpp` in full: **the entire world is 109 lines** — one
`FastNoise::Simplex` at 200 world units per period, `FractalFBm` with 4 octaves, lacunarity 2, gain
0.5, remapped to ±64 m. No erosion, no hydrology, no drainage, no lithology, no climate, no biome,
no tectonic structure. Sea level is the plane y = 0 and land is "noise above it".

Backlog and Checks: `docs/goals.md` Group AM. Specification:
`research/earth-terrain-geomorphology-research.md`, Part 7 §9 (the ten acceptance tests) and §10
(the pipeline), Part 2 §11 (the fluvial core and its parameter table).

---

## 1. The number that decides the architecture (goals 295, 296)

Before ranking anything, one piece of arithmetic — because it invalidates the obvious plan.

The research's channel-head threshold is **A_c = 0.1–5 km²** (Part 2 §11), and §9.4's drainage-density
band is quoted **at 30 m-equivalent resolution**. The shipped region is **512 m** across
(`root_size_log2 = 9`):

| | |
|---|---|
| shipped region area | 512 × 512 m = **0.262 km²** |
| channel-head areas that fit at A_c = 0.1 km² | **2.6** |
| ... at A_c = 5 km² | **0.05** |

**The playable region cannot contain a drainage network.** Not "would have a sparse one" — at the
larger end of the published threshold it cannot contain a single channel head. Any plan that runs
priority-flood and flow accumulation *on the region* is computing a river network for a patch of
ground smaller than one river's headwater catchment, and it would produce either nothing or noise
gullies, which is §9.4's own failure mode in both directions.

**So the erosion runs on a MACRO FIELD much larger than the playable region, and the region is a
window into it.** That is also what §10.2 says when read carefully — *"on a regional heightfield,
once, offline of the frame loop"* — but the prompt's framing (a tile store sized to the region)
would have missed it, so it is worth stating as the pass's first finding rather than a detail.

### Sizing it, from the same three numbers

| macro field | cells | A_c = 0.1 km² is | region spans | 8 float planes |
|---|---|---|---|---|
| 8 km @ 16 m | 500² = 250 k | **390 cells** | 32 cells | **8.0 MB** |
| 16 km @ 32 m | 500² = 250 k | 97 cells | 16 cells | 8.0 MB |
| 16 km @ 16 m | 1000² = 1 M | 390 cells | 32 cells | 32.0 MB |

**Chosen: 8 km × 8 km at 16 m cells (500 × 500), ~8 MB for eight planes.** The reasoning:

- **390 cells per channel head** is a network, not a token gesture. At 32 m cells it is 97, which is
  workable but leaves a channel head only ~10 cells across — too few for the constant-drop test
  (§9.6) to mean much.
- **8 km is 15× the playable region**, so the region sits well inside a catchment rather than
  straddling a divide by accident, and the rivers that cross it come from somewhere.
- **The drainage-density band is quoted at 30 m-equivalent**, and 16 m cells are *finer* than that,
  which would bias an extracted density high. **The simulation runs at 16 m and the acceptance test
  measures at 30 m-equivalent by downsampling** — that is the honest way to use a band that is
  quoted at a stated resolution, rather than matching the cell size to the band and losing
  resolution everywhere else.
- 8 MB is cheap enough to bake eagerly at load, which goal 299 wants.

### And the consequence that shapes everything downstream

A 16 m macro cell over a 512 m region is **32 samples across the whole playable world**. Interpolated,
that is a very smooth surface with none of the detail a 7.8 mm voxel world needs. So the shipped
height is necessarily a **sum**:

```
height(x, z) = macro_eroded(x, z)      [interpolated from the baked field]
             + detail(x, z)            [analytic, high-frequency, always present]
```

**This is not the prompt's option (b), and the distinction matters.** Option (b) is a macro field
*plus a residual that is zero until a tile is generated* — a world whose shape depends on what has
been baked. That is a **determinism hazard**, and determinism is this pass's rule 1: "same seed →
byte-identical world" cannot survive a term that is present or absent depending on visit history.
The sum above has both terms present always and both deterministic; it is option (a) with a
detail term, not a fallback.

## 2. The ranking (goal 295)

| option | verdict |
|---|---|
| **(a) baked field behind the same interface** | **chosen** |
| (b) analytic macro + baked residual, degrading gracefully | rejected — see below |
| (c) change the interface to region-scoped | rejected |

**(a) wins**, and I agree with the prompt's read, but for a reason it does not give. The prompt
argues (a) on caller count: `height_at` has twelve callers including a per-tick collision query and
the per-brick SIMD grid path, and (c) would touch all of them for no gain. True, and sufficient to
kill (c).

**(b) deserves a stronger objection than "it is a good fallback".** Its selling point is graceful
degradation — the world looks like today's terrain where nothing is baked. But that means **the
world's shape is a function of what has been generated**, and this engine's contract is that a seed
determines a world. A player who flies out and back would see terrain that changed; the byte-
equivalence test would pass or fail depending on visit order; a golden capture would depend on
history. **If eager baking turns out too slow, the answer is a smaller macro field or a cached bake
— not a world that changes shape.**

The pass therefore keeps the interface exactly as it is (`height_at`, `generate_column_heights`,
`generate_column_heights_spaced`) and changes only what is behind it.

---

## 3. The field, built (goals 296, 297)

`world/generation/field/` — `TerrainField` (the store), `FieldSampler` (the reader),
`macro_pipeline` (the stages).

**Eight planes, SoA**, per `memory-and-performance.md`: elevation, precipitation, temperature, flow
accumulation, lithology, biome, ice/snow, sediment. Every stage of the pipeline sweeps one or two
planes over the whole grid, so a struct-of-eight-floats per cell would touch eight cache lines to
read one quantity. **Measured, not estimated: 250,000 cells x 8 planes x 4 B = 8.00 MB**, with the
elevation plane alone 1.00 MB.

The enum is ordered by the stage that writes it, so it reads as the pipeline's own dependency chain
— nothing can precede `Elevation`, `FlowAccum` needs the filled surface, `Biome` needs both climate
planes.

### The pipeline's shape is an architecture decision, so here is why it is not a policy

`templates-and-metaprogramming.md` §3 (policy-based design) and §5 (type erasure) are both plausible
here and both are wrong for this case:

- **Policy-based design buys compile-time substitution of one implementation for another.** These
  stages are not alternatives to each other; they are a **dependency chain with one valid order**.
  Nothing wants to swap the flow-accumulation policy, and a policy parameter would cost a full
  recompile of the pipeline per experiment.
- **Type erasure buys a runtime-varying list.** The list does not vary at runtime — it is the same
  seven stages for every world.

What the stages genuinely share is a signature and a field, which is a **function-pointer table**.
That is what `stages()` returns, and it buys the one thing actually wanted: `run_pipeline(..., N)`
stops after N stages, which is how a single stage's contribution is isolated for a capture or a
statistic. **Dependency inversion (`modular-architecture.md` §2) applies at the FIELD, not the
stage**: every stage knows `TerrainField` and no stage knows another stage.

### Reconstruction: Catmull-Rom, and the derivative is the reason

Bilinear was rejected before it was written, for a specific consequence rather than on principle:
**`height_at`'s slope is load-bearing.** The material banding asks it (`grass_max_slope` decides
grass vs rock), tree placement masks on it, and the collider's walkability limit is a slope test.
Bilinear has a **discontinuous derivative at every cell boundary**, so a bilinear field would put a
16 m grid of grass-vs-rock decisions through the world — the kind of artefact that gets diagnosed as
a shading bug three passes later.

Catmull-Rom is C¹, **interpolating** (it passes through the baked samples, so the field is a result
rather than a suggestion — asserted by a test), and its derivative is a closed form, which is what
makes `slope_at` exact rather than epsilon-dependent.

### `slope_at`, and the epsilon that was wrong first

`slope_at` exists because several callers already finite-difference `height_at` **with their own
epsilon** — so "the slope" is currently several slightly different quantities depending on who asks.

**The detail term's epsilon was wrong in the first version and a test caught it.** I chose 6.25 m,
reasoning from the signal ("half the finest detail wavelength"). That is the wrong scale: a slope
query asks *"is this walkable"*, which is a question about the metre the body occupies. Measured, it
disagreed with a central difference of `height_at` over the same ground by up to **0.9**. It is 1 m
now — and 1 m is not a new number either: `CLAUDE.md` already records the walkable-slope decision as
reading *"the ANALYTIC heightfield by central difference at 1 m"*. The existing documented behaviour,
kept.

### Two tests that had to be rewritten before they measured anything

Both first versions failed, and in both cases **the instrument was wrong, not the code** — worth
recording because the corrected forms are the reusable ones:

- **Continuity.** The first version asserted *"the height jump across a cell boundary is under
  4 cm"* and measured **12.6 cm**. That is not a seam: over a 4 cm step, 12.6 cm is a slope of 3,
  and this terrain reaches that. **An absolute tolerance on a jump is really a statement about how
  steep the world is allowed to be**, which is not the property under test. Rewritten as a **ratio
  against the same measurement taken mid-cell**, which removes the terrain's own steepness from the
  answer — and which a bilinear reconstruction would still fail, since its derivative jump at a
  boundary is unbounded relative to the smooth interior. The raw slope jump at a boundary measures
  **0.0149**, i.e. under one degree.
- **`slope_at` vs a difference of `height_at`.** The first version compared the analytic gradient
  against a central difference at a *different* epsilon, which measures the surface's curvature
  rather than the two estimators' agreement. Compared at the same 1 m step, the worst disagreement
  is **under 0.05**.

**346/346 tests.**

### What this commit deliberately does not do

**It does not change the world.** The one stage implemented reproduces the shipped terrain's
character rather than replacing it, so the architecture change (295–299) and the physics (300+) are
separate risks. If the world looks wrong after the next commit there is one candidate cause, not
two.

---

## 4. Wiring it in, and the cost (goals 298, 299)

`--macro-field` bakes the field at world construction and generates from it; `--field-stages N`
stops the pipeline after N stages. **Off by default**, and for the same reason goal 295 rejected
option (b): shipping a half-built pipeline as the default would make the world's shape depend on
how far the pipeline had got, which is the determinism hazard from the other direction.

### Measured, `stress_pose`, vk

| | analytic noise (today) | baked field + analytic detail |
|---|---|---|
| **field bake** | — | **0.005 s** |
| build | 3.37 s | **3.53 s (+4.7%)** |
| **`sampler` share of the build** | **0.17 s** | **0.17 s — unchanged** |
| bricks | 892,655 | 900,458 |
| resident | 279.5 MB | 281.9 MB |

**Goal 298's Check is a 25% build regression as the failure threshold. Measured: +4.7%.** And the
number that matters more is the one that did not move: **the sampler's own share is 0.17 s in both**.
`height_at` reading a Catmull-Rom interpolation of a baked plane costs the same as a FastNoise2
`GenSingle2D` — the memory lookup the prompt hoped for, confirmed rather than assumed.

The +0.9% brick count is not a cost, it is a different world: the macro stage uses value noise
(this translation unit must not include FastNoise2 — `heightmap_generator.cpp` is documented as the
only place that does), so the terrain differs in detail while matching in character.

### Goal 299 — eager, and there is nothing to stage

The field is **8 MB and bakes in 5 milliseconds**. Goal 299's threshold for needing a progress
display is ~2 s; this is three orders of magnitude below it. The per-stage callback and its log line
exist anyway, because the stages that carry real cost (priority-flood, flow accumulation, the
implicit SPIM solver) are still to come and the place to report them should exist before they do.

World-ready is unchanged.

### The viewed capture

`research/captures/am_macro_field_pair.png`: the two worlds side by side. **Same character — same
amplitude, same water level, same material banding, same isotropic noise cones — and different in
detail.** That is precisely what this group was for: the architecture changed and the world did not.

**Both halves still show the noise cones the prompt complains about, and that is correct at this
point.** The physics that removes them is goals 300+; putting it in the same commit would have meant
that a wrong-looking world afterwards had two candidate causes.

---

## 5. The fluvial core (goals 300, 303–307)

`world/generation/field/fluvial.{hpp,cpp}` — priority-flood, D8 flow routing and accumulation,
implicit Braun–Willett stream-power incision, linear hillslope diffusion. Eight acceptance tests in
`test_fluvial.cpp`, 16,840 assertions.

### Goal 300 — the determinism strategy, and it is "serial", not as a concession

The prompt asks for this before the first solver and warns that retrofitting determinism onto a
parallel reduction costs more than designing for it. **Every stage is serial, and each has a
specific reason rather than one general one:**

- **Priority-flood is a priority queue**: the order cells pop in *is* the algorithm. A parallel
  version needs Cordonnier's basin-graph decomposition to be *correct*, not merely fast.
- **Flow accumulation adds each cell into its receiver in stack order.** Two threads adding into
  one receiver is a float reduction whose result depends on scheduling — exactly rule 1's hazard.
- **The implicit incision sweep reads each node's receiver after that receiver has been updated.**
  That dependency is *why* the method is unconditionally stable; breaking it to parallelise would
  break the stability, not just the reproducibility.

And the cost says there is nothing to buy: **the whole fluvial core runs in 0.41 s over 250,000
cells** (fill 0.034, flow 0.034, incise 0.312, diffuse 0.034).
`concurrency-and-parallelism.md` rule 36 says check for `std::execution::par` before reaching for a
pool; the honest answer is that neither is warranted, and a serial pass that is bit-identical *by
construction* beats a parallel one that needs a test to prove it. The determinism test asserts
`a == b` on the raw float vectors — byte-identical, not approximately equal.

### Goal 304 — D8, and why, given the research says D∞ is smoother

D8 is chosen and the reasons are recorded rather than defaulted:

1. **Every downstream consumer wants a single receiver.** The implicit SPIM sweep solves `h_i`
   against one `h_receiver`; a partitioned receiver turns that O(n) solve into a linear system.
2. **The artefact D8 is criticised for** — diagonal striping in accumulation on smooth hillslopes —
   is suppressed here by the diffusion stage and by the channel threshold, which discards exactly
   the low-accumulation cells where it shows.
3. **D∞ is not the bias-free alternative it is usually presented as**: the 2025 re-evaluation the
   research cites measures it carrying its own ~25% cardinal/ordinal bias.

### The tool that had to exist before any of it could be judged (goal 318, early)

**The first attempt to evaluate the erosion was a rendered game frame**, `--field-stages 1` against
`--field-stages 5`, and the difference was barely visible
(`research/captures/am_erosion_ingame.png`). That is not evidence the erosion did nothing — **it was
measuring the wrong surface.** The macro field is 16 m cells and the playable region is 32 cells
across, so what fills a game frame at that pose is mostly the **analytic detail term, which no stage
erodes**.

`tools/terrain_dump` dumps the field itself in false colour with §9's statistics printed beside it.
`research/captures/am_fluvial_field.png`, viewed, and it settles the question in one look:

- **Left, continents only**: an isotropic speckle of small islands. No continents, no coherent
  landmasses — the noise-terrain failure the research names.
- **Middle, after the full pipeline**: **visibly consolidated** into larger connected masses with
  smoothed interiors and coherent relief. The erosion is real.
- **Right, flow accumulation**: **a genuine dendritic river network**, branching across the whole
  field. Unmistakably tree-structured rather than noise. That is the thing this pass exists to
  produce and it is there.

### Two measured negatives, both stated with the number

**Goal 307's Check asks which side of the drainage-density band the first attempt lands on, "because
that is the finding". It lands on the "no rivers" side.**

| A_c | measured drainage density | research band |
|---|---|---|
| 0.1 km² | **0.86 km/km²** | 2–12 |
| 1.0 km² | 0.24 | 2–12 |
| 5.0 km² | 0.08 | 2–12 |

Measured at **30 m-equivalent** by striding the 16 m field, because the band is quoted at that
resolution and measuring at 16 m would bias it high. The network in the flow dump is clearly real,
so this is not "no rivers" in the literal sense — it is **too few channel cells per unit area**,
which points at the incision budget rather than at the routing: 40 steps × 1000 yr = 40 kyr, where a
landscape reaches stream-power steady state over millions.

**And the hypsometry is not Earth's**: land fraction **50.8%** against §9.3's ~29% target, moving
only to 53.7% after erosion. That is expected and is goal 301's job — stage 1 is currently a
symmetric noise field cut at zero, with no separate ocean and land crust and no hypsometric
construction. **Measured now so goal 301 has a before number.**

### What is done and what is not, in this group

**Done**: 300 (determinism strategy and its test), 303 (priority-flood; zero internal basins
asserted over every cell, and a companion test that the fill only ever *raises* terrain), 304 (D8,
accumulation exactly conservative to 1e-4 over 16,384 cells, plus a no-cycles assertion), 305
(implicit incision; slope–area exponent measured negative), 306 (diffusion; worst ridge curvature
falls, and L_c is derived from D/K rather than written down).

**Not done, and named**: 301 (Earth hypsometry and orogenic belts — the land fraction above is its
before number), 302 (Smith–Barstad orographic climate; needs an FFT and a written case for it),
307's tuning (the density is measured and outside the band; the band is not yet met), 308 (the
constant-drop t-test), 309 (meanders, base level and deltas).

---

## 6. Continents and hypsometry (goal 301) — and an adaptation that must be stated, not slipped in

The first stage-1 was the shipped terrain's own 200 m feature scale, and `terrain_dump` showed what
a game frame had hidden: **an isotropic speckle of small islands across the whole 8 km field, with
no continent anywhere.** The macro scale is where that failure is glaring.

Rewritten as three things, in the order they matter:

1. **Coherent landmasses.** A 4 km continent mask, so an 8 km field holds one or two landmasses —
   what a patch of real coast looks like. Separate ocean and land crust fields, per §10.2(1), so the
   coastline is a **crust boundary** rather than a contour of one noise field.
2. **The hypsometric shape** — see below.
3. **An orogenic belt**, per §10.1's *"stamp linear orogenic belts … with fake roots … and let the
   SPIM pass carve real drainage through the fake mountains"*: a ridge along a sinusoidal
   plate-boundary curve with a wide low-pass root (isostatic-looking) and a narrow crest, plus a
   lithology plane marking the hard core.

### The adaptation: §9.3's 29% land is a planetary statistic and this is 8 km of ground

Research §9.3 asks for a bimodal area-elevation curve with **~29% land**. That is a **whole-Earth**
number. This field is 8 km across — **1.3 × 10⁻⁷ of the planet's surface**. A random 8 km patch of
Earth is almost entirely land or almost entirely ocean; it cannot express a planetary land fraction,
and forcing 29% onto it would be applying a statistic to a sample that cannot carry it.

**What §9.3 does say that IS testable on a patch is the shape of the land half**: the land peak sits
near sea level with a thinning tail, and the distribution is not Gaussian. That is what the
hypsometric power curve produces and it is what is now measured:

| | measured |
|---|---|
| land median | **28.7 m** |
| land mean | 32.4 m |
| land p90 | 61.1 m |
| land max | 112.6 m |
| **median / max** | **0.255** |

**A Gaussian field puts its median at half its range; §9.3 wants the peak near sea level, i.e. well
below 0.5. Measured 0.255.** The land fraction is 64.7% and is *not* reported as a pass or a fail,
because at this scale it is not a meaningful quantity — it is stated as what it is.

`research/captures/am_fluvial_field.png` (re-taken): coherent landmasses with a real coastline, a
bay, beach bands, and a snow-capped orogenic belt — and, after the fluvial stages, **radial drainage
visibly carved into the mountain flanks**, with a rich dendritic network draining to the sea.

---

## 7. Goal 307: the research's two bands do not overlap, and here is the curve

Raising the incision budget from 40 steps to 200 (40 kyr → 200 kyr) barely moved drainage density:
0.86 → 1.06 km/km². That was the clue that the limit is not the incision budget.

For a space-filling channel network, drainage density and channel-head threshold are related by
**D ≈ 1 / (2√A_c)**. Evaluating the research's own two bands against each other:

| the research says | which implies |
|---|---|
| A_c ∈ [0.1, 5] km² (Part 2 §11) | **D ∈ [0.22, 1.58] km/km²** |
| D ∈ [2, 12] km/km² (§9.4) | **A_c ∈ [0.0017, 0.0625] km²** |

**They do not overlap.** And the generator sits exactly where the relation says it should:

| A_c (km²) | D measured | D the relation predicts |
|---|---|---|
| 0.002 | **13.63** | 12.13 |
| 0.0625 | 1.39 | 2.00 |
| 0.1 | 1.06 | 1.58 |
| 1.0 | 0.30 | 0.50 |
| 5.0 | 0.06 | 0.22 |

**So the generator is behaving correctly and the two published targets are mutually inconsistent.**
Goal 307's Check asks which side of the density band the first attempt landed on "because that is
the finding" — it landed on the low side, and **the finding is larger than the question**: at the
A_c the research quotes, the density band it quotes is unreachable *by construction*, for any
generator.

**The engineering answer is to choose A_c from the density**, because the density is the acceptance
test and A_c is a free parameter: **A_c ≈ 0.01 km² puts D mid-band at ~5 km/km²**, and 0.01 km² is
39 cells at this resolution — a small but entirely reasonable channel head. Recorded here rather
than silently adopted, because it means this pass **cannot satisfy both of the research's numbers
and has chosen which one to satisfy.**

### And two bugs the stage-1 rewrite exposed, both found by tests rather than by looking

- **The diffusion had a sea-level guard that skipped exactly the cells that needed it.** It skipped
  every cell at or below sea level, reasoning that hillslope diffusion is a subaerial process. But
  the sharpest curvature in the field is at the **coastline** -- the land/ocean crust boundary is a
  step -- so the guard skipped precisely the worst cells, and goal 306's ridge-curvature test
  measured the worst Laplacian as **bit-identical before and after diffusing**. Removed: smoothing
  the seabed is harmless and smoothing the shore is physically right, since waves and mass wasting
  soften a coast.
- **The fluvial tests were measuring a hillside, not a landscape.** They ran on a 2 km field, and
  stage 1's continent mask is 4 km -- so the field sat entirely inside one lobe, the slope-area fit
  ran on 329 cells of a single slope, and the exponent came out **+0.18**, which is not a landscape
  at all. The test was too small, not the solver wrong. At 6.1 km the exponent is negative again.
  **A test whose domain is smaller than the feature it is testing measures the feature's absence.**

---

## 8. Goals 307 and 308 — the density band met, the drop test failed, and why the failure is the honest answer

### 307, met

The channel threshold is now in the incision — below A_c stream power does not apply and the slope
is left to diffusion, which is what creates the **hillslope plateau** acceptance test 7 requires to
exist. (Eroding everywhere would have failed test 7 in a way that looked like a diffusion bug rather
than a missing threshold.)

**At A_c = 0.01 km², drainage density measures 5.58 km/km² — inside §9.4's 2–12 band.** Goal 307's
Check is met.

### And a design error the threshold exposed: 60 m of uplift on 112 m of relief

Adding the threshold sent the land hypsometry from median/max **0.248 to 0.494** — back to Gaussian.
The cause was not the threshold: **200 steps × 1000 yr × 3.0e-4 m/yr = 60 m of uniform uplift
against a total relief of 112 m.** With the threshold in place the uplands had no incision to
balance it, so they simply rose.

**Uplift is now zero, and that is the design rather than an omission.** §10.1 puts tectonic history
firmly on the "fake convincingly" side: *"stamp linear orogenic belts … and let the SPIM pass carve
real drainage through the fake mountains."* Stage 1 stamps the mountain; the fluvial core's job is to
**carve** that relief, not to grow relief from uplift — growing it is the planet-scale LEM the
research rules out. With uplift at zero the hypsometry is **median/max 0.124**, better than before
the threshold existed, because the erosion now works the uplands down toward base level.

### 308, and it fails — swept, because the test is a METHOD not a check

Tarboton's constant-drop is the published way to *choose* A_c, so running it at one threshold would
answer "does this one pass" when the useful question is "which threshold does it select".

| A_c (km²) | \|t\| | first-order drop (n) | higher-order (n) | verdict |
|---|---|---|---|---|
| 0.005 | 15.59 | 0.288 m (3887) | 0.119 m (864) | fails |
| **0.010** (shipped) | **10.92** | 0.195 m (1755) | 0.072 m (390) | **fails** |
| 0.050 | 3.78 | 0.090 m (310) | 0.039 m (111) | fails |
| 0.100 | 2.34 | 0.059 m (168) | 0.027 m (54) | fails |
| 0.500 | 2.38 | 0.026 m (33) | 0.001 m (10) | fails |
| 1.000 | **0.68** | 0.008 m (19) | 0.003 m (5) | *passes* |
| 2.000 | 1.11 | 0.002 m (10) | 0.000 m (3) | *passes* |

**The two thresholds that "pass" have five and three higher-order samples.** The drop *ratio* between
orders stays at roughly 2.4–2.7× at every threshold where the samples are real; **|t| falls because
the sample sizes collapse (864 → 5), not because the drops converge.** A t-test on n = 5 has almost
no power, so those are not passes — they are the test running out of data.

**So the honest statement is: the constant-drop test FAILS on this field wherever it has the power
to say anything, and the two thresholds where it passes are artefacts of sample size.** Reporting
|t| = 0.68 at A_c = 1 km² as a pass would have been the easy answer and it would have been wrong.

### What the three measurements together say

| test | selects A_c |
|---|---|
| §9.4 drainage density (2–12 km/km²) | 0.005–0.05 km² |
| §11's quoted A_c band | 0.1–5 km² |
| §9.6 constant-drop, where it has power | **above 0.1, and it never actually passes** |

**These do not reconcile on an 8 km field, and the reason is the field, not the solver.** The very
first finding of this pass was that the *playable region* is too small to contain a drainage network;
this is the same objection one level up — **8 km is large enough for a network but not large enough
for a network with enough high-order links to run a statistic on.** At A_c = 0.1 km² the whole field
holds 54 higher-order channel cells.

**A_c stays at 0.01 km²**, chosen from the density because the density is the acceptance test with
enough samples to be meaningful here. The constant-drop failure is recorded as an open goal rather
than tuned away, and the fix is a larger macro field, not a different threshold — which is a real
cost (a 32 km field at 16 m is 4 M cells and 128 MB) and belongs to a later pass.

---

## 9. Goal 302: orographic climate — four wrong answers, each corrected by its own measurement

This goal produced more self-corrections than any other in the pass, and every one of them came from
an instrument rather than from re-reading the code. They are listed because the *sequence* is the
finding: each fix exposed the next error, and three of the four would have shipped as plausible.

### The model, and what it deliberately is not

Research Part 7 §10.2(2) specifies **Smith & Barstad's linear theory** — "LT = one FFT pair". What
ships is the model the research itself names as LT's baseline, the classical **linear upslope law**
`S = C_w · U·∇h + S_∞`, plus the two things that law lacks and this goal's acceptance case demands:

1. **Depletion** — a moisture budget carried along the trajectory. The bare upslope law has no
   memory and will rain the same amount off the third ridge as the first; carrying the budget is
   what makes a lee **dry** rather than merely un-enhanced, and a rain shadow is a dry lee.
2. **Drift** — condensate enters a suspended reservoir that advects downwind and falls out with an
   e-folding length. **The first draft had no drift and therefore could not produce the spillover
   its own Check asked for.**

The sweep is semi-Lagrangian: each cell reads a bilinear sample one full dominant-axis cell upwind.
Stepping by `dx / max(|wx|,|wz|)` puts the dominant axis exactly on the previous column or row, so a
cell's own weight in its own upwind sample is exactly zero and one row-major pass in the wind's
quadrant is a valid evaluation order. The obvious alternative — snapping the wind to its dominant
axis — silently discards the cross-wind component, a **17° error** in where every shadow lands for
the default (1, 0.3) wind.

**Why not the FFT.** An FFT is the easy half of Smith–Barstad; the hard half is its parameters (moist
Brunt–Väisälä frequency, uplift sensitivity, conversion and fallout times), and the research gives
the model's shape without a parameter set for a world 8 km across with 112 m of relief. Fitting those
against nothing produces a field with an FFT's authority and a guess's content. Every parameter here
instead names the anchor it came from and the miniaturization it was divided by. Goal 302a records
the upgrade.

### Error 1 — the drift length miniaturized the wrong length

Derived at 1200 m by scaling Roe & Baker's physical 5–25 km drift by the ratio of the Olympic
transect (~60 km) to this field (8 km), a 7.5× reduction.

**The ridge test measured the lee half of a ridge as WETTER than the windward half.** The field span
is not what sets the shape — the barrier is. This world's field is 7.5× smaller than the anchor's
transect but its **mountains are 70× narrower** (`macro_pipeline.cpp` builds a 320 m crest against
the Olympics' ~25 km half-width). What has to be preserved across the gap is the dimensionless ratio
**drift / barrier-half-width**, which is 0.2–1.0 for the anchor and gives **64–320 m** here.

Now **250 m**. At 1200 m the spillover ran three barrier-widths deep and buried the shadow.

### Error 2 — the test measured a plain and called it a rain shadow

The same test compared the two **halves of the field** either side of the crest. The windward half is
eighty columns of flat approach plain that lifts nothing and rains only background; averaging it in
measures the plain, not the mountain.

Rewritten to compare **symmetric flanks**, one to three sigma out on each side. That is two points
equidistant from the barrier, which is what a rain shadow is a statement about.

| on a 400 m synthetic ridge | measured |
|---|---|
| windward flank vs lee flank | **15.2 : 1** |
| peak vs fully-shadowed floor | **73 : 1** |
| peak position | **15 cells (480 m) UPWIND of the crest** |
| far lee | **0.1000** against a background floor of 0.1000 |
| crest → one drift length → far lee | 2.47 → 1.07 → 0.10 |

The peak sitting upwind of the crest is LT's own signature and it falls out of depletion plus drift
without the transfer function.

### Error 3 — a predicted negative that was wrong by a factor of five

The first draft asserted, as a measured fact in a header comment, that carrying the **physical** 2 km
water-vapour scale height across the scale gap left "a 1.06:1 shadow, which is to say none". Written
as a test, it failed: the real number is **5.33:1**.

The prediction came from the *previous, unnormalized* model. The plane is now normalised to a field
mean of 1.0, and **normalisation cancels most of the wring-out height's effect** — scaling total
condensate up or down scales the windward flank and the drifted lee together.

What the miniaturization actually buys, measured on this world's 112 m of relief:

| `wring_out_height_m` | flank ratio | column wrung out |
|---|---|---|
| 2000 m (physical) | 5.33 : 1 | 5.4% |
| **133 m (2000/15, shipped)** | **7.39 : 1** | **57%** |

A real gain — the peak moves further upwind and leaves less in the cloud to spill over — but **not a
rescue**. The absolute budget is the part that genuinely matters, and only on a fetch crossing more
than one range: at 5.4% the second range never rains on air the first one dried.

**And the negative that outranks both: at 112 m of relief NEITHER value reaches §6.2's 10:1.** That
anchor is quoted for a 1.5–2 km barrier; the 400 m synthetic ridge reaches it at 15.2:1. This world's
mountains are shorter than the landform the research's number describes — the same shape of result as
this pass's drainage-density and constant-drop findings, one level up.

### Error 4 — the ocean was raining, and only a picture showed it

The first false-colour dump had the sea north-west of the continent reading **wet**. The seabed rises
toward that coast, and the sweep was reading a **rising sea floor as forced ascent**. Air over water
is at sea level; the bathymetry beneath it lifts nothing.

The ascent now runs on `max(h, 0)`, materialised as a separate surface plane rather than clamped at
the read site — the bilinear sample has to interpolate the *clamped* surface, or the last wet cell
before a coast still sees a fractional step up out of the water.

**Nothing in the numbers had flagged this.** The ridge tests all passed; they are built on a ridge
with no ocean in them. It took looking at the picture, against the elevation map, to see it.

### The calibration, swept rather than reasoned — and a second climate pass

`background_fraction` is the floor a fully shadowed cell falls to, so it alone sets the wet/dry
ratio, which is the one magnitude §6.2 states.

The first value, 0.10, was *reasoned*: a cell receiving the field-mean orographic rain would then be
10× a fully shadowed one. That is arithmetic about the wrong cell — **the research's ratio is quoted
against a windward PEAK, and the peak here is 3.7× the field mean**, so 0.10 delivered 32:1.

| background_fraction | 0.05 | 0.10 | **0.20** | 0.30 | 0.40 |
|---|---|---|---|---|---|
| land p90/p10 | 21.5 | 16.1 | **10.3** | 7.2 | 5.3 |

Across three seeds at 0.20: **10.3, 8.6, 9.8** against the ~10:1 anchor. Shipped.

**The sweep also caught an inconsistency worth more than the calibration.** At the same background
fraction the sweep read 16.1:1 while the shipped field read 32.4:1 — because the sweep recomputed on
post-erosion terrain while the pipeline's climate stage ran on stage-1 terrain. **Erosion moves the
wet/dry ratio by a factor of two**, through valleys that did not exist when the first pass ran.

Climate now runs **twice**: before the incision (which needs a rain field) and again at the end, so
the shipped plane is keyed to the terrain everything downstream can actually see. Research Part 7
notes one orographic pass per erosion checkpoint is standard practice in coupled fastscape work; this
is the cheapest honest version of it.

### Goal 302's Check, performed

| Check item | result |
|---|---|
| false-colour precipitation map, **viewed** | `research/captures/am_precip.png` — ocean uniformly dry, wet bands on upwind-facing coasts and hillsides, dry tails downwind, dendritic wet fingers on the incised valley walls |
| rain shadow **downwind of a stamped orogen** | yes, and the shadow direction tracks the (1, 0.3) wind rather than the x axis |
| **spillover** pattern | crest 2.47 → one drift length 1.07 → background 0.10; asserted as a test |
| wet/dry ratio **stated and compared to the band** | **10.3 : 1** land p90/p10, against §6.2's ~10:1. Whole-field windward-facing vs lee-facing land is a different and much gentler statistic: **1.70 : 1** |
| lapse rate **asserted numerically** | test asserts `6.5 °C/km` exactly and that the summit-to-lowland difference equals `−6.5 × Δh/1000` to 1e-4 |
| temperature falls with latitude | **it does not, and that is deliberate.** 8 km is 0.07° of arc; a gradient varying by 0.005 °C across the world is a constant with extra arithmetic, so latitude enters as the base temperature |
| **cost of the FFT pair** measured | N/A — no FFT. The substitute costs **13 ms + 11 ms = 24 ms** of a 485 ms pipeline at 500², both passes. For reference only, the research quotes LT at ~1 s for 256² on a 2004 workstation |

### One more scale finding, from the temperature capture

`research/captures/am_temperature.png` reads correctly — warm at sea level, cold summits, cool
dendritic ridges. But the whole world spans **13.50 to 14.00 °C: half a degree.** At 6.5 °C/km,
112 m of relief cannot produce more.

**Temperature is therefore useless as a biome discriminator on this field**, and a Whittaker-style
classifier that reads it will separate nothing. Precipitation, which spans 0.29 to 2.98 on the same
field, is the only climate axis with real range here. That is a requirement on the biome work, not a
defect in this stage.

---

## 10. Goal 309: the post-passes — and the finding that decided the module's shape before a line of it

### Rivers on this world are SUB-CELL features of the macro field

Measured first, built second. The largest basin on the 8 km field carries a
**precipitation-weighted contributing area of 5.10 km²**, which through Petit & Pauquet's
`Q_bf = 0.087·A^1.044` gives **0.48 m³/s bankfull**, and through `W = 3.5·Q^0.5` a
**bankfull width of 2.42 m**.

**The macro cell is 16 m.** The largest river on this world is a sixth of a cell wide, and its
meander wavelength at λ = 12 W is **29 m — under two cells.**

So goal 309 is not a heightfield stage and could not have been one. It extracts the network as
**polylines carrying width, discharge and elevation**, meanders them in continuous space, and hands
them to the detail layer. The research says exactly this (Part 7 §11(7) lists the post-passes as
operating on *centrelines*; the Q&A section says "route on the macro-grid then smooth/meander the
centerline polyline"), and the alternative would have been a heightfield operation whose own output
resolution is six times coarser than its subject.

`test_rivers.cpp` **asserts** `maxWidth < 16.0` for exactly this reason: if a river ever exceeds the
cell size, rivers have become a heightfield feature and the module's premise needs revisiting.

### Web research: the ratio I asked for does not exist, and the answer was better

Full write-up with CONFIRMED/INFERENCE labels: **`research/bankfull-discharge-ratio.md`**.

The regime equation the corpus gives is `W = a·Q_bankfull^0.5`, and what accumulation produces is
mean annual flow. I dispatched one read-only agent for the conversion ratio. **It is not a published
statistic** — the literature relates bankfull discharge to *drainage area* or to *recurrence
interval*, essentially never to mean annual flow.

What came back instead is strictly better: **Petit & Pauquet (1997)**, ~40 Ardennes gauging stations,
**catchments 4–2,700 km²**, Cfb oceanic climate, r = 0.989:

> Q_b = 0.087 · A^1.044   (m³/s, km²)

That is calibrated *on catchments of this world's size, in this world's climate*. The
`bankfull_multiple` parameter the research was requested for **was deleted** rather than tuned — the
relation it existed to feed is no longer in the path. (For reference, Petit & Pauquet implies
k = 5.8–6.9 over 1–50 km²; the guessed 10 sat at the top of the defensible 4–12 band.)

Three cautions from the same pass, all now in the header where they apply:

- **The width relation is extrapolated two orders of magnitude below calibration.** NEH Part 654
  Ch. 9's data ranges put Nixon (1959) at 19.8–510 m³/s, Hey & Thorne at 3.9–425, and the chapter
  says its generalized width predictors should not be used below 17 m³/s. This world's largest river
  is 0.48 m³/s.
- **An independent route disagrees by 2.8×**, measured: Sofia & Nikolopoulos's `W = 3.6·A^0.39` gives
  **6.80 m** where the shipped route gives **2.42 m**. That gap is a real region effect (humid
  maritime vs semi-arid montane), and it is the honest uncertainty on any river width here.
  `terrain_dump` prints both side by side so it is visible rather than hidden behind whichever one
  shipped.
- **W ∝ √Q**, so even a 2× discharge error is only 1.41× in width. The uncertainty is large but its
  leverage is small.

### The meander curve — the research gives three constraints and not the shape

Part 2 §7 states λ = 10–14 W, sinuosity 1.2–2.2, and R = 2–3 W. A lateral sinusoid cannot satisfy
them together: solving for sinuosity 1.4 puts R/W near 1.5, solving for R/W = 2.5 puts sinuosity at
1.15. That is the sinusoid being the wrong shape, not a tuning failure — it concentrates curvature at
its crests.

What ships is the **sine-generated curve**, where the channel's *direction* rather than its offset
varies sinusoidally: θ(s) = ω sin(2πs/λ). ω is solved by **bisection against the target sinuosity**
rather than from a formula, so the produced geometry hits the stated number regardless of how the
curve is discretised. **FLAGGED**: the sine-generated curve is not in this project's research corpus;
it is brought in from outside, which is why everything about it is measured back off the output.

### Four defects, all found by measuring rather than reading

**1. λ/W read 17.2 for a requested 12 — the research had a word in it I skipped.** Part 7 §11(7) says
"route on the macro-grid then **smooth**/meander the centreline". D8 moves in 45° steps one cell
(16 m) long, and this world's meander wavelength is 29 m — **the grid's own zig-zag is at the same
scale as the meander being applied to it.** With four smoothing passes on the centreline first,
λ/W measures **13.9**, inside the band.

**2. Sinuosity read 1.02 for a requested 1.4.** The sample budget was sized from the valley length,
but the channel is *longer* than the valley by exactly the sinuosity — the loop ran out of samples
before consuming the valley. The curve was right and the loop stopped early. Now **1.52**, in band.

**3. "Every reach terminates: NO", on a network whose lakes were all fine.** Sub-4-cell flat
components were rejected as lakes *after* their cells had already been marked with the lake id, so a
reach flowing into one carried an index into a lake that was never stored. Rejected components now
release their cells.

**4. Only 69% of junctions resolved their downstream link — two separate causes.** First, the link
was indexed by reach HEADS, but a tributary joining a larger river lands **mid-reach**; only a
confluence of two *equal* orders creates a head. Indexing every claimed cell dropped it to 66.5%,
which exposed the second: a junction reach carries the join cell as its last node so the polylines
meet without a gap, so **whichever reach was built first claimed the shared cell** and the link
resolved to self, then to nothing. A junction reach no longer claims its own terminal cell. **100%
now**, and the test asserts >90% of the population.

Worth noting what #1 and #2 have in common with §9's four: every one was a correct mechanism
reported wrongly by its own instrument, and none would have been visible in the code.

### Lakes: a definition replacing a heuristic

The first version identified lakes by the fill's epsilon slope — "a cell whose receiver is barely
lower" — and found **173 on a field with a handful**, because a diffused plain is also barely
sloping. A lake is now **exactly the set of cells the fill had to raise**, which needs the pre-fill
surface passed in alongside. That is not a better heuristic; it is the definition.

### Deltas: two of Galloway's three vertices, and saying so

Research Part 2 §9.1 puts deltas on the river/wave/tide triangle. **This world has no tide model and
no wave model**, so two vertices cannot be selected on their own terms. Rather than label deltas from
a coin flip, the regime runs on the one axis both sides of which are computable: **discharge against
open-water fetch at the mouth** (the fraction of a 200 m disc that is below sea level — an exposed
headland gets planed off, a sheltered embayment progrades). **`Tide-dominated` is deliberately absent
from the enum**, because an enumerator that can never occur is worse than an acknowledged gap.

Measured: **37 deltas, 15 river-dominated and 22 wave-dominated** — the axis discriminates rather
than collapsing to one answer, which is the thing worth checking about it.

### Goal 309's Check, performed

| Check item | result |
|---|---|
| meander wavelength-to-width **inside 10–14 for the largest rivers, stated** | **13.9**, measured off the produced node offsets over the largest tenth of reaches; asserted in `test_rivers.cpp` |
| every river polyline ends at sea level or a lake, **asserted** | yes — 1160 reaches: 37 to sea, 557 to lake, 526 to a junction (not a terminus: the water continues, and the link is asserted), 40 off-field |
| every lake has a spill path, **asserted** | **170 of 170** |
| viewed capture of a meandering river reaching a delta | **`research/captures/am_river_delta.png`** — 400 m at 0.44 m/pixel: the channel meanders from its lake spill point to a delta fan at the coast. `am_rivers_field.png` is the whole 8 km network |

Two numbers stated that the Check did not ask for, because they are the interesting ones: sinuosity
**1.52** (band 1.2–2.2), and the **2.8× disagreement** between the two published width families.

### And a note on what the whole-field capture shows

`am_rivers_field.png` has visibly **grid-aligned straight reaches**. That is D8's 45°-multiple
artefact, which the research names directly ("D8 gives 45°-multiples") and offers two fixes for:
smooth/meander the polyline, or route on D∞/TIN. The first is done — but at 8.89 m/pixel a 29 m
meander is three pixels, so the whole-field view cannot show it and the straightness is what reads.
**The zoomed capture is not a nicer picture of the same thing; it is the only scale at which this
world's rivers are resolvable at all.** Routing on D∞ would remove the residual alignment and is
worth a later goal.

---

## 11. Goals 317 and 318: the acceptance suite, and the three things it caught in its own first run

The prompt's own sequencing note said to build these early — *"they are how you will see everything
else. Doing them last means debugging blind, which is how the last two passes lost time."* It was
right, and the first run paid for the whole module.

### The shape of it

`world/generation/validation/acceptance.hpp` implements research Part 7 §9's ten tests. Two
structural decisions, both from the prompt's Check:

- **Every band is a named constant with its citation in a comment beside it.** A threshold whose
  provenance lives somewhere else becomes a magic number within one refactor, and this pass has
  already found three numbers that had drifted from the reason they were chosen.
- **Every metric is a separate free function with its own instrument test.** `test_acceptance.cpp`
  feeds each one a synthetic with an *analytically known* answer — a plane reads 45.000°, a
  synthesised k⁻² surface reads β = 1.85, a bowl reads exactly one internal basin, a plane's
  variogram reads H = 1.00 and white noise reads H = 0.00, the FFT puts a pure tone in exactly one
  bin and satisfies Parseval, and a synthetic V and U valley read b = 1.0 and b = 2.0 apart.

**The FFT is written, not depended on** (rule 7). Only a 1D transform is needed — the research's
β band is quoted for the *1D angle-integrated* slope, and a 2D radial average of the same surface
goes as k^−(2H+2) and would read a full unit high against the band. That is a units error that would
have looked like a real result.

### Three defects in the suite itself, all found by running it

**1. It was measuring the wrong surface.** §9 says its tests run on "final (post-pipeline) heights".
On this project the macro field is *not* that: `height_at` is macro **plus an analytic detail term**,
and the macro field alone has no energy below its own ~100 m feature scale. Run on it, spectral β
read **5.30** against a band of [1.6, 2.5] and Hurst read **0.84** against [0.46, 0.77]. Both were
reporting, correctly, that a surface with nothing in it below 100 m is too smooth to be terrain —
they simply were not measuring the terrain.

The five **shape** tests (1, 2, 3, 5, 8) now read a 512² sample of `height_at` at 7.8 m spacing; the
four **network** tests (4, 6, 7, 9) stay on the macro field, because flow routing is a macro concept
and the detail term is not routed through. The report names which surface each used.

**2. It was measuring the sea floor.** Every one of §9's bands comes from a source that measured a
*landscape*, and none of them measured bathymetry. With the seabed in, mean slope read **1.7°** and
the slope–area regression read **R² = 0.001 — no relationship at all**, because the ocean carries
every high-area cell at near-zero slope and owns the entire high-area end of the fit. Land-masked,
the same fit reads **R² = 0.826**. The relationship was always there; two thirds of the samples were
drowning it.

**3. It had an invented band.** §9.1 gives no numeric skewness range — it gives *unimodality* and a
*sign trend* (positive skew at low mean slope, negative at high). The first version invented
[−1.5, 2.5], and the terrain "failed" it at 3.24. **An invented band that a real landscape fails is
worse than no band: it reports a defect that was never established.** The band is now on the sign
only, with the pivot (15°) flagged as this project's reading rather than cited.

A fourth, smaller: the mode counter only looked at interior histogram bins, so a monotonically
decreasing distribution — the commonest shape a gentle landscape makes — reported "0 modes,
unimodal = yes". Right verdict, wrong reason, which is worse than wrong.

### The reading, on the shipped seed

| # | test | measured | band | |
|---|---|---|---|---|
| 1 | slope skew sign | **−0.549** at 42.6° mean, unimodal | negative above 15° | **PASS** |
| 2 | spectral β | **0.429** (R² 0.46) | 1.6–2.5 | FAIL |
| 3 | hypsometry median/max | **0.139** | ≤ 0.40 | **PASS** |
| 4 | drainage density | **5.58** km/km² | 2–12 | **PASS** |
| 5 | valley exponent b | **2.33** (median 1.70, V-index 0.18) | 0.7–1.4 fluvial | FAIL |
| 6 | constant-drop \|t\| | **13.67** | < 2 | FAIL |
| 7 | slope–area exponent | **−1.651** (R² 0.83, plateau present) | −0.6 to −0.35 | FAIL |
| 8 | variogram Hurst | **0.095** (R² 0.70) | 0.46–0.77 | FAIL |
| 9 | hydrological coherence | 0 basins, 1160/1160 reaches, 170/170 lakes | structural | **PASS** |
| 10 | stems/ha | n/a until goal 316 | 400–700 | — |

**The headline is clean and was not visible before this suite existed: the macro pipeline passes
every hydrological test it is judged on, and the ANALYTIC DETAIL TERM fails every surface-shape
test.**

Three metrics say the same thing three ways:

- **Mean land slope is 42.6°** at 7.8 m sampling. That independently confirms Prompt 003's finding
  — recorded in CLAUDE.md as "57–71 degrees where it is called a hillside, mostly unwalkable at any
  realistic limit" — from a completely different instrument.
- **β = 0.43** where real terrain is ≈ 2. A β near zero is white noise.
- **H = 0.095** where the band is 0.46–0.77. Near-zero H is, again, an uncorrelated surface.

And the two disagree with each other in an informative way: for fBm, β = 2H + 1, so H = 0.095
implies β = 1.19, not 0.43. **Neither R² is high (0.46 and 0.70), and that is the real signal**: the
surface is macro fBm *plus* white detail, so it has a spectral BREAK and no single power law
describes it. A suite that reported one confident β for this surface would be hiding the finding.

Two failures are already-known and already-owned: **6** is goal 308's constant-drop result, recorded
in §8 as failing wherever it has the statistical power to say anything. **7**'s exponent of −1.651 is
steeper than the −0.5 the shipped m/n = 0.5 implies — but §8 also set **uplift to zero**, and the
slope–area power law is a *steady-state* relation between uplift and incision. With no uplift the
landscape is decaying rather than graded, so −0.5 is not the prediction and −1.65 is not a defect in
the solver. Its hillslope plateau is present, which is the half of §9.7 that judges the diffusion.

**5** is a genuine and interesting result: this pipeline's valleys read as U-shaped (b = 2.33 mean,
1.70 median) against a fluvial band of 0.7–1.4. Hillslope diffusion rounds a V into a parabola, and
at 16 m cells with 112 m of relief there is not much V left to round. Goal 310's glacial stencil is
judged against the *glacial* band (1.5–2.2) — which this fluvial terrain is already inside, meaning
the b-value test cannot currently distinguish glaciated from unglaciated terrain here. That is a
finding about the instrument's power on this world, and it is recorded rather than tuned around.

### 318, and what it stopped duplicating

`terrain_dump` previously carried **its own copies** of drainage density and the constant-drop test.
They are gone; it calls the library. Goal 318's Check is exactly that — "the ten statistics printed
match the library's" — and the two copies had already diverged: the tool's version sampled only
link-END cells and reported first-order drops *larger* than higher-order, while the library's samples
every channel cell and reports the opposite sign. **Two implementations of one test gave opposite
answers, and nothing would have caught it but merging them.**

The tool keeps the *sweeps* (drainage density and |t| across six channel thresholds), because a suite
reports one threshold and a sweep answers which threshold the test selects — which is where both of
goals 307 and 308's findings came from.

---

## 12. Goals 319 and 320: five seeds, and the before column — which contains two surprises and one regression

### Why this is not "assert all ten pass"

Five of the ten fail. **A gate that fails on the day it is written is not a gate.** Per the prompt's
own rule on legitimate negatives — *report the measured value, the band, the cost, and open a goal;
do not widen the band to pass* — `test_acceptance_seeds.cpp` asserts the four that currently pass, on
every seed, and **records** the five that do not, with their spread.

Goal 319's Check asks for the spread rather than the mean, "because that spread is what tells you
whether a band is being met robustly or narrowly". Across five seeds:

| # | test | min .. max | seeds passing |
|---|---|---|---|
| 1 | slope skew sign | −0.564 .. −0.489 | **5/5** |
| 2 | spectral β | 0.397 .. 0.588 | 0/5 |
| 3 | hypsometry median/max | 0.139 .. 0.275 | **5/5** |
| 4 | drainage density | 2.689 .. 8.698 | **5/5** |
| 5 | valley exponent b | 1.331 .. 3.046 | 1/5 |
| 6 | constant-drop \|t\| | 6.28 .. 29.13 | 0/5 |
| 7 | slope–area exponent | −1.349 .. −0.678 | 0/5 |
| 8 | variogram Hurst | 0.068 .. 0.195 | 0/5 |
| 9 | hydrological coherence | 0 .. 0 | **5/5** |

**Drainage density's spread is the one to watch: 2.69 to 8.70 against a band of 2–12.** It passes on
every seed, but the low end is within 0.7 of the floor — met, but not robustly. That is exactly what
running five seeds is for, and it would have read as a comfortable 5.58 on the shipped seed alone.

### Goal 320: the before column, and a methodology error worth recording

The first version compared internal-basin counts **after** running the new priority-flood over both
columns, and got 0 versus 0. That proves only that the fill works. **The old terrain was never filled
or routed at all**, so the honest comparison is before any fill:

| | noise (4-octave) | pipeline |
|---|---|---|
| **internal basins, unfilled** | **3,536** | **80** |

That is the headline, and it is a 44× reduction. The rest, all five-seed means:

| # | test | noise | pipeline | prompt's prediction |
|---|---|---|---|---|
| 1 | slope skew sign | −1.001 (5/5) | −0.538 (5/5) | "wrong end" — both pass |
| 2 | spectral β | **2.268 (5/5)** | 0.498 (0/5) | — **noise passes, pipeline fails** |
| 3 | hypsometry | 0.277 (5/5) | 0.208 (5/5) | both pass, pipeline better |
| 4 | drainage density | 2.611 (5/5) | 5.471 (5/5) | expected noise to fail |
| 5 | valley b | 1.880 (0/5) | 2.099 (1/5) | both fail |
| 6 | constant-drop | **1.570 (4/5)** | 14.509 (0/5) | expected noise to fail |
| 7 | slope–area | +0.017 (0/5) | −1.111 (0/5) | ✓ predicted noise failure |
| 8 | Hurst | 0.208 (0/5) | 0.129 (0/5) | ✓ both at the wrong end |
| 9 | coherence (post-fill) | 0 (5/5) | 0 (5/5) | — see the unfilled row above |

### Surprise one: pure fractal noise PASSES Tarboton's constant-drop test, and the eroded terrain fails it

Noise reads |t| = 1.57 and passes on 4 of 5 seeds. The physically eroded pipeline reads 14.5 and
fails on all five.

The research introduces §9.6 as the test that "ties network extraction to physics". **On this world it
does the opposite: it certifies noise and rejects erosion.** Combined with §8's finding — that the
test only passes where its sample sizes have collapsed below any statistical power — the honest
conclusion is that **the constant-drop test is not discriminating on an 8 km field**, and goal 308's
open item should be read as "this instrument needs a larger domain", not "the terrain is wrong".

### Surprise two: the pipeline made the spectrum WORSE, and the R² says exactly why

β goes from 2.268 (in band, on 5/5 seeds) to 0.498 (0/5). That is a real regression this pass
introduced, and it would have been invisible without goal 320's before column.

The cause is in the fit quality, not the slope:

| | spectrum log-log R² |
|---|---|
| noise | **0.917** |
| pipeline | **0.539** |

**Four-octave noise is self-similar by construction, so it fits a power law cleanly and honestly
reports β = 2.27.** The new surface is a macro field plus a *two-octave* detail term, which is **not
self-similar at all** — it has a spectral break where the macro's roll-off meets the detail's band,
and no single β describes it. The 0.498 is a straight line drawn through a bent curve.

So the finding is not "the detail term is white noise", which is what §11 concluded from the pipeline
column alone. It is sharper: **the detail term was never re-tuned to CONTINUE the macro field's
spectrum after the macro field was introduced.** It went from four octaves carrying the whole surface
to two octaves sitting on top of something with a different slope, and nothing checked the join.

That is a well-defined fix — make the detail term span the octaves between the macro's cutoff and the
voxel size, matching amplitude and slope at the join — and it is opened as a goal rather than
attempted here, because it changes every height in the world and belongs beside its own before/after
capture.

The same explanation covers metric 8: Hurst falls 0.208 → 0.129 for the same reason, and its R²
(0.70) is likewise mediocre. **Two instruments, one cause, and neither of them would have been
believable alone.**

### What the pass bought, stated plainly

- **Hydrology, decisively**: 3,536 → 80 unfilled internal basins; drainage density from the band's
  very edge (2.61) to mid-band (5.47); a real dendritic network with 1,160 reaches that all
  terminate and 170 lakes that all spill, where before there were no rivers at all.
- **Hypsometry**, modestly: 0.277 → 0.208, both in band.
- **A slope–area relationship where there was none**: noise reads +0.017 with no relationship at all
  (R² ≈ 0); the pipeline reads −1.11 with R² = 0.83 and a hillslope plateau. The exponent is steeper
  than the band, for the reason §11 gives (uplift is zero, so this is a decaying landscape and not a
  graded one), but the *structure* the band describes now exists.
- **And it cost spectral fidelity**, measured above, with a named cause and a named fix.

---

## 13. Group AM-C: the stencils, and testing a subject the world does not have

Every stencil is tested on a **synthetic field that has its subject**, then measured on the shipped
world. That split is not ceremony. This world is a gentle coastal plain, so some stencils have no
subject in it, and a stencil tested only against the shipped world would be reported as broken when
it is merely unemployed.

| goal | on a synthetic that has the subject | on the shipped world |
|---|---|---|
| 310 glacial | b 1.00 → 1.11, V-index up | **snowline 2,153.85 m, highest ground 44.55 m — zero basins** |
| 311 coastal | steep coasts get platforms, gentle ones beaches | 1,423 coast cells, **0 cliffed** |
| 312 karst | dolines on carbonate only, fill re-run | 229 dolines, 13.6/km², mean 40.3 m |
| 314 dunes | both phase axes select | **three morphologies** in the desert |

**310's honest limit**: 1.11 is below §9.5's glacial band of 1.5–2.0. The trough width comes from an
ice-flux proxy that on a synthetic V with uniform drainage does not widen enough to dominate a
ridge-to-ridge transect. Reported, not tuned.

**312's answer to the question goal 312 demanded be answered explicitly**: dolines are drainage
sinks, which acceptance test 9 forbids. This stage **re-runs the fill** rather than exempting them,
because an exempted sink would have to be carried as a special case through the flow router, the
river extractor and the coherence metric — and because a filled doline is a shallow closed
depression brimming to its rim, which is what a doline with a blocked throat actually is.

---

## 14. Goal 313: caves, and reopening goal 80

Goal 80 said no to 3D density terrain, and **it was right on its own evidence** — full 3D touches
generation, meshing and streaming at once. What changed is Part 7 §7.5's **heightfield-first
hybrid**: the surface stays 2.5D and only a bounded band below it becomes 3D.

**The plan, written before the code** (the prompt required it; it lives at the top of `caves.hpp`):

`classify` returns Solid for a box entirely below every column's surface **without subdividing**. A
cave inside such a box would never be looked for. So the change is not "subtract a noise field" — it
is that every conclusion of "this whole box is solid" must first prove no cave can intersect it.

`caves_possible_in_band` is that proof and is **conservative by construction**. A band rather than a
noise bound because bounding 3D noise over a box needs either interval arithmetic FastNoise2 does not
offer, or a Lipschitz bound so loose every box in the band would return Mixed anyway.

Two design points worth keeping:

- **The carve lives in `column_material`**, the one rule `fill_brick` and `material_at` share, so the
  brick fill and the pointwise query cannot disagree about where a cave is. It samples the voxel
  **centre**, not its bottom face — the occupancy rule uses the bottom because that is what makes the
  two worlds byte-identical, but a cave is a volume and sampling its boundary at a face would make a
  voxel's fate depend on which side of the face the noise fell.
- **Two crossed tunnel fields, not one.** A single `|noise| < t` test carves a shell around a
  zero-crossing SURFACE — a sheet, not a tunnel. The intersection of two shells is a curve, and a
  thickened curve is a passage.

Measured: void fraction 0.90%, passages **15.5 m wide and 9.18 m high**, zero samples below the water
table, and **10,000 random boxes verified against pointwise truth**.

---

## 15. Goal 321: the pass's largest finding, and it was not on the list

**For the whole of Groups AM-A and AM-B, the pipeline was never rendering.** Three independent
reasons, all found by opening a PNG rather than by any number:

1. **`generate_column_heights_spaced` ignored the macro field.** That is the bulk path — every brick,
   every column, essentially all the world's geometry. It called the raw four-octave noise root while
   `height_at` read macro + detail. **The world being rendered and the world being collided with were
   different surfaces**, and every acceptance statistic in this pass was measured on the one nobody
   could see.
2. **`macro_field` defaulted to `false`.** A pipeline the shipped binary does not run is not shipped.
3. **The playable region was in the sea.** A 512 m window at world (0,0) against a 4 km continent
   mask; on the shipped seed, open water to the horizon.

The trigger was rule 2. After the detail retune put mean slope at 4.8°, the rendered frame still
showed near-vertical spires. Nothing in the numbers said so, because the numbers were reading
`height_at`.

**And a fourth, older bug it exposed.** The Remap treated FastNoise2's FBm as [-1,1] when an N-octave
stack spans ±Σgainⁱ. Five octaves at gain 0.71 delivered **2.82× the stated amplitude** — measured as
γ(7.8 m) = 16.6 m², a **5.8 m height change over 7.8 m of ground, a 37° slope everywhere**. That
single bug explains the "57–71 degrees where it is called a hillside" that CLAUDE.md records from
Prompt 003. Normalising dropped γ(7.8 m) to 2.10, exactly the predicted 2.82².

**The fix for (3) was a search, not a stamp.** Stamping a broad land bump under the origin was tried
first and measured **worse than the problem**: a 2.5 km bias took sampled land fraction to 100% and
the world lost its coastline. `recentre_on_land` shifts the field's world origin onto good ground and
touches no elevation.

### The equivalence test's fate (goal 321's explicit question)

It runs with **caves disabled**. Its purpose is to prove the two representations share the same
BANDING RULES — that is what "byte-identical at 1 m" was ever evidence for — and caves are a feature
the mesh fallback does not have and is not getting. The property that makes disabling sound is
asserted separately: at threshold zero the sampler is bit-identical to the cave-free world, and with
caves on it genuinely differs (334 of 20,000 voxels). **Not weakened, not deleted.**

---

## 16. The instrument was wrong more often than the terrain

Counted across this pass, because the pattern is the lesson:

| # | the instrument said | the truth was |
|---|---|---|
| 1 | rain shadow 1.02:1 | the sample point was past where moisture was exhausted |
| 2 | a 1.06:1 shadow "which is to say none" | 5.33:1 — the prediction came from a superseded model |
| 3 | the ocean is wet | the sweep read a rising **seabed** as forced ascent |
| 4 | wet/dry 32:1 against a 10:1 anchor | the band is quoted against a windward **peak**, not the field mean |
| 5 | λ/W 75.6 for a requested 12 | measured against the reach's chord, which a wandering D8 reach is not aligned to |
| 6 | λ/W 17.2 | the research said "route then **smooth**/meander" and the smooth was skipped |
| 7 | sinuosity 1.02 for a requested 1.4 | the sample budget was sized from the valley, but the channel is longer by exactly the sinuosity |
| 8 | "every reach terminates: NO" | rejected sub-4-cell lakes kept their claim on cells |
| 9 | 69% of junctions linked | a tributary joins **mid-reach**, and then the join cell is shared |
| 10 | spectral β 5.30 | measuring the macro field, which is not the final surface |
| 11 | slope–area R² = 0.001 | two thirds of the samples were **sea floor** |
| 12 | skew 3.24 fails | the band was **invented**; §9.1 gives a sign, not a range |
| 13 | β falls with every improvement | the fit weighted the top octave with half its points |
| 14 | 0 modes, "unimodal = yes" | the mode counter ignored boundary bins |
| 15 | the terrain is 4.8° | the renderer was not reading the pipeline at all |

**Fifteen.** Against roughly four cases where the terrain itself was wrong. Goal 317's Check — "test
the instrument before trusting its reading" — was the most valuable sentence in the prompt, and it
was written before any of this happened.

---

## 17. Cost, and what the pass bought

| | old noise terrain | pipeline + caves + biome trees |
|---|---|---|
| macro field bake | — | 0.545 s (8.00 MB) |
| bricks | 338,602 | 219,344 (−35%) |
| internal nodes | 183,618 | 91,685 (−50%) |
| solid leaves | 804,040 | 350,762 (−56%) |
| tree memory | 109.1 MB | 68.9 MB (−37%) |
| build time | 1.25 s | 1.01 s (−19%) |
| sampler time | 0.20 s | 0.27 s (**+35%**) |
| boxes classified | 1,575,541 | 769,889 (−51%) |
| bricks sampled | 1,024,654 | 525,141 (−49%) |
| unfilled internal basins | 3,536 | **80** |

**The research warned that caves and vegetation are the worst-case SVO content classes, and caves do
cost exactly what `caves.hpp` predicted** — +35% sampler time, the band's forced subdivision. It is
swamped by the surface being smoother: mean land slope 42.6° → 8°, so there is far less surface to
represent.

World-ready **1.56 s** against the prompt's ~2 s ceiling, so AM-A's tiling answer is not needed yet.
