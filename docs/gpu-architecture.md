# The GPU voxel architecture

What the renderer actually is after Prompt 004, in the order you need it to change something:
the structures, the residency rules, the request path, the frame budget, and **how to measure each
one** — because every number below came from a measurement that is repeatable, and a number you
cannot re-take is a number you will not trust in six weeks.

Reasoning, rejected alternatives and every before/after:
[`research/frame-time-and-gpu-architecture-log.md`](../research/frame-time-and-gpu-architecture-log.md).
Backlog and Checks: [`docs/goals.md`](goals.md) Group AK.

---

## 1. The shape in one paragraph

The world is a **grid of independent shallow octrees** ("cells"), not one deep tree. A ray walks the
grid with a 3D DDA and enters each cell at that cell's own root; **there are no cross-cell
pointers**, which is the property everything else depends on — a cell can be built, uploaded,
evicted or replaced without invalidating anything outside it. Cells live in two **fixed-capacity GPU
pools** (bricks by slot, nodes by repack). The marcher **marks which cells it used and requests the
ones it wanted**, through a UAV read back three frames later; a CPU-side LRU evicts by that. A ray
entering a cell that is not resident is answered from an **always-resident coarse proxy** rather than
passing through, so **no frame ever has a hole in it**.

---

## 2. The structures

### 2.1 The cell grid

| type | header | what it is |
|---|---|---|
| `CellGrid` | `world/svo/cell_grid.hpp` | grid of `shared_ptr<const BrickTree>`, one per cell, plus the DDA |
| `TreeView` | `world/svo/cell_grid.hpp` | non-owning window: node span, brick span, root offset, geometry |
| `FlatCellGrid` | `world/svo/cell_grid.hpp` | the GPU-shaped concatenation: one node array, one brick array, one record per cell |
| `ResidentGrid` | `world/svo/resident_grid.hpp` | the persistent version — pooled brick slots, dirty runs, per-cell install/evict |

**A cell is exactly a `BrickTree`** built with `root_size_log2 = 5` (32 m) instead of 9 (512 m), and
positioned by its own `geometry.origin`. `tree_layout.hpp` is untouched, so the 7,000-ray oracle
keeps its meaning against every cell individually. `trace_ray` already works in world space, so a
cell needs no coordinate wrapper.

Why a grid at all, in the two numbers the design rests on:

- **Rebuild cost is by AREA**, because terrain is a surface. One 32 m cell is (32/512)² = **1/256**
  of a 512 m region's footprint, against a measured 1.89–3.60 s whole-region build.
- **Chain length**: 16 levels become 12 — a 25% shorter chain of dependent cache-missing loads per
  ray, and a 13-deep shader stack instead of 22. Measured: **52% fewer traversal steps** than the
  single deep tree, and **−22% march on d3d12** (which converges the two backends; vk was unchanged).

### 2.2 The per-cell record — `FlatCell`, 4 words

```
uint32 node_base    word offset of this cell's first node in the shared node array
uint32 brick_base   BRICK INDEX (not a word offset) of this cell's first brick
uint32 root         root header offset, relative to node_base
uint32 flags        bit 0 = present, bit 1 = resident-and-empty
```

**Bit 1 is not redundant with bit 0, and getting this wrong is a real bug that happened.** "No
geometry here" and "not loaded yet" look identical to a marcher — both are a cell it cannot trace —
but they are *opposites* to the never-stall fallback (§4): an **absent** cell must be answered from
the coarse proxy, and an **empty** one must not be, or the proxy's coarser voxels put geometry into
space the fine build correctly found empty. The symptom was a proxy hit 0.3 m in front of the camera.

### 2.3 The node encoding (unchanged since the pivot, layout v2)

One header word; bits 0–7 child mask, 8–9 kind (0 internal, 1 brick leaf, 2 solid leaf), 16–23 the
representative material. Internal nodes and brick leaves carry one **attribute** word — three int8
snorm normal components plus a uint8 coverage — so a hit can be shaded with a normal averaged over
any ancestor's extent.

```
internal:   [header][attributes][child pointer per set mask bit ...]
brick leaf: [header][brick index][attributes]
solid leaf: [header]
```

