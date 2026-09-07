# Frame time — decision log

Prompt 004, Group AK (goals 244–275). **In progress**: AK-A (244–248) is done. Written the way this
repo's logs are: every measurement, every "decided against", and the things that turned out to be
wrong — including the ones I got wrong.

---

## 1. The first measurement changed the question (goal 244)

**Vsync was hardcoded.** `RenderContext::present()` called `swapchain->Present(1)` with the comment
"vsync on -- correctness over speed is M1.4's own done-when". With that in place, `present` time is
the *panel's*, not the renderer's, and every frame-time percentile downstream measures the display:
a 6 ms frame reads as 6 ms and a 7 ms frame reads as 12, which is a step function, not a signal.
It is a setting now (`--vsync` / `--no-vsync`, on by default because that is the shipping
behaviour), and every number below is measured with it **off**.

### And the headline number is not what the brief expected

The brief's baseline table says **76 fps / 13.15 ms**, and its instruction was: *"76 fps at 13.15 ms
with only 3.2–6.3 ms of GPU march+resolve means roughly half the frame is not the marcher. Before
you optimise the marcher, find out what the other 7–10 ms is."*

Measured on `stress_pose` today, vsync off, RelWithDebInfo, vk:

| | value |
|---|---|
| frame ms mean / median | 5.36 / **5.21** |
| frame p95 / p99 / max | 7.09 / 10.23 / 13.68 |
| GPU march (median) | **4.93** |
| GPU whole-frame range | 5.19 |
| phase coverage | 96.6% |

**The median frame is 5.21 ms — 192 fps — and 4.93 ms of it is the marcher.** There is no missing
7–10 ms. The gap the brief asked me to hunt does not exist on this build: the frame is
**GPU-bound on the march**, and the 76 fps baseline predates Prompt 002 (which turned the ImGui
overlay off for scenarios) and Prompt 003. Vsync on vs off at the same pose is 5.46 vs 5.21 ms
median — so vsync was *not* manufacturing the old number either, at this pose.

**Verdict, in one sentence: the frame is GPU-bound on the primary+secondary march, the average is
already comfortably past the 150 fps target, and the owner's complaint is entirely about variance.**
That reframes the whole prompt, and it is why AK-A comes first.

---

## 2. Secondary rays are half the marcher (goal 246)

Six traversals per shaded pixel (1 primary + 1 shadow + 4 AO). `stress_pose`, march GPU ms:

| configuration | vk | d3d12 |
|---|---|---|
| shadows + AO (shipping) | 4.90 | 5.12 |
| `--no-ao` | 3.09 | 3.70 |
| `--no-shadows` | 4.41 | 4.83 |
| both off (primary only) | **2.50** | **3.18** |

- **AO costs 1.81 ms — 37% of the marcher** — for four rays, so ~0.45 ms per AO ray class.
- **Shadows cost 0.49 ms — 10%** — for one ray.
- **Secondary rays together are 49% (vk) / 38% (d3d12) of the march.**
- The primary ray alone is 2.50 ms, i.e. 400 fps. **Even the primary is not the bottleneck at this
  pose**, which is the reframing goal 246 asked for and it should be said plainly: any plan that
  optimises primary traversal is optimising the smaller half.

---

## 3. The rebuild storm is real, and it owns the p99 (goal 247)

`fly_transect`, vsync off, with and without world rebuilds (`--no-rebuild` freezes the tree, which
makes the world go stale — that is the point):

| | rebuild ON | rebuild OFF |
|---|---|---|
| mean | 4.97 | 4.42 |
| median | 4.64 | 4.01 |
| p95 | 7.17 | 5.96 |
| **p99** | **13.16** | **6.89** |
| trees built | 3 | 1 (the initial one) |
| slow frames (>20 ms) | 7 of 2014 | 3 of 2265 |
| ...caused by upload or build | **5** | **0** |

**p99 halves, and every upload- and build-caused stall disappears.** The diagnosis in the brief's §0
is confirmed. What is left with rebuilds off is three frames, two of which are the harness's own PNG
capture (below) and one a 35 ms `present`.

### The 180 ms frames were my own instrument, and finding that out was the useful part

Both conditions showed a **max near 180 ms**, unchanged by suppressing rebuilds — which looked like
a second, larger problem hiding behind the first. The slow-frame line printed seven phases summing
to **0.7 ms of 180**, so the frame was 99.6% unaccounted.

It is the **`capture` phase**: a staging copy, a `WaitForIdle` and libpng, which `CLAUDE.md` already
documents at 200+ ms and which Prompt 002 added as an eighth phase. The *exit summary* included it;
the *live slow-frame line* still printed the original seven. A breakdown that does not add up to its
own total is not a breakdown, and this one cost a hypothesis before I noticed the exit summary
disagreed with it. `capture` is in that line now.

**So there is no mystery stall.** Excluding capture frames, the worst real frames are: with rebuilds
on, 39.0 ms (upload), 24.8 (post, while building), 22.7 (upload), 21.1, 20.0 — all rebuild-related;
with rebuilds off, 35.4 / 15.2 / 11.3 ms, all `present`, no upload or build cause at all.

### A vacuous measurement I caught before reporting it

The first attempt at this comparison produced *identical* numbers for both conditions, including
"3 trees built" with rebuilds supposedly off. The cause: my scenario-copy step used
`sed 's|^option --no-taa$|...|'` to inject flags, and **`fly_transect.scn` has no `option` lines at
all**, so the substitution matched nothing and both runs were the default configuration. The
`stress_pose` measurements in §1 and §2 are unaffected (that file does have the line), and the
fly_transect numbers above were re-taken by inserting after `backend`, which every scenario has.

That is the sixth vacuous instrument in this arc, and the first I caught by checking the instrument
rather than by disbelieving the result — the check was "does the flag appear in the file I actually
ran", which took one grep.

---

## 4. The rebuild storm, stopped (goals 249–253) — a stopgap, labelled as one

`> lod_radius * 0.5f` tied how *often* the world is rebuilt to a *detail* parameter, and at the 4 m
default that asked for a full 400 MB rebuild every **2 metres**. The trigger is now a named policy on
`SvoWorldOptions` with four parts: a trigger distance, hysteresis, a minimum interval measured from
the last **adoption** (not the last build start — the upload is 13–21 frames of `UpdateBuffer`
traffic after the build ends), and a speed gate.

### What it bought

`fly_transect`, vsync off, vk:

| | before | after |
|---|---|---|
| median | 4.64 | **4.02** |
| p95 | 7.17 | **5.89** |
| **p99** | **13.16** | **8.52** |
| slow frames (>20 ms) | 7 of 2014 | **4 of 2271** |
| ...caused by upload or build | 5 | **2** |
| trees built | 3 | 2 |

**p99 −35%**, against a rebuilds-*disabled* floor of 6.89 ms — so this closes about two thirds of the
gap between the shipped behaviour and never rebuilding at all.

### The trigger distance turned out not to be the lever, and the ramp is what showed it

I shipped 24 m first, on the reasoning that a 2 s build at 40 m/s is outrun after 80 m anyway. Then
I ramped it, because a number chosen by argument is not a measurement:

| trigger | p99 (ms) |
|---|---|
| 4 m | 9.32 |
| 8 m | 8.19 |
| 16 m | 8.06 |
| 24 m | 8.28 |

**Flat, inside the run-to-run spread.** The reason is the speed gate: during a fast flight *no*
rebuild triggers at any distance, so the trigger distance only decides behaviour while the camera is
moving **slowly** — which is exactly when a rebuild is cheap to absorb. **The AK-B win is goal 250's
speed gate, not goal 249's distance.**

That mattered, because the distance is *not* free in the other dimension. Rendering the same pose
with the LOD centre displaced (`tools/svo_render --lod-center`, the deterministic reproduction
CLAUDE.md names for exactly this):

| LOD-centre offset | staleness, mean levels | pixels changed |
|---|---|---|
| 4 m | 0.03 | 0.2% |
| **8 m** | **0.27** | **2.9%** |
| 16 m | 5.11 | 36.7% |
| 24 m | 13.70 | **45.3%** |

**Free to 8 m, off a cliff by 16.** Capture: `research/captures/ak_lod_staleness.png` — at 24 m the
near-field terrain is visibly chunky, which is the cost my first draft would have shipped silently.

**So the shipped value is 8 m**: all of the frame-time win, and 94% of the image quality 24 m threw
away. Two measurements, opposite directions, one obvious answer.

### Goal 250's check FAILS, and the reason is structural

The check is "zero rebuilds completing more than one trigger-distance behind the camera". Measured
adopt lag on `fly_transect`: **135.2 m and 33.9 m**, against an 8 m trigger. Both fail.

- The 135 m one is the **initial** build, requested at spawn and adopted after the camera has already
  flown; no trigger policy governs it.
- The 33.9 m one is a build that triggered while the camera was slow and then **accelerated during
  the ~2 s build**.

**Deferring cannot bound the adopt lag, because the camera's speed after the trigger is not knowable
at trigger time.** I chose deferral over velocity prediction (goal 250 permits either) because
prediction needs a reliable build-duration estimate to extrapolate by, and the measured duration
varies **1.89–3.60 s** with terrain complexity — a prediction would be wrong by tens of metres
exactly when it matters. But deferral does not fix it either. **This is a structural limit of
"rebuild the whole world around a point", and it is the argument for AK-C/D rather than a tuning
failure.**

### Goal 251: decided against, with the numbers

`build_job` calls `sampler.set_focus(camera, 4 * lod_radius)`, rebuilding a 1/16 m height field over
a 16 m radius on every build, and 251 asks for cross-build reuse. Measured first:
**`sampler` is 0.15–0.19 s of a 1.89–3.60 s build — 5–9%.**

