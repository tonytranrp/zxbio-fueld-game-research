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
