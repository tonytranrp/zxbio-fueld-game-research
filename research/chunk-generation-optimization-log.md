# Chunk-generation load-time optimization pass (2026-09-05)

## The complaint

Launching `voxel_app` via Visual Studio's own F5/debugger (the `windows-debug` CMake preset —
the only "run it interactively" option that existed before this pass) made the loading screen
take an extremely long time, far beyond the already-documented 53.6s Release baseline for the
same radius-48/56,454-chunk world (`docs/progress.md`). This is exactly the profiling work
`docs/goals.md` goal 132 had explicitly flagged as undone: *"reaching [8km] later is real
profiling work (where does ~1ms/chunk actually go), not a parameter bump."*

## Method

Read the real `WorldLoader`/`ChunkVoxels`/`mesh_extractor` source before touching anything —
no changes were made on a guess. Three real, independently-verifiable inefficiencies were found
this way, plus one build-configuration gap. A `web-researcher` agent then cross-checked the plan
against production voxel-engine prior art and MSVC's own documented build-flag behavior *before*
any fix was implemented — confirmation and refinement, not a substitute for reading this
codebase's own source. Every fix below was then measured, not assumed: the SAME radius-48 world
(56,454 real chunks, 78,408 including the meshing halo) was timed before and after, using the
already-established Release-build methodology from the redesign pass (`voxel-representation-
redesign.md`).

## Finding 1: `consider_mesh_candidate`'s per-chunk deep-copy snapshot

`WorldLoader::consider_mesh_candidate` (carried over unchanged from the old per-tick streaming
system per the redesign's own note) allocated a brand-new `ChunkStore` — its own
`std::pmr::synchronized_pool_resource` plus a `boost::unordered_flat_map` — and deep-copied all
27 neighboring chunks' `ChunkVoxels` into it, **synchronously on the main thread**, once per real
chunk (~56,454 times). This was a real, necessary safety measure in the OLD system (a live store
under concurrent per-tick mutation); in the new one-time pregeneration pass it is pure overhead,
because:

- A chunk's voxel data is write-once: `drain_generation_completions()` assigns it exactly once,
  and `consider_mesh_candidate(coord)` only runs once ALL 27 of `coord`'s neighbors (including
  itself) are already in that frozen, post-write state (`neighborhood_generated()` just proved
  it).
- `ChunkStore`'s pointees are heap-stable across the underlying flat map's own rehashing (already
  documented in `chunk_store.hpp`) — a concurrent `get_or_create()` for some unrelated coordinate
  can move map *slots* around, never the `Chunk` objects a previously-resolved pointer names.

So the 27 neighbor pointers only need to be *resolved* (27 `ChunkStore::find()` calls) on the
main thread — the one thread that ever mutates the store — and can then be read from a worker
with zero further synchronization. Fix: `world::meshing::NeighborCache` (previously a private
implementation detail of `mesh_extractor.cpp`) moved into the public header with a second
constructor taking an already-resolved `std::array<const Chunk*, 27>`, plus a
`NeighborCache::resolve(store, coord)` static helper. `WorldLoader` now resolves once and hands
the trivially-copyable 216-byte array to the worker lambda by value — no allocation, no copy of
voxel data, no second `ChunkStore` at all. `extract_mesh(const ChunkStore&, ChunkCoord)` stays as
a one-line wrapper over the new `extract_mesh(const NeighborCache&)`, so every existing test/tool
call site (10+ of them) needed zero changes.

A `web-researcher` pass afterward found this is *stronger* than what Sodium (the most
widely-used, heavily-optimized Minecraft rendering mod) does for the analogous case: Sodium
**does** clone chunk data before meshing, but for a reason that doesn't apply here — Minecraft's
world is mutable *during* meshing (a player can break a block mid-job), so a snapshot is a
correctness requirement for them, not just a performance one. This project's world is frozen
immediately after generation, a strictly stronger invariant that makes Sodium's mitigation
unnecessary here.

## Finding 2: `ChunkVoxels` palette-promotion thrashing during fill

`ChunkVoxels::set()` grows its palette lazily and calls `promote(newBits)` every time the
palette's required bit-width crosses 0→1, 1→2, or 2→4 bits — each `promote()` re-packs the
**entire** 32,768-voxel index buffer from scratch, an O(chunk size) cost paid again at every
boundary. `terrain_fill.cpp`'s per-voxel loop can introduce Stone/Dirt/Sand/Grass/Water in
whatever order a chunk's columns happen to produce them, so a "busy" surface chunk could cross
all three boundaries and pay the full repack cost up to three times redundantly for one fill.