Reuse would recover at most part of that, and only when consecutive focus regions overlap — which,
with the trigger now at 8 m and the speed gate suppressing rebuilds during flight, is a handful of
builds per minute. Against `fill_brick` at ~60% of build time (goal 162's own note), this is the
wrong 8% to optimise. **Decided against, and the number is why.** It becomes worth doing if AK-C's
per-cell rebuild makes builds frequent and small, which is the opposite regime.

### Goal 161: fixed, and the test that hid it now cannot

`fill_terrain` truncated the surface height toward zero (`static_cast<int32_t>`) instead of flooring,
so a column at −3.4 m became −3: **underwater terrain sat one voxel high, everywhere below sea
level.** Above sea level truncation and flooring agree, which is precisely why the sampler-vs-fill
equivalence test — which **skipped every negative-height column** — never saw it.

Both fixed together: `fill_terrain` floors, and the test's skip is deleted. It now compares
**196,608 voxels with 0 skipped** (was ~2/3 of that with the underwater third excluded). A test that
excludes the region where two implementations disagree is not an equivalence test.

### Goal 253: what this did NOT fix

- The world still goes **stale between rebuilds** — 2.9% of pixels at the shipped 8 m trigger, and
  the capture above shows what the 24 m version would have looked like.
- The **adopt lag is unbounded** (above), so a fast flier is always looking at a tree centred where
  they were, not where they are.
- The **whole-tree cost is untouched**: still 1.89–3.60 s of CPU and 431–688 MB re-uploaded per
  rebuild. AK-B makes it happen less often; it does not make it cheaper.
- The single worst frames are still the **tree swap and its upload** (2 of the 4 remaining slow
  frames), because adopting a tree still means creating and filling GPU buffers for the whole world.

---

## 5. The streamable structure, designed before written (goal 254)

### The ranking, and I rank it differently from the brief

The brief's read is *"do both, shallow-grid first"* — a grid of shallow trees, with ESVO-style paging
applied **within** a cell. Having done the arithmetic I rank it: **grid of shallow trees, and paging
within a cell is then unnecessary, not merely second.** One paragraph of why, as the goal asks:

ESVO's relative-within-block addressing exists so that *a piece of a tree can be relocated while the
rest stays valid* — which is only a problem because pieces of one tree point at each other by
absolute offset. **A grid of independent cells has no cross-cell pointers at all**: a ray leaving a
cell does not follow a pointer, it re-enters the grid at the top and indexes the next cell
arithmetically from its own position. So the unit of relocation is the cell, a cell is built and
uploaded atomically, and nothing inside it ever moves independently of the rest of it. The flat
offsets already in `tree_layout.hpp` remain correct **unchanged**. The grid does not make relative
addressing easier; it makes it *moot*. Paying for a `far` bit, a per-block far-pointer table, and a
page allocator to get a property the grid gives away free is the wrong order.

I would revisit this only if a single cell grew large enough that sub-cell residency mattered — see
the cell-budget arithmetic below, which says it does not.

### The arithmetic

Today, and every figure measured rather than assumed:

| | value |
|---|---|
| region root | 512 m (`root_size_log2 = 9`) |
| finest voxel | 7.8 mm (`voxel_size_log2 = -7`) |
| **levels, V** | `9 − (−7)` = **16** |
| `max_brick_level` | `16 − 3` = 13 |
| `kMaxVoxelBits` / `kMaxLevels` | 24 / 22 |
| measured tree | 710 K–1.13 M bricks, **431–688 MB**, build **1.89–3.60 s** |
| brick | 8³ = 512 voxels in **144 words = 576 bytes** (16 mask + 128 material) |

Proposed cell: **32 m** (`root_size_log2 = 5`), same 7.8 mm voxel.

| | value |
|---|---|
| **levels per cell, V** | `5 − (−7)` = **12** |
| `max_brick_level` per cell | 9 |
| V against `kMaxVoxelBits = 24` | 12 ≤ 24 — **half the budget, comfortable** |
| stack depth needed | 12 + 1 = 13 against today's `kMaxLevels = 22` |
| cells across a 512 m region | 16 × 16 horizontally |
| occupied vertical span | terrain [−64, 64] m + ~15 m trees ⇒ **5 layers** of 32 m, not 16 |
| **surface cells** | ~16 × 16 = **256** carry the surface; the rest are empty or solid |

**The rebuild-cost claim, derived rather than quoted.** Terrain is a *surface*, so cost scales with
the area a rebuild covers, not the volume: one 32 m cell is `(32/512)² = 1/256` of the region's
footprint. The brief's "~256×" is exactly this, and it checks out. Against the measured 1.89–3.60 s
whole-region build, **a single surface cell should cost ~7–14 ms** — which is the number that makes
goal 250's unbounded adopt lag go away, because a rebuild that finishes in 14 ms cannot be outrun.

**The dependent-load chain.** 16 levels → 12 is a **25% shorter** worst-case chain of dependent,
cache-missing loads per primary ray. Research §3.4's claim (Aokana: *"deep data structures connected
by pointers are not cache-friendly"*, fixed with *"multiple shallow SVDAGs"*, 2–4× above 32 K) is
what this tests on this hardware; goal 256 reports the measured march-ms delta, and I will not
pre-credit it — §9.3's lesson from this same prompt is that a cited speedup can be architecture-bound
and stale.

**The stack.** `kMaxLevels = 22` sizes a fixed `uint stack[22]` in the shader. A 12-level cell needs
13. That is **9 fewer registers per lane** on the hot traversal, which on a register-limited kernel
is free occupancy — goal 256 measures whether it shows up.

### What a cell is, and how a ray crosses one

- A cell is exactly today's `BrickTree` with `root_size_log2 = 5`, plus its integer grid coordinate.
  **No encoding change**: the same header word (bits 0–7 child mask, 8–9 kind, 16–23 material), the
  same layout-v2 attribute word, the same `node_child_slot(header, octant) = 2 + popcount(...)`.
  This is the single most important property of the design — it means the oracle keeps its meaning
  and `tree_layout.hpp` is untouched.
- The grid is a flat array of cell records: `{ ivec3 coord, uint nodeOffset, uint brickOffset,
  uint rootWord, uint flags }`. Node and brick storage stay two big arrays; a cell owns a contiguous
  span of each, and its internal offsets are **relative to `nodeOffset`** — which is the one change
  to addressing, and it is an addition rather than a re-encoding.
- A ray marches the **grid** with a 3D DDA (Amanatides & Woo, which is what Teardown does per
  §9.4), and inside each cell it enters, runs today's octree traversal unchanged. Crossing a cell
  boundary is a DDA step, not a pointer dereference.
- An **absent** cell is a flag, not a null pointer, and that is what AK-D's "never stall a ray" hook
  attaches to: a ray entering an absent cell shades from the coarsest resident level and files a
  request, exactly as GigaVoxels does (§1.4).

### Why 32 m rather than 16 or 64

| cell edge | levels V | cells over 512 m | surface cells | est. rebuild (from 1.89–3.60 s ÷ area ratio) |
|---|---|---|---|---|
| 16 m | 11 | 32 × 32 | ~1024 | ~2–4 ms |
| **32 m** | **12** | **16 × 16** | **~256** | **~7–14 ms** |
| 64 m | 13 | 8 × 8 | ~64 | ~30–56 ms |

64 m cells leave a 30–56 ms rebuild — still a visible hitch if it lands on one frame, and only a 3
level saving. 16 m cells quadruple the cell count and the per-cell fixed overhead (a grid record, a
root node, a partly-filled brick span) for a rebuild already well under a frame. **32 m is the
largest cell whose rebuild fits inside a 150 fps frame budget with room to spare**, which is the
property that matters.

### What this does NOT decide

Residency and eviction (AK-D) — the grid gives a natural unit for both, but the cache, the LRU and
the ray-guided request path are that group's work. And **the payload is unchanged here**: goal 258's
palette and dedup levers apply per brick and are orthogonal to where the brick lives.

### The design's own arithmetic was wrong, and measuring it before writing code is what caught it

The table above predicted **~7–14 ms** for a 32 m cell, by scaling the measured whole-region build
by the surface-area ratio. Before writing any of it I measured a real build at each region size
(`voxel_app --region-log2 N`, one build, RelWithDebInfo):

| region | bricks | MB | build | sampler |
|---|---|---|---|---|
| 512 m (shipping) | 710 K–1.13 M | 431–688 | 1.89–3.60 s | 0.15–0.19 s |
| 64 m | 22,870 | 13.8 | **0.08 s** | **0.10 s** |
| **32 m** | **471** | **0.3** | **0.09 s** | **0.09 s** |

**A 32 m cell costs ~90 ms, not 7–14 ms, and almost none of it is the cell's content.** 471 bricks
and 0.3 MB cannot take 90 ms. What does is the **fixed per-build cost**: `set_focus` builds a 1/16 m
height field over a 16 m radius *plus* a 1/8 m field over 64 m on every build, and neither shrinks
with the cell. At 512 m that fixed cost is 5–9% of the build and invisible; at 32 m it **is** the
build.

Two consequences, and the second reverses a decision I made an hour ago:

1. **Per-cell rebuild is not viable until the fixed per-build cost is removed.** A 90 ms cell
   rebuild is six frames at the target rate — worse than the thing it replaces, if it happens per
   cell. The design needs **one sampler shared across every cell build in a region**, with the focus
   field built once and reused, and the thread pool and classification setup hoisted out of the
   per-cell path.
2. **Goal 251 is reopened by this, and my "decided against" was right for the wrong regime.** I
   closed it measuring the sampler at 5–9% of a 512 m build and wrote *"it becomes worth doing if
   AK-C's per-cell rebuild makes builds frequent and small, which is the opposite regime."* That is
   now the actual regime, and the measurement says focus-field reuse goes from a marginal 8% saving
   to **the precondition for the whole group**. The earlier verdict stands as written for the world
   it was written about; it does not survive the cell grid.

**This is why goal 254 says "on paper, in the log, before writing code".** The paper arithmetic was
defensible and wrong, because it modelled the part of the cost that scales and ignored the part that
does not. Twenty minutes of measurement turned a 256× win into a 256× win *conditional on a
prerequisite the design had not identified*.

### Revised ordering for AK-C

1. **Hoist the fixed per-build cost** (goal 251's focus-field reuse, now a prerequisite rather than
   an optimisation): one `TerrainSampler` per region, reused across cell builds.
2. Then the cell grid (255/256/257), whose marginal per-cell cost is only then the ~0 ms of tree
   work the 32 m measurement shows.

Without step 1, step 2 makes the frame time worse, and the measurement above is the evidence.

---

## 6. The build profile (goal 259) — LTO is pay-to-win that does not win here

`release-codegen-and-tradeoffs.md` §1's four buckets, applied. MSVC has no `-O3`, so the `-O2` vs
`-O3` question translates to `/O2` (what Release already does) versus `/O2` plus `/GL` + `/LTCG`
(CMake's `INTERPROCEDURAL_OPTIMIZATION`), which is the LTO lever the skill's rules 17 and 18 govern.

`stress_pose`, vsync off, vk, two runs each:

| configuration | frame median (ms) | GPU march (ms) | build time |
|---|---|---|---|
| RelWithDebInfo | 5.19 / 5.15 | 4.93 / 4.87 | — |
| **Release `/O2`** | **5.26 / 5.19** | **4.98 / 4.91** | **3 m 22 s** |
| Release `/O2 /GL /LTCG` | 5.28 / 5.26 | 5.00 / 4.98 | **8 m 34 s** |

**LTCG is not faster. It is a hair slower — inside the run-to-run spread either way — and it costs
2.5× the build time.** The LTO build also had Tracy compiled out, which should have helped it, and
it still did not move.

The reason is goal 244's verdict, and this is the same finding arriving from a second direction:
**the frame is GPU-bound on the march.** The CPU phases sum to ~0.4 ms of a 5.2 ms frame — 7.7%.
LTO optimises exactly that 7.7%, so even a spectacular 20% win on it would be 0.08 ms, or 1.5% of
the frame, against a measured run-to-run spread of ~0.1 ms. **There is nothing here for it to win.**

**Bucket: `pay-to-win` in general, `pay-to-nothing` on this workload.** Which is the point of
classifying rather than assuming — the skill's rule 17 says LTO belongs to the shipping build and
not the dev loop, and that is still true in general; it just has no subject here.

**PGO: not worth it, and the arithmetic is the same one.** PGO improves branch layout and inlining
decisions in *CPU* code, which is the 7.7%. A generous 20% improvement on that is **0.08 ms, or
1.5% of the frame, below the ±0.1 ms run-to-run spread** — it could not be measured on this
workload even if it worked perfectly. It becomes worth revisiting only if the CPU share grows, which
is what AK-C's per-cell rebuild would do (many small builds instead of one large one on a background
thread).

**`CMakePresets.json` is unchanged.** The chosen shipping configuration stays `windows-release`
(`/O2`, no LTCG), and the reason is now measured rather than inherited. Adding an LTO preset would
advertise a 2.5× build-time cost for a benefit this project has demonstrated it does not receive.

---

## 7. The payload: palette compression has a 2.7× headroom, measured (goal 258 / goal 157)

A brick is **576 bytes for 512 voxels** — 16 mask words (64 B) plus **128 material words (512 B),
one byte per voxel**, with no palette and no deduplication. Goal 157 has been open since the pivot
on the theory that "most bricks contain two or three materials". That is a theory, so I measured it
before implementing anything (`tools/palette_probe`, a real 64 m build at the shipping voxel size,
seed 1337, trees on):

| distinct materials in a brick | bricks | share | cumulative |
|---|---|---|---|
| **2** | 18,398 | **55.2%** | 55.2% |
| **3** | 14,456 | **43.4%** | **98.6%** |
| 4 | 466 | 1.4% | 100.0% |
| 5 | 14 | 0.04% | 100.0% |
| ≥6 | **0** | — | — |

**The theory is right and stronger than stated: 98.6% of bricks hold three materials or fewer, and
nothing in a real build exceeds five.** (Note there is no 1-material row — a uniform brick is
already collapsed to a solid leaf by the builder, so every brick that exists holds a boundary.)

### What that buys, arithmetically

| encoding | material bytes per brick | brick total | ratio |
|---|---|---|---|
| today: 1 byte/voxel | 512 | **576 B** | 1.00× |
| **2-bit index + 4-entry palette** | 128 + 4 | **196 B** | **2.94×** |
| 3-bit index + 8-entry palette | 192 + 8 | 264 B | 2.18× |

**The right design is the 2-bit one with a fallback**: a 4-entry palette covers 98.6% of bricks, and
the 1.4% that need more keep today's byte-per-voxel encoding behind a kind bit. Chasing the last
1.4% with a 3-bit index costs 35% more memory on the other 98.6%, which is a bad trade by a wide
margin.

Applied to the measured shipping tree — 710 K bricks × 576 B = **409 MB of the 431 MB total is the
brick array** — a 2.94× cut on 98.6% of it takes the tree to roughly **161 MB**, i.e. **431 → 161 MB,
2.7× overall**. That is the single largest memory lever identified in this prompt, it is independent
of the cell grid, and it directly reduces the upload traffic that AK-B could only make less frequent.

### The cost, and what must be measured before it ships

Decoding a 2-bit index plus a palette fetch is **one extra dependent load per voxel hit** in the
marcher, against a saving of ~2/3 of the brick bandwidth. Goal 258's own instruction is the right
one and I have not yet paid it: *"if a lever costs more tracing time than it saves in bandwidth, say
so with both numbers and leave it off."* The march is currently **4.9 ms and GPU-bound** (§1), so
this is not free money — the measurement to make is march ms before and after at the same pose, both
backends, with the oracle at 0/7,000.

**Status: the headroom is measured and the encoding is chosen; the implementation and its
before/after march number are not done.** Recorded as the specific next step rather than claimed.

### DAG deduplication (goal 163): not attempted, and the reason is ordering

SVDAG's published figures are large (§3.1: *"19 billion voxels in 945 MB"*). But dedup interns
identical subtrees, and its payoff depends on how much of the tree is identical — which the palette
change alters, because two bricks differing only in palette order are not bit-identical today but
would be after canonicalisation. **Measuring dedup before the payload is settled would measure the
wrong tree.** Palette first, then dedup on the result.

### And the apron, decided against as instructed

Research §1.3 is unambiguous and this prompt restates it: a one-voxel apron on an 8³ brick is a
**1.95× memory blow-up** — it would take the post-palette 161 MB back to ~314 MB, undoing the entire
lever above and then some. **No apron.** If Prompt 005 wants filtered brick sampling, the
corner-centred 3³ trick or explicit neighbour fetches are the published alternatives.

---

## 8. The prerequisite, built — and the cache inside it, measured at 0% and removed (goal 251)

§5's measurement said per-cell rebuild is blocked until the fixed per-build cost is hoisted. That
cost is `set_focus`: a 1/16 m height field over the focus radius plus a 1/8 m field over four times
it — **~1.3 M noise samples**, and the count does not depend on the region being built. At 512 m it
is 5–9% of a build; at 32 m it *is* the build.

**What shipped**: the tiers are now shareable. `TerrainSampler` gained `FocusTiers`
(`vector<shared_ptr<const HeightField>>`), a `FocusKey` describing the snapped rectangle a tier
covers, and three functions — `focus_keys()`, `make_focus_tier()`, `adopt_focus()` — so a caller can
build the tiers **once** and hand the same objects to many samplers. That is precisely what the cell
grid needs: one set of tiers per rebuild, adopted by every cell in it, instead of 1.3 M samples per
cell.

Two tests pin it: an adopted tier answers identically to a sampled one over 8,192 probes, and the
keys snap identically for centres inside one coarse cell while differing for centres a trigger
distance apart.

### The cache I wrote first, and why it is gone

The obvious next step looked like a cross-**build** cache in `SvoWorld` — remember the last few
rectangles, reuse on a hit. I implemented it, instrumented it, and measured:

```
focus tiers: 0 reused, 4 built
```

**A 0% hit rate, at both 32 m and 64 m regions, over a 900-frame flight.** The reason is arithmetic,
not tuning: the keys are snapped to the 0.5 m coarse cell, consecutive builds are ≥8 m apart *by
construction* (goal 249's trigger), and a stationary camera does not rebuild at all (the hysteresis
and speed gate see to that). **A moving camera never revisits a rectangle and a still one never
asks.** There is no camera path on which that cache can fire.

It was **removed rather than left in**. Machinery that provably cannot fire is worse than no
machinery, because it reads as coverage — which is this arc's most repeated failure mode, and the
first time I have caught it in code I had just written rather than in an instrument I inherited.

**The sharing primitive stays; the cache does not.** The distinction matters: sharing tiers *within*
one rebuild across many cells is the mechanism that pays, and it is what §5's revised ordering
called for. Caching them *across* rebuilds was a different idea that happens to be worthless here.

---

## 9. What the marcher actually does, and a premise that did not survive (goals 245, 269)

### The step distribution, measured on the CPU reference

`tools/svo_render` now histograms primary-ray steps and reports percentiles, because a mean hides
exactly the tail these constants are supposed to bound. Three poses, 640×360, shipping geometry:

| pose | mean | p50 | p95 | p99 | **max** |
|---|---|---|---|---|---|
| ground | 20.8 | 16 | 46 | 69 | **119** |
| panoramic | 14.8 | 14 | 26 | 35 | **63** |
| macro | 20.6 | 13 | 51 | 80 | **173** |

**`kMaxIterations = 2048` against a measured worst case of 173** — a **12× margin**, and **26×**
against the p99. The ceiling was chosen as a safety value and never checked; it is 12–26× the real
requirement.

Against the literature (research §2.1, §3.1): this engine traverses at **14–21 steps per primary
ray**, which is what a well-formed octree DDA should cost at 16 levels. The marcher is not lost in
the tree — goal 245's comparison says the traversal is *structurally* fine, and §2's finding stands:
the cost is the *number* of traversals (six per shaded pixel), not the cost of each.

### Sizing the constants from that data made it slower, on one backend only

Goal 269's premise is *"with a shallow-grid tree both shrink... register pressure and occupancy are
the mechanism"*. I sized both from the data and measured, isolating each:

| `kMaxIterations` | `kMaxLevels` (stack) | vk march ms | d3d12 march ms |
|---|---|---|---|
| **2048** | **22** (shipping) | **4.97 / 4.96** | **5.10** |
| 512 | 22 | 4.95 / 4.96 | — |
| 256 | 22 | 4.96 / 4.96 | — |
| 2048 | **14** | **5.74 / 5.76** | **5.04** |
| 512 | 14 | 5.68 / 5.75 | — |

Two findings, and the second is the one worth having:

1. **`kMaxIterations` costs nothing at any value.** 2048, 512 and 256 are identical to three
   significant figures. That is unsurprising in hindsight — it is a loop bound that rays exit long
   before — but it is now measured rather than assumed, and it means the 12× margin is free. **Left
   at 2048.**
2. **Shrinking the stack from 22 to 14 costs 16% on Vulkan and nothing on D3D12.** 4.97 → 5.75 ms on
   vk; 5.10 → 5.04 on d3d12, which is inside the spread. Consistent across repeated runs on both.

**A 16% regression on one backend and a wash on the other is a shader-compiler artefact, not a
hardware property** — the two paths compile the same HLSL through different toolchains, and only one
of them dislikes the smaller array. Whatever the mechanism, the measured fact stands: **on the
backend this project develops against, a smaller traversal stack is a penalty, not free register
relief.**

### This corrects §5's design

§5 listed *"9 fewer stack registers"* among the cell grid's benefits, on goal 269's premise. **That
benefit is measured as a 16% penalty on Vulkan.** The cell grid's case rests on the other two
columns — a 25% shorter dependent-load chain and a ~256× cheaper rebuild — and the stack line should
be struck from it rather than quietly kept. If the grid ships, `kMaxLevels` should stay at 22 unless
a measurement on *that* structure says otherwise.

This is the second time in this prompt that a cited mechanism failed to reproduce here (after §9.3's
retracted persistent-threads result), and the pattern is worth naming: **published GPU optimisation
mechanisms are architecture- and toolchain-bound, and this repo's rule of measuring both backends is
what keeps catching it.** Neither would have been visible on vk alone.

---

## 10. Cheapening the secondary rays — the largest win in this prompt (goal 268)

§2 measured secondary rays at **49% of the marcher on vk**, with AO alone at 1.81 ms (37%) for its
four traversals. That is where the frame time is, so that is what to cut.

Two levers, measured separately and then together, on `stress_pose`:

| configuration | vk march | d3d12 march |
|---|---|---|
| shipping: 4 AO rays, unbounded ray length | 4.89–4.94 | 5.10–5.12 |
| **2 AO rays** | 4.27–4.31 (**−13%**) | 4.64 (−9%) |
| **clamped AO ray length (≤2 m)** | 4.46–4.47 (**−9%**) | 4.86 (−5%) |
| **both** | **4.00–4.05 (−18%)** | **4.42 (−13%)** |

**Both shipped.** The whole march is 18% cheaper on vk and 13% on d3d12.

### Why each works

- **The ray length was unbounded.** `rayLength = max(0.15, aoRadiusPx * hitDistance * pixelAngle)`
  grows linearly with hit distance, so **distant pixels paid the most for the AO that mattered
  least** — a contact-shadow term whose feature size out there is sub-pixel. Clamping at 2 m (a
  couple of the terrain cubes this look is built from) costs nothing visible.
- **Four hemisphere samples were two more than needed.** Two opposed azimuths plus the existing
  per-pixel `rot` jitter, with TAA integrating across frames, is what the four quadrants were really
  buying.

### The image, viewed

`research/captures/ak_ao_cheaper.png` — `macro_ground`, the close-up look shot, side by side.
**Mean difference 0.097/255 over 0.11% of pixels, max 10.** Visually indistinguishable, and that is
measured **with `--no-taa`**, the harsher case: the shipping path has TAA to further hide the halved
sampling. The change is also **below the 1.5% golden gate**, so every scenario golden still passes
unmodified — which is a useful independent confirmation that the image did not move.

Oracle **0/7,000** (4,000 + 3,000 rays, 0 mismatches). The traversal itself is untouched — AO is a
shading consumer of it — but the rule is satisfied and checked rather than assumed.

### The CPU reference changed with the shader

Prompt 004 rule 2, and it matters here for a specific reason: `tools/svo_render` is what
`--lod-center` reproductions and the GPU-vs-CPU diffs are judged against, so an AO change in one and
not the other would silently invalidate every future comparison. Both levers are mirrored in
`tools/svo_render/src/main.cpp` with the same constants.

### Two vacuous measurements caught on the way, both mine

1. **The first "2 rays" run reported no change** (4.89 → 4.94, i.e. nothing) — because my patch
   script matched `\n` against a file with **CRLF** line endings, so it replaced nothing and I
   measured the baseline twice. Caught by disbelieving a result that contradicted §2's 1.81 ms.
2. **The `[unroll]`/loop-bound sweep in §9** had the same shape and was caught the same way.

Both are the same root cause as §3's `fly_transect.scn` incident: **a patch that silently matches
nothing produces a confident measurement of the unchanged system.** The habit that catches it is
cheap — grep the file for the change before trusting the number — and it is now three for three.

### Not attempted, and why

- **AO from the node's `coverage` attribute** instead of tracing (goal 268's option 2). The
  attribute is already stored and already read for the LOD early-out, so it is nearly free — but it
  describes *volume* coverage of a node, not directional occlusion at a point, so it answers a
  different question. It would need a term relating the two, which is a look decision rather than a
  performance one; **handed to Prompt 005** with this note.
