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