Fix: `ChunkVoxels::reserve_bits(bits)` — widens the index buffer to a target bit width once, up
front, from the cheap all-zero starting state (no re-read loop needed when `bits_` starts at 0).
`fill_terrain` calls `chunk.voxels().reserve_bits(ChunkVoxels::bits_for_palette_size(kMaterialCount))`
right before its per-voxel loop, sizing for this project's real (self-updating) material count
instead of letting incremental growth discover it. `bits_for_palette_size` itself moved from a
private free function in `chunk_voxels.cpp` to a public `static constexpr` on the class, so both
the internal promotion logic and this external call site share one table.

Research cross-check: mainstream palette-compression references (Minecraft's own format,
Glowstone, oxidized-chunks) all use the same incremental-promote-and-rebuild shape this project
already had — no fundamentally different general-purpose algorithm exists in what was found.
The reason a pre-widen is viable *here* and not in Minecraft's own format: Minecraft's palette
must support arbitrary future edits over an unbounded chunk lifetime, so it genuinely can't know
the final bit-width in advance. This project's fill happens once, during generation, from a
small, fully-enumerable, known-in-advance material set — an easier problem Minecraft's
general-purpose format isn't solving.

Measured memory cost of always reserving 4 bits (this project's real material count, 8, needs
exactly 4): **343 MiB → 347.85 MiB total (+1.4%)** for the full radius-48 world
(`measure_world_memory`, re-run after the fix). Far smaller than a naive worst-case estimate —
most non-homogeneous chunks already needed 4 bits under the old incremental scheme too (a chunk
touching Stone+Dirt+Grass+Sand+Water needs 4 bits regardless of promotion order), so the fix
mostly just avoids redundant *repacking*, not redundant *storage*. The homogeneous fast path
(56.4% of all chunks, unchanged: 44,194/78,408) never reaches `reserve_bits` at all, since it's
still short-circuited before the per-voxel loop.

## Finding 3 (deferred, not implemented this pass): per-column heightmap redundancy

`fill_terrain` calls `heightmap.generate_column_heights()` (a 34×34 FastNoise2 grid) once per
chunk, unconditionally, even for chunks the min/max short-circuit immediately discards as pure
air or pure stone. Chunks stacked at the same (x,z) column across the world's Y-range (6 real
layers + halo) recompute the *identical* 2D result up to 8 times. A `web-researcher` pass
confirmed this is a real, named, standard pattern elsewhere — Veloren's `WorldSim`/`SimChunk`
batches the entire world's 2D layer in one parallelized pass before any 3D chunk generation;
Cuberite's `cHeiGenCache` is a purpose-built LRU cache for exactly this. Deliberately NOT
implemented this pass: the real timing breakdown below shows generation (54.22s CPU-time) is
already the smaller of the two phases by ~7.8x — even fully eliminating this redundancy would be
a smaller win than what Findings 1+2 already delivered, and this project's own static/bounded
world (goal: a known column set in advance) would make the "precompute the whole 2D layer up
front in one parallel pass" shape (the research-preferred option over a lazily-populated
concurrent cache) a clean fit for a *future*, dedicated pass — not a rushed addition here.

## Finding 4: no fast, debuggable build config existed