- **Reusing one traversal's stack for the next ray.** The two-ray AO loop no longer has enough rays
  for the bookkeeping to pay, and the shadow ray starts from a different origin and direction.

---

## 11. `SV_Depth` was costing 17% of the march, and nothing was reading it (goal 267)

Goal 267 asks for the march to be ported to compute, on the strength of two claims. §9.3 already
retired one of them (persistent threads, retracted by its own authors). §9.1 kept the other, and it
is specific: **a pixel shader's ROP exports retire in submission order**, so a shader with a cheap
fast path and an expensive slow path stalls its finished pixels behind unfinished ones — Aaltonen
measured 1.5–3.4× moving exactly that shape to compute, and NVIDIA's own guidance is *"consider
converting your full screen pass to a compute shader if there's a large difference in latency between
warps."* An SVO marcher over sparse terrain is precisely that shape.

**Before building the port, I measured the mechanism.** A shader that writes `SV_Depth` forces the
ROP to order its exports; one that does not, does not. So: comment out the depth write and see.

| | vk march | d3d12 march |
|---|---|---|
| writing `SV_Depth` (shipping) | 4.03 / 4.00 | 4.39 |
| **not writing it** | **3.35 / 3.37** | **3.95** |

**17% of the march on vk, 10% on d3d12** — the ROP-ordering mechanism, measured on this hardware,
without moving a line of traversal code.