Air is not a node — it is an unset mask bit. Octant bit layout: bit0 = +x, bit1 = +y, bit2 = +z.
`tree_layout.hpp` and `svo_march.psh.hlsl` mirror this and **must change together**.

### 2.4 The brick — 70 words, 280 B, palette-compressed (goal 258)

```
words [0, 16)   512-bit occupancy mask, bit i = voxel i is not Air
words [16, 68)  512 three-bit palette indices, TEN PER WORD (30 bits used, top 2 unused)
words [68, 70)  the 8-entry palette, one MaterialID byte per entry, 4 per word
```

- **Ten per word, not the 10.67 that fit**: 3 does not divide 32, so a dense pack straddles a word
  boundary every ~10 voxels and a correct fetch would need two loads and a shift-select. Four wasted
  words buy a fetch that is **exactly one load with no boundary case**.
- **Entry 0 is always Air, and an Air voxel always stores index 0.** A non-zero entry therefore holds
  Air *iff* it has never been assigned, which is what lets interning work with no separate
  "entries used" counter — and makes an all-zero brick a valid empty brick.
- A `static_assert` pins `kMaterialCount <= 8`. **Adding a ninth material is a build error**, not a
  rare corrupted brick, and its message names both fixes (a wider index, or a second pool size class).

Measured: **543.68 → 279.46 MB resident** at the shipping pose for **+1.5% march on vk** and no
measurable change on d3d12.

### 2.5 The pools

`BrickPool` (`world/svo/brick_pool.hpp`) is a fixed-capacity slot allocator — every brick is exactly
`kBrickWords` long, so it is a perfect fixed-size allocator with LRU stamps. `allocate()` returns
`kNoSlot` when full; `touch()` is idempotent (a `max` on the stamp); `evictable(frame, max)` returns
oldest-first; `slots_for_budget` does the byte arithmetic in one place.

**Bricks are pooled; nodes are repacked and re-sent.** That split was chosen from where the bytes are
(bricks are **94.6%** of resident bytes), not from an architecture diagram, and §21 of the log books
the bill it presents: a stationary 2,000-frame streaming run sends **3,011.6 MB total**, almost all
of it the node array being re-sent whole on ~511 install frames. Pooling nodes too would cut that to
roughly 300 MB. **Streaming's win today is latency and smoothness, not total bytes** — say it that
way, because the total-bytes number looks bad on its own and is honest in context.

---

## 3. The request path

1. **The marcher marks.** Under `SVO_MARK_USAGE` the pixel shader writes each cell it stepped into to
   `RWStructuredBuffer<uint> g_CellUsage`, and sets a request bit when it *wanted* a cell that was
   not resident. Encoding (`world/svo/cell_marks.hpp`):
   ```
   bit 31      kCellRequested (0x80000000) -- a ray wanted this cell and could not have it
   bits 0..30  kCellFrameMask (0x7FFFFFFF) -- the frame index it was last used on
   ```
   One word per cell, no atomics: a plain store is enough because every writer writes the same frame
   index, and the request bit is monotone within a frame.
2. **The CPU reads it back three frames later**, through a fenced ring so nothing stalls.
   **16 KB per frame** at the shipping grid size — small enough that the readback is not the cost.
3. **`CellMarks::compact`** turns the sparse word array into a used-list and a request-list in one
   pass, ordered nearest-first.
4. **The producer plans, then pumps.** `SvoWorld::start_stream` **PLANS** (fixes the grid shape,
   builds the coarse proxy) and `pump_stream` **SUBMITS** a bounded slice per frame
   (`--cells-per-frame`, default 8). Splitting these two is not stylistic: the first working version
   built the proxy synchronously on the frame loop and submitted all 4,096 cells at once, and put
   **1,710 ms on a single frame**. After the split: **7 slow frames of 2,000, none on swap, none
   while uploading.**
5. **Install / evict.** `ResidentGrid::install` takes brick slots from the pool and records the cell
   present (or **present-and-empty**, §2.2). `evict` returns slots. Node arrays are repacked and the
   brick indices inside them rewritten — **structurally, walking from the root**, because a linear
   walk of the node array assumes a gapless layout that `tree_layout.hpp` never promises. That
   assumption was made once and showed up as a hit where the flat grid saw air.