Only `windows-debug` (MSVC debug CRT, `_ITERATOR_DEBUG_LEVEL=2` by MSVC's own documented default)
and `windows-release` existed. This project had ALREADY independently measured IDL=2's cost for
concurrent hash-map-heavy code at ~80x (`mesh_extractor.cpp`'s `NeighborCache` comment, from the
original Phase-1-era streaming work) — Visual Studio's own F5 defaults to whichever config was
last selected, and a user with no reason to know about that measured cost has no way to avoid it
short of manually switching the dropdown to Release.

Fix: added a `windows-relwithdebinfo` CMake preset. Verified against MSVC's own documented
`_ITERATOR_DEBUG_LEVEL` default-value rule (IDL defaults to 0 whenever `NDEBUG` is defined, i.e.
whenever the code is NOT linked against the debug CRT) and CMake's own documented MSVC
RelWithDebInfo default flags (`/MD /Zi /O2 /Ob1 /DNDEBUG`) — `/MD` + `/DNDEBUG` puts this
squarely on the release-CRT side of that rule, so this does NOT reintroduce the measured Debug
slowdown. CMake's own default leaves one real gap versus Release, though: `/Ob1` (only
explicitly-`inline`-marked functions) instead of `/Ob2` (the compiler's own inlining judgment) —
real precedent exists for closing this gap (OBS Studio's own RelWithDebInfo hardening added
`/Ob2` for exactly this reason). Since this preset exists specifically so a VS user gets fast
chunk generation without hunting for a config change, it explicitly adds `/Ob2` (plus
`/OPT:REF`/`/OPT:ICF` at link) rather than leaving CMake's more conservative, more
stepping-friendly default.

Deliberately NOT touched: `/fp:fast`. Microsoft's own optimization-guidance doc recommends it for
games, but this project has already independently discovered (documented in CLAUDE.md) that
floating-point instruction selection changes noise output at the rounding-error level across SIMD
levels, which is why `FastSIMD::FeatureSet` is pinned for determinism. `/fp:fast` would risk
making terrain generation non-reproducible across build configs on top of the SIMD-level risk
already handled — a real, named trap to avoid, not just a hypothetical one.

## Real, measured result

Same radius-48 world (56,454 real chunks, 78,408 incl. halo), same machine, Release build,
before vs. after Findings 1+2 (Finding 3 not implemented; Finding 4 is a separate, additive
build-config change with no effect on this number):

- **Before** (redesign-pass baseline, `docs/progress.md`): **53.6s**
- **After** (this pass): **29.9s** — a **44% reduction** in end-to-end load time for the
  identical world.

New per-phase breakdown (`WorldLoader::log_timings()`, added this pass specifically to answer
goal 132's own flagged question — summed across all 16 worker threads, so these are CPU-time
totals, not wall-clock):

```
generation: 54.22s CPU-time over 78,408 chunks (0.692ms/chunk avg)
meshing:   422.83s CPU-time over 56,454 chunks (7.490ms/chunk avg)
```

This is a genuinely new finding, not just a number: meshing (including tree decoration, timed
together) is now **~7.8x** more total CPU-time than generation, and — divided by the 16 worker
threads — meshing's ~26.4s wall-clock-equivalent is the dominant contributor to the 29.9s total,
not generation's ~3.4s. `docs/progress.md`'s existing note that `extract_mesh` being "~2x more
expensive [is] accepted because it's background-threaded and never blocks a frame" is true for
per-frame streaming (a background thread's cost never stalls the render thread's own frame time)
but was never evaluated for *bulk upfront loading* specifically, where the aggregate meshing cost
across every worker thread directly gates how long the loading screen lasts. Left as an explicitly
named, deferred goal (see `docs/goals.md` Group V) — a bigger, riskier change (the actual
per-voxel-face AO-sampling hot loop) than this pass's scope, and one that deserves its own
dedicated, measured pass rather than a rushed addition here.

## Real, measured result: Debug vs. RelWithDebInfo (the direct "launching via the app" fix)

Same small world (radius 10: 2,646 real chunks, 4,232 incl. halo), same machine, both configs
freshly rebuilt with this pass's fixes already in place — isolating the build-config effect from
the algorithmic fixes above (both configs already have Findings 1+2 applied):

- **Debug** (`windows-debug`, the config Visual Studio's own F5 was defaulting to): **32.1s**.
  Breakdown: generation 88.88s CPU-time/4,232 chunks (21.003ms/chunk), meshing 258.33s
  CPU-time/2,646 chunks (97.630ms/chunk).
- **RelWithDebInfo** (`windows-relwithdebinfo`, this pass's new preset): **9.4s** — a **3.4x
  reduction** for the identical world. Breakdown: generation 6.48s CPU-time/4,232 chunks
  (1.532ms/chunk — a ~13.7x per-chunk improvement over Debug), meshing 79.24s CPU-time/2,646
  chunks (29.946ms/chunk — a ~3.3x per-chunk improvement over Debug).

The generation-phase improvement (~13.7x) being so much larger than meshing's (~3.3x) is itself
consistent with the root-cause theory: generation's hot path (`ChunkVoxels::set()`/`promote()`)
touches `std::pmr::vector` element access repeatedly per voxel, while meshing's hot path
(`NeighborCache::sample()`) was already redesigned in an earlier pass specifically to use raw
array indexing instead of container lookups — exactly the kind of code MSVC's
`_ITERATOR_DEBUG_LEVEL=2` checked-iterator/container-proxy machinery taxes hardest. Neither number
matches Release exactly (RelWithDebInfo's meshing, 29.946ms/chunk, is still well above Release's
~7.49ms/chunk at the larger radius-48 world) — expected, since RelWithDebInfo trades some of
Release's own optimization budget (no LTCG, e.g.) for symbol quality; the fix this section
measures is specifically the Debug-CRT tax, not "make RelWithDebInfo identical to Release."

## Regression check

76/76 tests pass on the rebuilt Release binaries (0 failures) — no test needed changes, since
every fix preserved existing public call shapes (`extract_mesh(store, coord)` unchanged;
`ChunkVoxels`'s default incremental-promotion behavior unchanged; `reserve_bits` is new,
opt-in, and only called from `fill_terrain`).