### And nothing was reading it

The shader's own comment said the write existed *"so the existing post chain / overlay see a real
depth"*, and the PSO comment said *"what matters is that the WRITE lands, for the overlay."* Both
are wrong, and checking took one grep: on the svo path **every pass after the march has
`DepthEnable = False`** — the TAA resolve, the composite, and ImGui. The TAA is *distance*-reprojected
and reads the R32_FLOAT `Dist` target, not depth. Nothing sampled the depth buffer at all.

Verified rather than argued: a frame captured **with the overlay and crosshair on**, both
configurations (`research/captures/ak_no_sv_depth.png`). Identical but for the overlay's own changing
digits — and the overlay's own `gpu march+resolve` readout moved **2.28 → 1.95 ms** in the shot,
independently agreeing with the harness. All 273 tests pass including every golden, so the image is
unchanged within the 1.5% gate.

The DSV stays **bound** (the format is still declared) because ImGui's PSO is created against it and
the attachment must exist; only the write and the depth state are gone.

### What this means for the compute port

**The port's main justification is now already banked.** §9.1's argument was that compute escapes
ordered ROP export; this pass escapes it *while remaining a pixel shader*, by not writing the thing
that forced the ordering. What compute would add on top is thread-group-ID swizzling (§9.2, up to
47% — but only under three preconditions this frame has not been shown to meet, since it is not
VRAM-latency-bound), against the cost of re-plumbing colour + distance to UAVs and losing `SV_Depth`
entirely — which is now free anyway.

**Recommendation, recorded rather than acted on: do not port to compute yet.** Measure Bavoil's
three preconditions first (VRAM as top-throughput unit, L2 hit rate under 80%, overlapping
footprints between adjacent groups) — that is goal 271's counter access. If they do not hold, the
port buys nothing this pass has not already taken.

### The running total on `stress_pose`

| | vk march | d3d12 march |
|---|---|---|
| start of this prompt | 4.90 | 5.12 |
| after goal 268 (AO) | 4.02 | 4.42 |
| **after goal 267 (no `SV_Depth`)** | **3.33** | **3.93** |
| **total** | **−32%** | **−23%** |

---

## 12. The counters, the answer to both open measurement questions, and a decisive negative for 267 (goal 271)

### §8.4's question: does this NVIDIA driver expose `VK_KHR_performance_query`? **No.**

The research section could not answer this because the database page truncated before the NVIDIA
entries and every implementation hit was Mesa. `vulkaninfo` on this machine answers it outright:

| device | driver | device extensions | `VK_KHR_performance_query` |
|---|---|---|---|
| **NVIDIA GeForce RTX 4070 Laptop GPU** | NVIDIA 610.47 | 278 | **absent** |
| Intel(R) UHD Graphics | Intel 101.6790 | 148 | present (plus `VK_INTEL_performance_query`) |

So the Mesa-only pattern the research noticed is real, and it extends to NVIDIA's Windows driver:
**the GPU this engine renders on does not implement the extension.** The iGPU that does is not the
one being measured. Recorded so nobody spends another pass looking for it.

### §8.5's question: the first-timestamp fault. Still no primary source; recorded as folklore.

No primary source was found for the `vkCmdWriteTimestamp`-as-first-command fault. The workaround —
`gpu march+resolve` skipping the first two frames — stands **because it was reproduced here**, not
because it is documented. Written down explicitly, in the code and here, so a later reader does not
"clean up" a two-frame skip that looks arbitrary and get a driver fault back.

### The counter decision: Nsight Perf SDK is out on this machine, and here is exactly why

Prompt 002 goal 222 left the counter path open. Resolving it:

- **Nsight Perf SDK / Nsight Graphics: not installed.** `C:\Program Files\NVIDIA Corporation` holds
  only `FrameViewSDK`, `NvContainer`, `NVIDIA App`, `NvTelemetry`, `Installer2` — no Nsight, and no
  CUDA toolkit.
- **And it would need an admin change even after installing.** The counter permission lives at
  `HKLM\SYSTEM\CurrentControlSet\Services\nvlddmkm\Global\NVTweak\RmProfilingAdminOnly`; on this
  machine the key exists and the value is **unset**, so the driver default (admin-only) applies —
  the `ERR_NVGPUCTRPERM` case the research names.

**Decision: no counter SDK this pass.** It is an install plus a privileged registry write on the
owner's daily-driver machine, for data that turns out — see below — not to change any decision in
this prompt. Recorded as a goal rather than done.

### What IS available on this GPU, checked and written down so the next pass does not re-derive it

`vulkaninfo`'s NVIDIA device list does contain three extensions that reach some of the same data
without Nsight and without admin:

| extension | what it would give | cost to use here |
|---|---|---|
| `VK_KHR_pipeline_executable_properties` | **registers per thread**, spill counts, per-stage stats straight from the driver — goal 269's exact question | the pipeline must be created with `VK_PIPELINE_CREATE_CAPTURE_STATISTICS_BIT_KHR`, which Diligent does not set and does not expose a hook for; needs a standalone Vulkan probe |
| `VK_KHR_shader_clock` | per-lane cycle counts inside the real shader — direct latency variance | DXC's `vk::ReadClock` is SPIR-V only, so vk-path only, and Diligent must be made to enable the device extension |
| `VK_NV_shader_sm_builtins` | `gl_SMIDNV` / `gl_WarpIDNV` — which SM and warp slot each pixel ran on, i.e. real occupancy | same DXC/Diligent plumbing |

Also present and relevant to goal 275: **`VK_KHR_ray_query` and `VK_KHR_ray_tracing_pipeline`** — the
hardware RT path this pass declines is available on this device, not merely hypothetical.

### The measurement that actually mattered, taken a cheaper way

Goals 267 and 269 both rest on one claim: that this marcher's warps finish at very different times,
so redistributing their work would win a lot. The counters would have measured that. **So does the
CPU reference, for free, deterministically, and in CI** — because a warp runs until its *slowest*
lane finishes, so it costs `lanes × max(steps)` issue slots while doing only `sum(steps)` useful
work. Over a frame:

> **efficiency = Σ over warps of sum(steps) ⁄ Σ over warps of (lanes × max(steps))**

