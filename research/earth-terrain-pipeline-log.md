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
  step -- so the guard skipped precisely the worst cells, and goal 306'''s ridge-curvature test
  measured the worst Laplacian as **bit-identical before and after diffusing**. Removed: smoothing
  the seabed is harmless and smoothing the shore is physically right, since waves and mass wasting
  soften a coast.
- **The fluvial tests were measuring a hillside, not a landscape.** They ran on a 2 km field, and
  stage 1'''s continent mask is 4 km -- so the field sat entirely inside one lobe, the slope-area fit
  ran on 329 cells of a single slope, and the exponent came out **+0.18**, which is not a landscape
  at all. The test was too small, not the solver wrong. At 6.1 km the exponent is negative again.
  **A test whose domain is smaller than the feature it is testing measures the feature'''s absence.**