### The 30% trap, worth knowing before you add a second UAV

**A bound pixel-shader UAV costs ~30% of the march even when nothing writes to it.** Caught by the
goal 273 gate on the *marking-off* path, which is the whole argument for having built the gate first.
The fix is two PSOs behind an `SVO_MARK_USAGE` define so the non-marking path binds no UAV at all.
If you add another UAV, add another define — do not bind it unconditionally.

---

## 4. The never-stall rule

**A ray must never pass through space it cannot resolve.** A cell that is not resident (`flags`
bit 0 clear, bit 1 clear) is traced against an **always-resident coarse proxy**: one tree over the
whole region at 1 m voxels, built once, ~9 levels, small enough to keep forever. It is traced over
that cell's own DDA span only, so a coarse answer never leaks past the cell that needed it.

Shader constants:

```
g_ProxyOrigin  xyz = world min corner, w = root edge in metres (w == 0 means NO proxy)
g_ProxyInts    x = root node offset, y = voxel bits V, zw spare
```

`w == 0` disables the proxy and restores pre-263 pass-through, which is how the two are A/B'd.

**How you know it works**: 29 dumps over a 240-frame run — **frame 8 is the complete landscape at
1 m voxels with zero fine bricks resident**, and **near-black pixels stay at 0.00% on every frame of
the sequence**. That is the mechanical form of "no holes, no black, no sky where terrain should be".
Capture: `research/captures/akd_never_stall_convergence.png`.

---

## 5. The constant buffer, which is where the silent bugs live

The march cbuffer's tail order is, in C++ (`MarchConstantsCpu`) and HLSL, exactly:

```
g_WaveParams, g_GridDims, g_GridOrigin, g_MarkParams, g_ProxyOrigin, g_ProxyInts, g_Materials
```

**These are the same size, so a `static_assert` on the total will not catch a reordering.** It
happened: `g_MarkParams` sat between `g_GridDims` and `g_GridOrigin` in HLSL and after `gridOrigin`
in C++, the assert passed, and the world rendered empty — 0.0% contrast against 19.8%. The new UAV
write was the obvious suspect and was innocent. **If the world goes empty after a constants change,
check field ORDER before anything else.**

Two more that cost real time:

- **`voxel_harness` is a different binary from `voxel_app`.** Shaders load at runtime so a shader
  edit needs no rebuild, but the C++ cbuffer mirror does, and the harness renders with its own copy.
  **A cbuffer change means rebuilding every binary that renders**, not just the one you are running.
  Goldens went to 48.5% before this was noticed, and the bisect ran four times.
- **FXC will not unroll a `[unroll]` loop containing `continue` or `return`**, which makes any vector
  component write inside it runtime-indexed → **X3500**, and Vulkan's compiler accepts what d3d12
  rejects. Both loops in the marcher are branch-free with early-outs hoisted out. **Test both
  backends on every shader change.**

---

## 6. The frame budget

`stress_pose` (ground level at a hilltop, shadows and AO on) is the budget pose — not the panorama.
Measured on this machine, RelWithDebInfo, vsync off, after this pass:

| | vk | d3d12 |
|---|---|---|
| march median | 3.50 ms | 5.5–6.1 ms |
| whole-frame GPU | ~3.8 ms | — |
| frame median | ~6.1 ms | — |
| resident | 279.5 MB (892,655 bricks) | same |
| peak GPU | 333.1 MB | same |

Where the march goes, and the two facts that should shape any plan to shrink it:

- **Secondary rays were 49% (vk) / 38% (d3d12) of the marcher** before goal 268; the primary ray
  alone is 2.50 ms, i.e. 400 fps. **Optimising primary traversal is optimising the smaller half.**
- **Warp efficiency is 92.0–94.1%** across six poses and every tiling, with step divergence ~7%.
  There is no occupancy prize left — which is why the compute port is a measured *no*, not a
  deferral.