and **1 − efficiency is the theoretical ceiling on any work-redistribution scheme** — persistent
threads, ray reordering, SER — whatever its published figure on other content.

`tools/svo_render` now keeps a per-pixel total-step map and prints this. The metric itself lives in
`world/svo/warp_divergence.hpp` with **7 test cases / 817 assertions**, written *before* its number
was quoted, because this pass has already found six instruments that reported a number while
measuring nothing. The cases are chosen so a plausible bug in each direction fails one: taking the
peak per pixel instead of per warp (reports a perfect 1.0), forgetting a clipped tile has fewer
lanes, striding only in x, or returning 0 rather than 1 for an empty frame.

**Six poses, 1280×720, the shipping tree:**

| pose | primary steps p50/p99/max | 8×4 | 4×8 | 16×2 | 2×2 quad |
|---|---|---|---|---|---|
| `stress_pose` (hilltop, ground) | 26 / 76 / 130 | **93.4%** | 92.9% | 92.4% | 97.6% |
| `valley_far` | 27 / 76 / 130 | **93.3%** | 93.0% | 92.4% | 97.5% |
| `macro_ground` (99.5% terrain) | 27 / 84 / 175 | **92.7%** | 92.2% | 92.0% | 97.0% |
| `macro_tree` | 22 / 75 / 122 | **94.1%** | 93.4% | 93.5% | 97.8% |
| `fly_transect` (32.3% shadowed) | 18 / 96 / 173 | **93.8%** | 93.9% | 92.4% | 97.7% |
| grazing (−1° pitch) | 24 / 75 / 129 | **93.4%** | 93.0% | 92.4% | 97.6% |

**Warp efficiency is 92.0–94.1% on every pose and every tiling.** The exact pixel-to-warp mapping on
NVIDIA is undocumented, which is why four tilings are printed — the conclusion does not depend on
guessing it right.

### So: work redistribution has at most 6–8% of headroom on this content

Aila & Laine's 1.92× came from **incoherent** rays over sharply varying geometry. This engine's rays
are the opposite: a pinhole camera over a height field, plus shadow rays that all point at the same
sun. Adjacent pixels traverse nearly the same nodes, so lanes finish together.

**The instrument was falsified before it was trusted** — it moves in the predicted direction when
ray coherence changes, and the sizes make physical sense:

| workload at `stress_pose` | secondary steps/px | 8×4 efficiency |
|---|---|---|
| primary rays only | 0.0 | **94.0%** |
| + shadow rays (all one sun direction — *coherent*) | 7.5 | **94.2%** |
| + 4 AO rays (per-pixel random rotation — *incoherent*) | 12.8 | **93.4%** |

Coherent shadow rays add **no** divergence; incoherent AO rays cost **0.8 points**. An instrument
that reported a constant, or moved the wrong way, would have failed here.

**This closes goal 267 as a measured negative, and it agrees with the mechanism result in §11.** Two
independent measurements, of two different things, telling one consistent story:

- **Step divergence is ~7%** — the lanes do nearly the same amount of *work*.
- **Removing ordered ROP export won 17%** (§11) — larger than 7%, so the remainder is **memory
  latency variance, not work imbalance**. And that part has already been captured, by a change that
  kept the pixel shader.

A compute port's remaining prize is thread-group swizzling, whose 47% figure requires being
VRAM-latency-bound; the persistent-thread half of the argument is now measured at a **6–8% ceiling
on this content**, on top of being retracted by its own authors (§9.3). **Recommendation stands and
is now evidence-backed: keep the pixel shader.**

*Caveat, stated plainly:* this measures divergence in traversal **steps**, not in time. Two lanes
taking equal steps still finish apart if one misses cache. The number is therefore a **lower bound**
on total latency variance — which is exactly why §11's 17% is the complementary measurement and why
both are reported together rather than either alone.

---

## 13. The beam pre-pass: 40–53% of primary traversal steps, and the CPU reference proves it (goal 266)

### First: `tOffset` is not the hook the prompt hoped it was

The prompt suggests `TraceRay`'s existing `tOffset` may be "exactly the hook you need". It is not.
`tOffset` is goal 164's **LOD distance bias** — added to `t` before the *LOD test only*, so a
secondary ray can judge detail from a different origin. It never moves where the ray starts. A real
start-`t` needed new machinery: `TraceParams::t_start` on the CPU, and with it a subtlety worth
naming, because it is the kind of thing that produces a wrong image three passes later — a seeded
ray no longer starts **on** a root face, so the entry-axis snap that fixes the first cell's
coordinate has to be skipped and the cell derived from the seeded position alone. `lastAxis` goes
to −1 for the same reason, which is exactly how a ray starting *inside* the root was already
handled.

### Measure the ceiling before building the thing (`--beam-tile`)

Research §4.7 lists four shipped or published systems doing this under four names — ESVO's beam
optimization, Teardown's per-object linear-depth early-out, Aokana's Hi-Z, GigaVoxels' Early-Z
proxy — and the prompt is explicit that **ESVO's own beam resolution and speedup could not be
confirmed from primary sources**, with two contradictory figures circulating. So no borrowed number.

The **oracle beam** measures the ceiling: seed each ray with the tile minimum of the *true* per-pixel
hit distances. No conservative bound can be looser than that, so whatever it saves is an upper limit.

| tile | primary steps/px | saved |
|---|---|---|
| baseline | 27.4 | — |
| 4×4 | 10.1 | **63.0%** |
| 8×8 | 10.7 | **60.9%** |
| 16×16 | 11.8 | **57.1%** |
| 32×32 | 13.0 | **52.5%** |

Two things are notable. The ceiling is **large**, and it is **barely sensitive to tile size** — a
32×32 pre-pass, at 1/1024 the pixels, still reaches 52.5%. That is what made this worth building.

### Then build the real one, and prove it conservative

`world/svo/beam.hpp` over-approximates a tile's frustum as a **cone** and descends the octree,
expanding each node's AABB by the cone's radius at that node's farthest reach before slab-testing
the centre ray. The proof is in the header: for any ray R in the cone and any point P of geometry in
node N, t_R ≤ tFar because P is in N's box, and the centre ray at t_R is within t_R·tan θ of P — so
the centre ray at t_R lies inside the expanded box, hence tNear ≤ t_R. Children are visited
nearest-first and pruned against the running best, which is what keeps it cheap.

**Tests first, again: 8 cases / 8,189 assertions**, and the property under test is the only one that
matters — *for every ray inside the cone, `beam_start_t` ≤ `trace_ray(ray).t`* — asserted over
thousands of real rays through a real tree at four cone widths and four view directions including a
grazing one, plus monotonicity in cone width (the property a pruning bug breaks), an apex inside
geometry, `max_t`, an empty tree, and an end-to-end check that seeding a trace changes neither the
hit, its distance, its material, nor its normal.

### The result, six poses, 16×16 tiles

| pose | steps/px | oracle (ceiling) | **conservative (real)** | pixels changed |
|---|---|---|---|---|
| `stress_pose` | 27.4 | 57.1% | **44.5%** | **0** |
| `valley_far` | 28.1 | 58.5% | **45.7%** | **0** |
| `macro_ground` | 28.8 | 79.4% | **53.2%** | **0** |
| `macro_tree` | 25.1 | 45.1% | **35.1%** | **0** |
| `fly_transect` | 23.8 | 58.5% | **44.6%** | **0** |
| grazing (−1°) | 25.8 | 53.9% | **42.1%** | **0** |
| skyward (+25°) | 16.3 | 19.9% | **20.9%** | **0** |

**The correct, provably-conservative bound reaches about 80% of the oracle ceiling, and changes zero
pixels at every pose.** The bound's own cost at 1280×720, on 16 CPU threads: 24.1 ms for 14,400
tiles at 8×8, **5.9 ms for 3,600 at 16×16**, 2.5 ms for 920 at 32×32.

(`skyward` beats its own oracle by one point. The oracle is a bound on the *seed*, not on the *step
count*, and starting later can put a ray on a slightly different cell path; at a pose that is mostly
sky the two are within noise of each other anyway. Recorded rather than smoothed over.)

### What limits the bound — and it is not the code

Two separate hours went into chasing a bound that came back at the root's own entry face before the
cause turned out not to be a bug. **The bound is the entry distance of the first node that contains
geometry, so its slack is that node's own extent.** In the test tree the sphere's near face is
represented by a 16 m brick leaf whose front face is the root boundary: the bound is 20 for geometry
at 34, and that is as tight as that tree allows.

Which states the general rule, now written into the test that found it: **a beam pre-pass buys
exactly as much as the tree is fine along the ray.** This engine's tree is deliberately coarse at
distance, and the measured 35–53% is what that coarseness leaves on the table.

One real robustness fix came out of the same hunt: an axis-aligned ray (`dir[a] == 0`) with the apex
exactly on a node face made `(hi − o) · 1e30` evaluate to `0` and reported the node as entered at
t = 0. Conservative, but it collapses the bound to the root entry for the very common case of a
camera looking straight down an axis. That axis is now handled explicitly — no constraint unless the
apex is outside the slab.

### Status

**CPU reference done, tested, and measured; the shader mirror is the remaining work.** The saving is
32% of *all* traversal steps at `stress_pose` (primary is 27.4 of 40.2), which on a 3.33 ms march
that is dominated by traversal predicts roughly a 1 ms cut — the largest single item still open in
this prompt. 283/283 tests pass.

---

## 14. The beam pre-pass on the GPU: correct, effective, and it still does not pay (goal 266, closed)

The CPU half of §13 said the seed removes 35–53% of primary traversal steps. This is the shader
mirror, the app wiring, and what happened when it ran on the actual GPU.

### It works, and it is correct

`svo_beam.psh.hlsl` renders one pixel per screen tile into an R32_FLOAT target and
`svo_march.psh.hlsl` starts every primary ray at that tile's bound. The one structural difference
from the CPU is forced and harmless: a pixel shader cannot recurse, so the descent walks an explicit
stack and visits children in a fixed order derived from the direction signs instead of sorting them
near-to-far — ordering changes only how fast the running best prunes, never the result, which is a
minimum over admitted nodes.

Two details that are not slop and are written into the code as such:

- **The seed is read with `Load()` at an explicitly computed tile index**, never sampled by UV. A
  filtered or rounded fetch can land on a *neighbouring* tile, whose bound is not conservative for
  this pixel, and the failure mode is a hole in the terrain far from its cause.
- **The cone's margin is one whole pixel, not half.** Half covers a pixel being sampled at its
  centre but covering its footprint; the other half covers TAA's sub-pixel jitter, which moves the
  ray by up to 0.707 px diagonally while the shader still indexes by the *unjittered* pixel.

**Correctness evidence: with the pass on, `stress_pose` passes the shipping golden — captured before
this feature existed — at a mean of 0.099/255 and 0.0093% of pixels changed.** 288/288 tests pass,
including the five GPU scenario tests.