Throughput, in the form Prompt 007 needs it: **457 million voxels at 7.8 mm, 544 MB resident, at
150+ fps on both backends** (conservative, both-backends), which is **260 M primary rays/s** and
**23.9 G traversal steps/s**.

---

## 7. How to measure each of these

| what | how |
|---|---|
| frame attribution | `voxel_harness --scenario X --report R.json`; nine phases account for 99.9% of wall time. A frame that writes a PNG is **not a normal frame** — `capture` is 200+ ms. |
| march GPU ms | the report's `gpu_march_ms` percentiles; `--gpu-timers` on (cost measured at 0.16% of a frame) |
| one shading term | `--debug-view lit\|ao\|normal\|facenormal\|level\|steps\|coverage\|cubepx\|smooth\|lodcube\|material\|distance` — reach for this **before** staring at a composite |
| traversal steps/pixel | `--debug-view steps`; `run_ramp.cpp`'s `mean_steps_from()` reduces the capture to a number |
| warp efficiency | `world/svo/warp_divergence.hpp` over a `steps` capture |
| memory | report counters `bricks`, `internal_nodes`, `resident_mb`, `peak_gpu_mb` |
| upload traffic | the exit summary's MB total and per-frame mean; slow-frame causes are attributed by phase |
| a regression | `ctest -L scenario` — but read §8 first |
| an artefact, on the CPU | `tools/svo_render --lod-center x,y,z --view NAME`. Reproduce on the CPU **before** touching the shader; it prints a per-level brick histogram and the camera column probe |
| CPU/GPU agreement | the 7,000-ray oracle. It must stay 0/7,000 through any change to `ray_trace.cpp` or `svo_march.psh.hlsl`, which change **together** |

### 8. What the regression gate can and cannot tell you

`stress_pose` carries four assertions — a **median** per backend (the regression signal) and a
**p95** per backend (a loose tail guard). Read `dev/scenarios/stress_pose.scn`'s own comment block
before changing any of them; the short version:

- **An absolute millisecond threshold on this machine has an ~18% noise floor that belongs to the
  laptop.** Twelve runs early in a session measured vk median 3.49–3.50; three hours of continuous
  GPU load later the same binary at the same pose measured 4.01–4.14, and `nvidia-smi` *during* a run
  reads **2445 MHz of a 3105 MHz maximum at 23 W with no thermal-slowdown flag** — a lower boost
  state. 3.50/4.13 = 84.7% against 2445/3105 = 78.7%: the clock explains essentially all of it.
- **Contention moves the tail and leaves the median alone.** A single p95 gate flaked one run in
  three inside a full `ctest`, at a median indistinguishable from standalone.
- **d3d12's gate is a guard only** — its 18% spread is wider than a 20% tightening margin, so it
  cannot be both non-flaky and falsifiable. vk carries goal 273's falsification Check, and vk is what
  `ctest -L scenario` runs (**CI runs neither**: the GitHub Windows runner has no Vulkan ICD).
- **The fix is a clock-independent metric** and it is goal 275e: `mean_primary_steps` is
  deterministic — same scene, same pose, same number on any clock and either backend — and it already
  exists in the ramp path.

---

## 9. Things that are true and non-obvious

- **`--upload-budget` still means exactly what it meant**: a per-frame byte ceiling. It is applied to
  dirty brick runs now instead of tree slices, which is why it was kept rather than deleted.
- **Diligent MUTABLE SRB variables bind exactly once per SRB**; a second `Set` is silently ignored in
  Release. Anything replaced at runtime must be a DYNAMIC variable.
- **`psoCI.pPS = createShader(...)` binds a temporary `RefCntAutoPtr`** and releases the shader before
  PSO creation — an access violation at address 0 with no stack. Hold it in a named variable.
- **Vulkan faults if the app's very first command is a timestamp query**; the GPU timer skips the
  first two frames.
- **No apron, ever.** A one-voxel apron on an 8³ brick is a **1.95× memory blow-up** — it would undo
  the entire palette lever and then some. If filtered brick sampling is wanted, the corner-centred 3³
  trick or explicit neighbour fetches are the published alternatives.