### And the measurement, interleaved so clock drift cannot fake it

`--ramp beam-tile:0,8,0,8,0,8,0,8`, one process, alternating rungs:

| | beam ms | march ms | **beam + march** |
|---|---|---|---|
| **vk**, off | 0.00 | 3.59 / 3.63 / 3.65 / 3.64 → **3.63** | **3.63** |
| **vk**, tile 8 | 0.70 (all four) | 3.02 / 3.02 / 2.88 / 2.88 → **2.95** | **3.65** |
| **d3d12**, off | 0.00 | **4.22** | **4.22** |
| **d3d12**, tile 8 | 1.04 | **3.90** | **4.94** |

**The march really does get 19% faster on vk. The pre-pass really does cost 0.69 ms. They cancel to
within 0.01 ms — and on d3d12 the pre-pass is 0.72 ms of pure loss.**

Across tile sizes (vk, mean steps from the ramp's own counter):

| tile | beam pixels | mean steps | beam ms | march ms | total |
|---|---|---|---|---|---|
| off | — | 91.9 | 0.00 | 3.61 | **3.61** |
| 2 | 230,400 | 61.6 | 2.32 | 3.24 | 5.56 |
| 4 | 57,600 | 63.0 | 1.18 | 3.16 | 4.34 |
| **8** | 14,400 | 65.4 | 0.68 | 3.20 | **4.02** |
| 16 | 3,600 | 68.7 | 0.58 | 3.60 | 4.18 |
| 32 | 920 | 72.5 | 0.58 | 3.63 | 4.21 |
| 64 | 240 | 75.8 | 0.48 | 3.19 | 3.67 |
| 128 | 60 | 80.6 | 0.48 | 3.24 | 3.72 |
| 256 | 15 | 91.9 | 0.01 | 3.62 | 3.63 |

**No tile size wins.** The best total is worse than doing nothing.

### Why, and this is the part worth keeping

**The pre-pass cannot fill the GPU.** Look at the cost column against the pixel count: 14,400 pixels
cost 0.68 ms and 3,600 cost 0.58 ms — a **quarter of the work for 85% of the time**. Sixty pixels
still cost 0.48 ms. That is not a shader doing more or less work; it is two warps chasing a chain of
dependent node loads with no other warps resident to hide the latency. The beam is a small,
serial, pointer-chasing dispatch, which is the exact shape a GPU is worst at — and the same
mechanism §12 measured from the other side.

(The tile-256 row is the control: 15 pixels, cone so wide it terminates at the root, 0.01 ms and
mean steps back at the un-seeded 91.9. The pass costs nothing when it does nothing, so the 0.48 ms
floor is real work, not fixed overhead.)

### The finding that outlives the feature

> **A 29% cut in traversal steps buys 19% of march time.**

That is the first direct measurement of how step-bound this marcher actually is, and it is a
number the rest of this prompt needs. Roughly two thirds of the march is traversal and one third is
everything else — so **any lever that shortens the dependent-load chain should be costed at about
0.65× its step reduction**, including AK-C's shallow-grid tree, whose stated premise is exactly
that ("a shallower tree should be measurably faster on its own, from the shortened dependent-load
chain"). It does not make that idea wrong; it sizes it.

### Status: closed as a measured negative, kept behind a knob

`Settings::beam_tile` defaults to **0** and `--beam-tile N` turns it on. The code stays because the
result is a *this GPU, this tree, this frame* result, not a claim about the technique: a coarser
tree, a different GPU, or the reprojected variant below would each move it, and re-measuring is one
flag.

**The specific follow-up, recorded rather than attempted:** the beam and the march are serialised
only because the march reads the beam's output *this* frame. Seeding from the PREVIOUS frame's beam,
widened by the camera's own travel over one frame (0.24–0.96 m at this project's 40–160 m/s fly
speeds — a bounded, cheap subtraction that keeps the bound conservative), makes the two passes
independent and lets the GPU overlap them. That is what Teardown's and Aokana's Hi-Z variants
effectively do. If the overlap were free the result flips from a 0.01 ms wash to a 0.68 ms win, or
19% of the march. Opened as a goal rather than guessed at.

---

## 15. TAA history length: free to lengthen, and the decision is Prompt 005's (goal 270)

`taa_blend = 0.125` is an eight-frame exponential history. Research §3.5 has NAADF (2026) retaining
**32** frames for exactly this content class and exactly this engine's artefacts (*"flickering,
blurring, ghosting, and aliasing ... especially important in voxel worlds with sharp edges"*). Goal
270 is explicitly a probe, not a commitment.

New: `--taa-blend F` on every executable, and `dev/scenarios/taa_pan.scn` — a deliberately slow
(~7°/s) pan with **TAA left ON**, unlike every golden-comparing scenario, capturing at rest, mid-pan,
one second after the pan stops, and four seconds after.

### The cost question is settled, and the answer is zero

| | resolve ms | history memory |
|---|---|---|
| `--taa-blend 0.125` (8 frames) | **0.37** | 2 × RGBA16F full-res |
| `--taa-blend 0.03125` (32 frames) | **0.37** | 2 × RGBA16F full-res |

**Identical, and necessarily so.** An exponential history is not a ring of N frames — it is the same
two buffers with a different blend weight, so "32 frames" costs exactly what "8 frames" costs. NAADF's
figure comes from a different, deeper scheme (quantised positions and normals of ray bounces); the
cheap version of its idea is one constant, and this engine can have it for nothing.

That disposes of the half of goal 270 that is a performance question. What remains is quality.

### The quality question, measured only where the measurement is valid

| capture (both runs at the same pinned pose) | local contrast @1/8 | @1/32 | mean diff | % changed |
|---|---|---|---|---|
| `rest` (static, before the pan) | 16.64% | 16.62% | 3.02/255 | 9.40% |
| `just_stopped` (1 s after motion) | 9.43% | **7.64%** | 1.49/255 | 9.86% |
| `settled` (4 s after motion) | 2.81% | **1.83%** | 0.68/255 | 2.29% |

Two things follow:

- **At rest the two are indistinguishable** (16.64% vs 16.62%). With the camera still, an eight-frame
  history has already converged — the only per-frame variation left is the jitter pattern — so a
  longer one buys nothing there. That is worth knowing: the case people imagine TAA history helping
  most is the case where this engine gets nothing from it.
- **After motion the longer history is measurably softer** — 19% less local contrast one second after
  the pan stops, 35% less four seconds after. That is the trade appearing exactly where theory says
  it should.

**And the viewed capture** (`research/captures/ake_taa_history_8_vs_32.png`, mid-pan and one second
after, both settings, 2× crop): at 7°/s the difference is **subtle**. The 1/32 pan is slightly
smearier on the ridge banding; neither shows the obvious doubled-edge ghosting a much longer history
would produce on fast motion.

### One vacuous measurement caught, and written into the scenario

The mid-pan pair was going to be quoted at "14.8% of pixels changed". It is not a TAA number.
`capture frame N` fires on a **frame index** while the script's motion is timed in **seconds**, so
two runs at slightly different frame rates — 1644 frames against 1635 — reach frame 380 at different
yaws, and almost all of that 14.8% is the camera having moved. Only the captures taken while the
camera is **held** are comparable across runs. That is now a comment in `taa_pan.scn` so the next
person does not quote it either. (Running total for this pass: **seven** instruments that reported a
number while measuring something else.)

### Handoff to Prompt 005, as goal 270 requires

> **Lengthening this engine's TAA history is free.** Cost: 0.37 ms either way. Memory: identical.
> One flag: `--taa-blend`. There is no performance argument on either side, so the choice is purely
> aesthetic and it is yours.
>
> What the measurements say you would be choosing between: **at rest, nothing changes** — 1/8 has
> already converged. **After motion, 1/32 is softer** — 19% less local contrast one second on, 35%
> four seconds on. Whether that reads as welcome anti-aliasing on this engine's sharp voxel edges or
> as mush is the question §5 of your own brief is better placed to answer, and
> `voxel_harness --scenario taa_pan --ramp taa-blend:0.125,0.03125` puts both in front of you in one
> command.
>
> One caution: the softening measured here is at **7°/s**. Prompt 005's grain and normal work will
> change what there is to smear, and fast motion was not tested. Re-measure at the pan rate the look
> is actually judged at.

---

## 16. The frame-time regression gate, and the two things that had to be measured to make it real (goal 273)

Research §6.8's prescription: assert on a **percentile across a fixed camera path from GPU
timestamps**, not a mean and not fps — this machine's 165 Hz FIFO_RELAXED panel caps fps at 155–159,
so fps cannot express a regression at all. The harness already had `gpu_ms_p95` as an assertable
metric. Writing three thresholds and calling it done would have produced a gate that does not gate,
for two separate reasons, and both had to be measured before a number could be written.

### Reason one: a p95 needs about 800 frames before it is a statistic

Four runs each, same machine, same build, d3d12:

| scenario | frames | `gpu_ms_p95` across runs | spread | median across the same runs |
|---|---|---|---|---|
| `stress_pose` | ~800 | 5.05, 5.09, 5.14, 5.21 | **3%** | 4.70–4.72 (0.4%) |
| `valley_far` | ~320 | 4.45, 4.93, 5.92, 6.46 | **45%** | 3.78–4.03 (7%) |

And it is not a d3d12 quirk — vk shows the same shape (`valley_far` p95 4.76 / 3.68 / 3.53, a 35%
spread, against `stress_pose`'s 3.80 / 3.80 / 3.82 at 0.5%).

**At 320 frames the 95th percentile is the sixteenth-worst frame, which is one hitch away from
anything.** The median of the same short run is stable. So `gpu_ms_median` was added as a metric,
`valley_far` gates on it, and the long scenarios gate on p95 — with the table written into
`assertion.hpp` beside the enum so the next author picks correctly instead of re-deriving this.

The first cut of this gate had `valley_far` on p95 at a threshold taken from two runs. It failed on
its third run. That is the whole reason this section exists.

### Reason two: one threshold for two backends is not a gate

vk and d3d12 differ by **30–35%** here, so a threshold that must hold on both is set by d3d12 and
hands vk that much slack. And `ctest -L scenario` runs **vk only** (the Windows CI runner has no
Vulkan ICD, so CI excludes these entirely — see the note below). **A 30% vk regression would have
sailed straight through the gate that is actually run.**

So the scenario grammar gained an optional trailing backend on `assert`, the same shape as
`capture ... no-golden`:

```
assert gpu_ms_p95 < 4.4 vk
assert gpu_ms_p95 < 6.0 d3d12
```

An assertion narrowed to the other backend is **skipped**, not passed, so a per-backend budget can
never be mistaken for one that held everywhere. Round-trips through `emit_scenario`, and is tested —
including that `vulkan`, `both` and `gpu` are all rejected, because a typo that silently widened
every gate back to both backends is precisely the failure this feature exists to prevent.

### The gate as it ships

| scenario | metric | vk | d3d12 |
|---|---|---|---|
| `stress_pose` | `gpu_ms_p95` | **< 4.4** | **< 6.0** |
| `fly_transect` | `gpu_ms_p95` | **< 4.5** | **< 5.7** |
| `valley_far` | `gpu_ms_median` | **< 3.6** | **< 4.6** |

Each is ~1.2× the worst of three or four runs on that backend — the smallest margin that survives
run-to-run noise and still fails a 20% tightening.

### The Check, performed on both backends

| | at the shipping budget | at 20% tighter |
|---|---|---|
| `stress_pose` vk | 3.794 **PASS** | 3.804 vs < 3.52 **FAIL** |
| `stress_pose` d3d12 | 4.988 **PASS** | 5.011 vs < 4.80 **FAIL** |
| `valley_far` vk | 2.957 **PASS** | 2.921 vs < 2.88 **FAIL** |
| `valley_far` d3d12 | 3.958 **PASS** | 4.009 vs < 3.68 **FAIL** |
| `fly_transect` vk | 3.906 **PASS** | 4.014 vs < 3.60 **FAIL** |
| `fly_transect` d3d12 | 4.754 **PASS** | 4.807 vs < 4.56 **FAIL** |

**Six of six pass at the budget; six of six fail at 20% tighter.**

`stress_pose` and `fly_transect` are now registered in `ctest -L scenario` alongside the five that
were already there — a gate that is not in `ctest` is a number in a log file. They are the two most
expensive scenarios (11 s and 13 s on vk), which took the labelled suite from 74 s to 102 s. That is
the price of the gate. **292/292 tests pass.**

### Where this does NOT run, stated plainly

**CI does not run it.** The GitHub Windows runner has no Vulkan ICD, which is why the workflow
already excludes the whole label with `-LE scenario` (Prompt 002's finding, not a new one). The gate
is a **local, pre-push** instrument on this machine, and the numbers in the table are this GPU's.
Goal 273's own text anticipates this — *"if the runner has no GPU, say so and gate only the CPU-side
numbers"* — and the CPU-side assertions (`walk_violations`, `inside_solid`, `frames`,
`stance_changes`) do run in the no-GPU jobs and are untouched by this work.

---

## 17. The throughput answer (goal 274) — the number Prompt 007 derives its view distance from

`voxel_harness --scenario throughput_ramp --ramp lod-radius:1,2,4,8,16`, RelWithDebInfo, pose held
fixed. `mean steps` is read out of the marcher's own `steps` debug view, so it is an iteration count
and not an estimate.

### Before and after this whole prompt, Vulkan

| `--lod-radius` | bricks | resident MB | mean steps | **gpu ms p50** | **gpu ms p95** |
|---|---|---|---|---|---|
| | before → after | before → after | before → after | before → after | before → after |
| 1 | 58,266 → 56,364 | 35.3 → 34.2 | 83.4 → 80.8 | 2.99 → **2.22** (−26%) | 3.52 → **2.70** (−23%) |
| 2 | 229,454 → 224,139 | 139.5 → 136.3 | 86.6 → 88.4 | 3.53 → **2.78** (−21%) | 4.28 → **3.03** (−29%) |
| **4** (shipping) | 903,469 → 892,655 | 550.4 → 543.7 | 89.2 → 91.9 | 4.60 → **3.54** (−23%) | 6.21 → **3.83** (−38%) |
| 8 | 3,412,367 → 3,387,265 | 2,082.2 → 2,066.8 | 91.3 → 95.0 | 5.87 → **4.62** (−21%) | 6.17 → **4.92** (−20%) |
| 16 | FAILED | FAILED | — | — | — |

**21–26% faster at every rung, and the p95 improved more than the p50 (20–38%)** — the second number
is AK-B's rebuild-storm work appearing as reduced variance rather than as a lower median. The small
brick-count differences (58,266 → 56,364) are goal 161's `std::floor` terrain fix moving the surface
by a voxel in places, not a measurement artefact.

D3D12 at the same rungs: 2.75 / 3.49 / 4.65 / **6.19** ms p50.

### The answer, in the form goal 274 asks for

**How many voxels can this GPU render, at what detail, at 150+ fps?**

150 fps is a 6.67 ms frame. Everything outside the march measured 0.10 ms (post) plus present.

| | **Vulkan** | **D3D12** |
|---|---|---|
| Highest rung inside the budget | **`--lod-radius 8`** | **`--lod-radius 4`** |
| Resident bricks | 3,387,265 | 892,655 |
| **Voxels represented** (bricks × 512) | **1.73 billion** | **457 million** |
| Resident MB | 2,066.8 | 543.7 |
| Finest voxel | 7.812 mm | 7.812 mm |
| GPU march p50 / p95 | 4.62 / 4.92 ms | 4.65 / 5.01 ms |
| Headroom in the 6.67 ms budget | 1.75 ms | 1.92 ms |

**The conservative, both-backends answer for Prompt 007: 457 million voxels at 7.8 mm, 544 MB
resident, `--lod-radius 4`, comfortably inside 150 fps on vk AND d3d12. Vulkan alone reaches 1.73
billion voxels and 2.07 GB at the same frame rate.**

D3D12's rung 8 is the boundary case and it fails honestly: 6.19 ms p50 and 6.68 ms p95 consume the
entire 6.67 ms budget on the march alone.

### Traversals per second, and the literature

At the shipping default on vk (3.54 ms, 1280×720):

- **260 M primary rays/s** — 921,600 rays / 3.54 ms.
- **23.9 G traversal steps/s** — 91.9 steps × 921,600 / 3.54 ms.
- **937 M rays/s counting every ray cast** — each of the 52% of pixels that hit also casts one
  shadow ray and four AO rays.

Against the published figures, with the comparison's own caveats stated rather than buried:

| system | figure | hardware |
|---|---|---|
| ESVO (Laine & Karras) | 60.9 M primary rays/s at 5 mm | GTX 285 |
| SVDAG (Kämpe et al.) | 170 / 240 MRays/s | GTX 680 |
| Aokana | ~6 ms/frame at 64K `[UNVERIFIED]` | RTX 3060 Ti |
| **this engine** | **260 M primary rays/s at 7.8 mm** | RTX 4070 Laptop |

**This is a modest result for the hardware gap, and saying so is the point.** An RTX 4070 Laptop is
many generations past a GTX 680, and 260 against SVDAG's 240 is not the margin that gap would
suggest. Two honest reasons: this marcher shades as it goes (albedo mottle, grain, fog, water
fresnel, a smoothed normal) where those figures are traversal-only, and §14 measured that **only two
thirds of the march is traversal at all**. The primary-ray figure therefore understates the work by
roughly the same factor it flatters the comparison. Treat the row as "same order of magnitude", not
as a ranking.

### Rung 16 is a BUILDER wall, not a GPU wall — and it does not crash

Prompt 002 recorded rung 16 as `FAILED, no frames`, and `CLAUDE.md` separately records
`--lod-radius 32` crashing in `Builder::build_node`. Rung 16 is neither of those:

```
voxel_app --renderer svo --lod-radius 16 --frames 120 --mode vk    ->  25 s, 0 slow frames, exit 0
voxel_app --renderer svo --lod-radius  8 --frames 120 --mode vk    ->   7 s, 0 slow frames, exit 0
```

**It builds, it renders, and it exits cleanly.** What it does not do is finish building inside the
harness's frame ceiling — the initial build is roughly 20 s against 5 s at rung 8, and loading-screen
frames do not advance the script, so the run hits its ceiling with the script unstarted. The
harness's message (*"the build did not complete"*) is literally correct and was worth confirming
rather than inheriting.

**So the ceiling this ramp finds is the build path's throughput, not the GPU's.** That matters for
Prompt 007: the limit on view distance today is how fast a region can be BUILT, which is exactly
what AK-C's per-cell rebuild exists to change and what §5 of this log's design section costed.

---

## 18. The grid of shallow trees: correct, 52% fewer traversal steps, and goal 254's cost model was wrong (goal 255)

Goal 254 designed it; this built it on the CPU and put it in front of the oracle.

### What it turned out to be: very little code, exactly as the design predicted

The design's central claim was that a grid needs **no encoding change**, and that held completely.
`world/svo/cell_grid.hpp` is a `vector<GridCell>` plus an Amanatides–Woo DDA. A cell is today's
`BrickTree` built with `root_size_log2 = 5`; `tree_layout.hpp` is untouched; `trace_ray` already
normalises by `tree.geometry.origin`, so a **world-space ray goes straight into a cell with no
transform at all**. There are no cross-cell pointers, so ESVO's `far` bit and per-block far-pointer
table are not second-ranked here — they are moot, exactly as §5 argued.

### Correctness: three independent checks, and the last one separates two questions

1. **The oracle, 7,000 rays: 0 mismatches**, 4,888 of them hits. A 64 m world as one tree against the
   same world as 4×4×4 cells of 16 m — compared not to a hand-rolled expectation but to the shipping
   `trace_ray` on the single tree, which pins the grid to the behaviour the renderer already has.
2. **Real terrain at 512 m, distance LOD: 902,616 bricks and 549.8 MB in BOTH structures.** The same
   world, to the brick.
3. **Real terrain, uniform LOD: 4,439 bricks, 2.7 MB, 3,633 hits, and 100.0% ray agreement over
   20,000 rays** in both.

Check 3 exists because check 2 came with **87.4%** ray agreement, and 87.4% needed explaining rather
than excusing. The hypothesis was that a 512 m root and a 32 m root compute *different LOD levels for
the same point* — the level is relative to the root edge and the two roots are four levels apart — so
the structures hold genuinely different geometry and rays are entitled to disagree. Turning LOD off
makes the two structures hold identical geometry, and then they agree **100.0%**. **The traversal is
correct; the 12.6% is the LOD model.** `--uniform-lod` is now a flag on the probe so the two
questions can never be confused again.

### The traversal claim: confirmed, and it is large

Real terrain, 512 m region, 20,000 rays from a ground-level camera:

| | octree steps/ray | grid steps/ray |
|---|---|---|
| one 512 m tree (16 levels) | **21.7** | — |
| grid of 32 m cells (12 levels) | **10.5** | 2.6 |

**A 52% reduction in octree steps**, for 2.6 DDA steps that touch a flat array instead of chasing
pointers. Costed at §14's measured exchange rate — a 29% step cut bought 19% of march time, so
roughly 0.65× — that is worth about **a third of the march**, which would be the largest single win
identified in this prompt. It is not banked: goal 256's shader mirror is what would bank it.

### The build-cost claim: wrong as stated, and the reason is worth more than the number

Goal 254 predicted **7–14 ms per cell** from an area argument: terrain is a surface, one 32 m cell is
1/256 of a 512 m footprint. Measured, 512 m region, real terrain, cells built in parallel with
goal 251's shared focus tiers:

| | measured |
|---|---|
| whole region, one tree | **2.354 s** |
| whole region, as 4,096 cells | **2.630 s** (+12%) |
| per cell, p50 | **0.6 ms** |
| per cell, p95 | **21.3 ms** |
| **the camera's own cell, alone** | **255.6 ms** — 87,450 bricks, 9.7% of the region's total |

**Building the world as a grid costs 12% more, not 6× more, and most cells are free.** But the cost
is not uniform and the area argument does not describe it: **cost follows DETAIL, and distance LOD
concentrates detail at the camera.** One cell holds a tenth of the region's bricks and costs 255 ms
on its own — 400× the median cell and 18× goal 254's prediction.

### What that means for goal 257, stated as the blocker it is

Per-cell rebuild is cheap for the 4,000 cells that cost 0.6 ms and viable for the ones that cost
21 ms. It is **not** viable for the LOD-centre cell at 255 ms — and worse, with a *continuous*
distance LOD **every cell's content depends on the camera position**, so moving the camera dirties
the whole grid rather than a ring of it. The grid does not fix that on its own.

**The specific prerequisite, now named: per-cell LOD quantisation.** A cell's level must be a
function of its distance BAND, not a continuous function of camera distance, so that a camera move
re-levels only the cells that crossed a band. That is a change to the build's LOD model, not to the
grid, and it is what goal 257 actually needs. Recorded as a goal rather than attempted here.

### Three more instruments caught measuring nothing

This section's running total is now **ten** for the pass, and all three were mine, in one tool:

- **The camera was inside the hill.** Hard-coded `y = 20`; all 20,000 rays reported 1.0 steps and a
  hit. The eye height is now derived from the heightmap.
- **A serial grid against a parallel region.** Cells were built one at a time, each handing the pool
  to `build_tree` — but a 32 m cell has almost nothing to split. That reported the grid **6.1×
  slower**. The parallelism in a grid is *between* cells; measured that way it is 12% slower.
- **Two different worlds.** The region snapped its origin to 8 m and the grid to 32 m, so they were
  offset by 16 m and the grid "missed" two thirds of the hits. Both snap to the cell edge now.

Every one of them produced a plausible number. The pattern this pass keeps re-learning: **a
measurement that confirms what you expected is exactly as likely to be broken as one that does not**
— the serial-build error looked like a real refutation of the design, and it was an artefact.

---

## 19. The grid on the GPU: 22% on d3d12, nothing on vk, and the backends converge (goal 256)

§18 built the grid on the CPU and passed the oracle. This is the shader mirror, the app integration,
and the number.

### What had to move, and why the collider and the crosshair were not optional

The renderer was the easy third of it. `OctreeCollider` had to follow because **its entire
justification is that it answers from the same structure the renderer marches** — leaving it on a
single tree would have silently reintroduced the clipping bug Prompt 003 built it to remove — and
`query_aim_octree` for the same reason, which the crash handler demonstrated immediately by catching
a null-tree dereference on the first grid run.

The shader change is smaller than it sounds because the CPU groundwork was right: `TraceRay` became
`TraceCell(Cell, ...)` where `Cell` mirrors `TreeView`, and **`MakeWholeCell()` turns the old
single-tree globals into a grid of exactly one cell**, so the pre-grid path is not a separate code
path and cannot drift. With `g_GridDims.x <= 0` the shader is bit-for-bit what it was: the checkpoint
commit confirmed it at 0.0470% of pixels against the shipping golden.

### FXC rejected what Vulkan accepted, exactly as CLAUDE.md warns

The grid shipped working on vk and failed to compile on d3d12:

```
X3500: array reference cannot be used as an l-value; not natively addressable
X3511: forced to unroll loop, but unrolling failed
```

The cause is worth writing down because it is a *second* face of the documented X3500 trap: my
`[unroll]` loops contained `continue` and `return`. FXC will not unroll a loop containing them, and
once the loop is not unrolled the `cellCoord[c]` / `tMax[c]` writes become **runtime-indexed vector
component writes**, which FXC also rejects. Both loops are now branch-free with the early-outs
hoisted after them. **Had I only tested vk, this would have shipped broken on d3d12.**

### The measurement

`stress_pose`, interleaved rungs (`0,5,0,5`) so clock drift cannot fake it:

| | bricks | resident MB | octree steps/ray | **march ms (vk)** | **march ms (d3d12)** |
|---|---|---|---|---|---|
| one 512 m tree, 16 levels | 892,655 | 543.7 | **91.9** | 3.62 / 3.71 | 4.59 / 4.59 |
| grid of 32 m cells, 12 levels | 891,811 | 543.1 | **33.7** | 3.62 / 3.59 | **3.58 / 3.59** |
| | −0.09% | −0.1% | **−63%** | **unchanged** | **−22%** |

**Same world (0.09% fewer bricks, the LOD-level difference §18 explains), 63% fewer octree steps, and
a 22% faster march on d3d12 while vk does not move at all.**

### The interesting part is the split, not the average

Before the grid, d3d12's march was **27% slower than vk's** (4.59 against 3.62) on identical work.
After it, they are the same (3.58 against 3.62). **The grid did not make the marcher faster in
general — it removed a penalty that only d3d12 was paying.**

The honest reading, and it is a hypothesis rather than a measurement: the deep tree's chain of
dependent, cache-missing loads costs more under one backend's compiler and scheduler than the
other's, and shortening the chain from 16 levels to 12 removes exactly that. Vulkan was evidently
not bound by the chain at this pose, which is consistent with §14's finding that only about two
thirds of this march is traversal at all.

Note also that the 63% step cut is **not** comparable to §14's 29% cut: `mean steps` is the octree
iteration count *inside a cell* and does not count the grid DDA that replaces the removed levels.
The grid walk is not free — each step fetches a cell record and does a slab test — and vk's flat
result is what that costs when the chain was not the bottleneck.

### Verified by looking, on both backends

`research/captures/akc_cell_grid_vs_tree.png` — the single tree on vk, the grid on vk, the grid on
d3d12, same pose. **The same world in all three.** A ×10-amplified difference image against the
single tree shows fine banding on the terrain surface and nothing else, which is the LOD-level
difference §18 proved on the CPU: with `--uniform-lod` the two structures are identical and agree on
100.0% of 20,000 rays.

Both configurations report **23.7%** on `--verify-frame` — the same number to the tenth.

### Build cost in the app, and where 257 stands

The app's own build log, same seed and pose, 128 m region: **1.20 s as one tree, 1.28 s as a grid**
(+7%), 488,061 bricks against 487,621. At 512 m the controlled probe in §18 measured 2.354 s against
2.630 s (+12%). Building as a grid is not the problem; §18's finding stands that **per-cell rebuild
(goal 257) needs per-cell LOD quantisation first**, because with a continuous distance LOD every
cell's content still depends on the camera.

`--cell-log2 N` selects it (0 = the single tree, and the default). 304/304 tests.

---

## 20. Per-cell rebuild: 10.4× faster, and the blocker §18 named was real (goal 257, closes goal 158)

§18 ended by naming the prerequisite rather than guessing at it: *"with a continuous distance LOD
every cell's content depends on the camera position, so moving the camera dirties the whole grid
rather than a ring of it."* This built the fix and measured what it bought.

### The fix: quantise detail to a distance BAND

`world/svo/lod_bands.hpp`. Band 0 is full resolution; each band beyond doubles the voxel edge, with
boundaries at `lod_radius`, 2×, 4×, … A cell's content becomes a **step function** of camera
distance — it does not change at all until the camera crosses a boundary. `BuildParams` gained
`quantized_voxel_edge`, which replaces the distance ramp with one target for the whole cell, so a
cell's build depends only on its band and not on where the camera is.

Two details that are correctness, not taste, and are asserted:

- **Distance is to the cell's NEAREST point, not its centre.** A cell the camera is standing inside
  must be band 0 however large it is, and its centre can be 16 m away.
- **A NaN distance must be band 0.** The test writes it as `!(d > r)` rather than `d <= r` precisely
  so a NaN cannot become a band that asks for a voxel the size of the world.

### What it bought, measured on the shipping configuration

Same seed, same pose, 512 m region, 32 m cells, 4,096 cells:

| build | cells rebuilt | cells reused | **time** | bricks | MB |
|---|---|---|---|---|---|
| #1, cold | 4,096 | 0 | **4.07 s** | 464,456 | 283.6 |
| #2, after the camera moved | **138** | **3,958** | **0.39 s** | 479,891 | 292.9 |

**96.6% of the grid carried over, and the rebuild is 10.4× faster than the cold build** — and 3–8×
faster than the single-tree rebuild it replaces (1.20–3.25 s measured in §19). On a longer flight
the second build reused 3,430 of 4,096 (84%) after a larger move, which is the same story at a
different step size.

The band function's own property is tested directly rather than inferred: an 8 m camera move — the
rebuild trigger goal 249 settled on — re-levels **under 35%** of a 16³ grid, against **100%** under
the continuous rule. Standing still re-levels nothing.

### What it costs, stated plainly

**The cold build is slower: 4.07 s against ~2.4 s for one tree.** The reason is structural rather
than a bug: a band-0 cell is built at the finest voxel across its *whole* 32 m extent, where the
continuous ramp would have coarsened the far side of that same cell. So the shell of cells near the
camera is more expensive than it was. That is the price of making their content independent of the
camera, and it is paid once per session rather than once per 8 m of motion.

**And the detail ramp becomes a staircase.** `research/captures/akc_banded_lod.png` puts the three
side by side at one pose — one tree with the continuous ramp, the grid with it, and the grid with
bands. **All three are the same world, and the banded one is if anything crisper near the camera**,
for the same reason it is more expensive to build. `--verify-frame` reads **19.8%** against the
continuous rule's **23.7%**, which is the far terrain being one band coarser and is the only visible
difference at this pose.

### The half that is NOT done, and it is AK-D's

Goal 257's Check asks for **MB uploaded per second of flight, before and after**. The build is now
incremental; **the upload is not.** `FlatCellGrid` concatenates every cell into one node array and
one brick array, so rebuilding 138 of 4,096 cells still re-uploads all **292.9 MB**.

That is not an oversight in this goal — it is exactly what AK-D's fixed-capacity node and brick
**pools with slot allocation** exist to fix. A cell that did not change must keep its slots and not
be re-sent, and that requires the pools (goal 260), which requires the residency flags the
`FlatCell` record already carries a bit for. **Goal 158 ("incremental rebuild") is closed on the
build side and its upload side is goal 260's.**

309/309 tests.
