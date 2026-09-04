# M1.2 — World Generation: Paletted Chunk Storage & FastNoise2 Terrain Fill

Companion to [`PROJECT_BRIEF.md`](PROJECT_BRIEF.md) §5/§8 and [`PHASE_1_BRIEF.md`](PHASE_1_BRIEF.md)'s
M1.2 line. This document supersedes both on chunk storage specifically — see §9 for the exact patch.

**Status acknowledged, not re-litigated**: M1.1's real fix (the `ThreadPool` destructor-order UB,
member-declaration-order corrected, clean under 5 repeated ASan runs) matters here directly —
`PROJECT_BRIEF.md` §10 already commits M1.2 to verifying noise determinism *across threads*, so M1.2's
own test suite is about to become a second, independent stress test of that fix, on top of the
generation-correctness job it's actually for. Worth taking those specific tests seriously for that
reason, not just as world-gen tests. The Radient finding (a whole-scene framework, no culling of its
own) doesn't touch M1.2 — noted for continuity, not acted on here.

Table of contents: [1. Resolving "flat array" vs. "paletted"](#1-resolving-flat-array-vs-paletted) ·
[2. FastNoise2, concretely](#2-fastnoise2-concretely) ·
[3. Coordinate math](#3-coordinate-math) ·
[4. The fill algorithm](#4-the-fill-algorithm) ·
[5. ECS integration boundary](#5-ecs-integration-boundary) ·
[6. Testing plan](#6-testing-plan) ·
[7. Folder/file additions](#7-folderfile-additions) ·
[8. Explicitly not decided here](#8-explicitly-not-decided-here) ·
[9. Patch note](#9-patch-note)

---

## 1. Resolving "flat array" vs. "paletted"

**The inconsistency, named plainly rather than quietly picked around**: `PROJECT_BRIEF.md` §5
specified a chunk as a flat `std::vector<uint8_t>` of material IDs. `PHASE_1_BRIEF.md`'s M1.2 line
called the same thing "paletted storage" without ever specifying a palette scheme. Those aren't the
same design, and the gap needed resolving with an actual scheme, not another restatement of the word
"paletted."

**The resolution**: a real palette scheme, where the flat-byte-array design in §5 turns out to be the
*ceiling* case of the same scheme, not a contradiction of it — nothing in §5 was wrong, it just
described the worst case.

### 1.1 The scheme

A chunk (`32³` = 32,768 voxels, unchanged) stores:

- A small `palette`: a `std::pmr::vector<MaterialID>` of the *distinct* material IDs actually present
  in this chunk — typically single digits (Air, Stone, Dirt, Water is a plausible full v1 set).
- A bit-packed `indices` buffer: one index per voxel into `palette`, at the *minimum* bit width the
  current palette size needs — `bits = max(0, ceil(log2(palette.size())))`.
- **The homogeneous fast path, which is the whole reason this is worth doing rather than always using
  the flat array**: when `palette.size() == 1`, `bits == 0` — there is no `indices` buffer at all,
  just the one `MaterialID`. This is not an edge case to handle defensively; for real terrain it's
  the *common* case — every chunk entirely above the generated surface is pure air, and (with no
  caves in v1's scope) every chunk sufficiently far below the surface is pure stone. Only chunks
  actually straddling the surface, or the water level, carry a real multi-entry palette.

```cpp
class ChunkVoxels {
public:
    // Reads are branch-once, not "always decode a bit-packed array":
    MaterialID at(std::size_t localIndex) const {
        if (palette_.size() == 1) return palette_[0];
        return palette_[read_packed(indices_, localIndex, bits_)];
    }
private:
    std::pmr::vector<MaterialID> palette_;
    std::pmr::vector<std::byte>  indices_;   // empty when palette_.size() == 1
    std::uint8_t                 bits_ = 0;
};
```

### 1.2 Memory numbers, cross-checked two ways

`palette.size()` and its bit width move together in the obvious table:

| Distinct materials | Bits/voxel | Bytes/chunk (32,768 × bits ÷ 8) | vs. flat `uint8_t` (32,768 B) |
|---|---|---|---|
| 1 (homogeneous) | 0 | 0 (+ a few bytes for the single `MaterialID`) | ~4,000× smaller |
| 2 | 1 | 4,096 | 8× smaller |
| 3–4 | 2 | 8,192 | 4× smaller |
| 5–16 | 4 | 16,384 | 2× smaller |
| 17–256 | 8 | 32,768 | **identical** — this row *is* `PROJECT_BRIEF.md` §5's flat array |

**First derivation** (direct): `bytes = voxel_count × bits ÷ 8 = 32768 × bits ÷ 8 = 4096 × bits`.
**Second, independent cross-check** (ratio-based): `bits` bits is `bits/8` of a full byte, so the
chunk should be that same fraction of the 32,768-byte flat-array baseline — 4 bits is 50% → 16,384 B
✓; 2 bits is 25% → 8,192 B ✓; 1 bit is 12.5% → 4,096 B ✓. Both methods agree at every row. The 8-bit
row landing exactly on the flat-array size is the algebraic proof that §5's array was the
`palette.size() > 16` tier of this same scheme all along.

### 1.3 Promotion — the part that's actually correctness-critical

Growing the palette (a new distinct material appears in a chunk that's about to exceed its current bit
width) has to re-pack every existing index at the new width in the same pass — a bug here silently
corrupts already-written voxel data, not something a quick visual check would catch. `world/chunk`'s
own unit tests (§6) test this directly: build a chunk through every promotion boundary (1→2, 2→3,
4→5, 16→17 distinct materials) and assert every previously-set voxel's *value* (not just that the
palette grew) survives each promotion unchanged.

---

## 2. FastNoise2, concretely

`PROJECT_BRIEF.md` §2.5 already picked FastNoise2 and cited its benchmarks — this section is the part
that was missing: how it's actually called.

### 2.1 Building the node tree in code, not from an encoded string

Confirmed directly from FastNoise2's own API (its wiki and the Rust binding that wraps the same C++
surface 1:1): two legitimate construction paths exist. `FastNoise::New<FastNoise::Simplex>()` — a
templated factory returning a reference-counted node handle — builds a tree directly in C++.
`FastNoise::NewFromEncodedNodeTree(encodedString)` deserializes a tree exported from the visual
NoiseTool GUI. **Building the tree in code is the right default here**: an encoded string is an opaque
blob in source control — unreviewable in a diff, and it hides exactly the parameters (frequency,
octaves, seed offsets) this project will spend the most time tuning. The NoiseTool is still worth
having installed for *visualizing* a candidate node graph interactively before committing it to code,
just not as the actual shipped configuration mechanism.

### 2.2 Noise type for terrain: Simplex over Perlin, with the real tradeoff

FastNoise2's own documentation states the actual tradeoff rather than leaving it to guesswork: Perlin
noise can show grid-aligned artifacts at 45°/90° angles (it samples a square/cubic grid) and is faster
in 2D; Simplex is more isotropic (fewer directional artifacts, its grid is triangular/tetrahedral) and
samples fewer vertices per point in 2D. Given "highly detailed... land, mountains" is explicitly about
visual quality, not raw throughput, and given noise generation is a per-chunk-load cost (once, not
per frame — the same "does this run once or in a hot loop" test `release-codegen-and-tradeoffs.md` §9
already established), **Simplex is the right default base noise**, not Perlin — the throughput
difference genuinely doesn't matter here, the artifact difference does.

### 2.3 Fractal composition for "mountains"

"Mountains" is amplitude, not a separate system, exactly as `PROJECT_BRIEF.md` §8 already framed it —
FastNoise2's fractal wrapper node (an FBm-style node composing a base generator across multiple
octaves with a lacunarity/gain pair) is the concrete mechanism: a `FastNoise::FractalFBm` node wrapping
the `Simplex` source from §2.2, 3–5 octaves as a reasonable starting range (more octaves adds
high-frequency detail at diminishing visual return and real generation-cost — worth exposing as a
tunable constant, not hardcoding a specific count as gospel). Higher lacunarity and gain push more
weight into higher-frequency octaves, which is the actual "how mountainous" knob.

### 2.4 `GenUniformGrid` and the free homogeneous-chunk detection

Confirmed API shape (directly, via FastNoise2's own binding surface): the grid-generation call
(`GenUniformGrid2D`/`3D`) takes an output buffer, an offset, a count, a step size, and a seed, and
**returns the min/max value found across the whole generated grid** — not just the buffer contents.
This is a genuine, free optimization worth using rather than reimplementing: after generating a
chunk-column's heightmap (or, if a chunk later needs true 3D density, its raw noise grid), checking
whether that returned min/max straddles the surface threshold tells you *before* touching a single
voxel whether the chunk can go straight to §1.1's homogeneous fast path (min/max both above the
surface → pure air; both below → pure stone) — skipping the whole per-voxel fill loop, not just
compressing its output afterward.

### 2.5 The determinism question — what's confirmed, what isn't, and the mitigation

This is the one place this pass couldn't find a documented guarantee either way, and it's said that
plainly rather than assumed safe. **Confirmed**: FastNoise2 compiles each SIMD level
(Scalar/SSE2/SSE4.1/AVX2/AVX512/NEON) as a genuinely separate code path via its `FastSIMD` dispatch
layer, and selects among them *at runtime* based on the running CPU's actual capability. **Not
confirmed by this pass, in either direction**: whether two different SIMD levels are guaranteed to
produce bit-identical floating-point output for the same logical computation. Reasoned from what's
independently well-established about floating point rather than left as a bare unknown: different ISA
extensions frequently expose different instructions for logically-equivalent operations — FMA
(fused multiply-add, available from AVX2 onward on x86, absent on SSE2) computes `a*b+c` with a single
rounding step instead of two separate roundings, which can change the result in the last bit or two
even for mathematically identical code. That's a real, known failure class
(`SKILL.md`'s own "check formal claims against known failure classes: numerical instability" rule,
applied to a library-usage question rather than a proof), not a hypothetical one.

**Why this matters concretely, not just in principle**: "same seed reproduces the same world" is a
real, implicit product expectation for a procedural sandbox — sharing a seed, or reloading a save,
silently producing a *different* world because it happened to generate on a different CPU (or even
run through a different code path due to how the runtime dispatch resolves) would be a surprising,
hard-to-explain bug, not a cosmetic one. **The mitigation, concrete and cheap given generation is a
run-once-per-chunk cost, not a hot loop**: pin world generation to one explicit SIMD level (FastSIMD
exposes level selection, not just auto-dispatch-to-fastest) rather than trusting cross-level bit-
identical output. Paying for the lowest-common-denominator level's throughput on a cost that's already
amortized per chunk load is a trade worth making deliberately, not one to discover the value of after
a bug report about a seed producing different terrain on two machines. This is exactly what M1.2's
cross-thread/cross-run determinism tests (§6) need to actually exercise — the pinned level, not
whatever the auto-dispatcher happens to pick on the test machine.

---

## 3. Coordinate math

**The gotcha**: converting a world-space voxel coordinate into a `(chunk coordinate, local-voxel
offset)` pair via naive `/` and `%` is wrong the moment either coordinate goes negative — C++'s
integer division truncates toward zero, so for `X = -1` and chunk size 32, `X / 32 == 0` (not `-1`)
and `X % 32 == -1` (not `31`). A bidirectional world (chunks extending negative *and* positive from
the origin, the standard genre convention and the reasonable assumption here absent a stated reason
to restrict it) hits this immediately, not as an edge case reached only at extreme coordinates.

**Why chunk size staying `32` (already the existing default, per `PROJECT_BRIEF.md` §5) is worth
keeping specifically because it's a power of two**: C++20 standardized signed integers as two's
complement representation and, as a direct consequence, made right-shift of a negative signed integer
well-defined as sign-propagating (arithmetic) shift — both previously implementation-defined. For a
power-of-two chunk size, that turns the general floor-division problem above into a shift and a mask,
correct for negative coordinates by construction, not by a branch:

```cpp
constexpr std::int32_t kChunkSize  = 32;
constexpr std::int32_t kChunkShift = 5;   // log2(32)
constexpr std::int32_t kChunkMask  = kChunkSize - 1;   // 0b11111
std::int32_t chunkCoord = worldVoxelCoord >> kChunkShift;   // correct floor division, X=-1 -> -1
std::int32_t localCoord = worldVoxelCoord &  kChunkMask;    // correct wrap into [0,31], X=-1 -> 31
```

This isn't a new argument for `32` over some other size — it's a new, independent reason the existing
choice is a good one, worth stating since it wasn't part of the original justification.

---

## 4. The fill algorithm

Concretely, not just named, and deliberately **not** storing a continuous density value alongside the
material ID — reasoned from what M1.3's meshing actually needs, not decided in isolation:

1. For each `(worldX, worldZ)` column a chunk covers, sample the §2.3 fractal-Simplex heightmap once
   (not once per voxel — height only varies in X/Z, not Y) to get `surfaceHeight`.
2. Per voxel at `worldY`: `Air` if `worldY > surfaceHeight`; else `Stone` (or a thin `Dirt` cap near
   the surface, a one-line refinement, not a separate system); separately, `Water` overrides `Air`
   wherever `worldY <= seaLevel` (a fixed constant) *and* the cell would otherwise be `Air` — water
   never overrides solid ground.
3. §2.4's min/max short-circuit runs *before* step 2's per-voxel loop for the whole chunk, not per
   column.

**Why no stored density**: Naive Surface Nets needs to know, per edge between two voxels, whether it
crosses the surface (one side solid, one side not) and roughly where along that edge to place a
vertex. A discrete occupancy signal — read directly off material IDs (`material != Air`) — is
sufficient for that; the interpolated crossing position can be estimated from the two endpoints'
occupancy alone (the standard simplified/naive-surface-nets approach) without a stored per-voxel
float. Storing one would also directly defeat §1's whole palette scheme, which only compresses
*discrete* values — a `float` per voxel is 32 distinct-looking bits almost everywhere, uncompressible
by a palette by construction. If M1.3 later finds mesh quality genuinely needs sub-voxel precision a
discrete signal can't give, that's a real, specific reason to revisit this — not a reason to
speculatively store density now against a problem that hasn't been shown to exist yet.

---

## 5. ECS integration boundary

`PROJECT_BRIEF.md` §2.4 justified EnTT partly by "spin up an entity per loading chunk, attach/detach
state components as generation/meshing/upload stages complete" — this milestone is where that becomes
concrete rather than a justification for a library pick:

- **A chunk's voxel data (§1) is plain-owned data**, held in `world/chunk`'s coordinate-keyed
  `std::unique_ptr<Chunk>` map — unchanged from `PROJECT_BRIEF.md` §5. It is **not** EnTT component
  data; EnTT's sparse-set storage is built for many small, homogeneous, cache-friendly components, not
  one variable-sized paletted blob per entity.
- **A chunk's *pipeline state* is an EnTT entity**, carrying one small component —
  `ChunkLoadState { Requested, Generating, Generated, Meshing, Ready }` (an enum, a genuinely
  small, homogeneous, sparse-set-friendly component) plus a raw, non-owning pointer/handle back to the
  real `Chunk` the coordinate-keyed map owns. Systems query `ChunkLoadState == Requested` to know what
  to enqueue next, exactly the pattern the original justification described, now given an actual shape
  instead of staying a sentence.

---

## 6. Testing plan

Updates `PROJECT_BRIEF.md` §10's "noise determinism... across runs and across threads" and Surface-Nets
boundary-case list for what's now concrete:

- **Palette promotion correctness** (§1.3): every promotion boundary, previously-set voxel values
  survive unchanged — the test that would have caught a re-packing bug.
- **Homogeneous fast path**: a chunk entirely above the generated surface has `palette.size() == 1`
  and zero-length `indices`; assert this directly, not just "the chunk looks empty."
- **Determinism, same-thread**: same seed + chunk coordinate, generated twice sequentially, byte-
  identical `Chunk` output.
- **Determinism, cross-thread**: the same chunk coordinate generated concurrently from multiple
  `ThreadPool` (M1.1) worker threads, all results byte-identical to each other and to the
  same-thread case — the test that's simultaneously exercising the M1.1 destructor-order fix under
  real concurrent load, per this document's opening note.
- **Coordinate math**: negative chunk/world coordinates specifically (§3) — a voxel at
  `worldX = -1` resolves to `chunkCoord = -1`, `localCoord = 31`, asserted directly, not inferred from
  "the terrain looked continuous across the origin."
- **Boundary content**: an all-air chunk, an all-stone chunk, a chunk straddling the generated
  surface, and a chunk straddling sea level specifically — four distinct palette shapes, each
  asserted, not just "some chunk somewhere has water in it."

---

## 7. Folder/file additions

None beyond what `PHASE_1_BRIEF.md` §7 already scaffolded (`world/chunk`, `world/generation` were
already stubbed in Phase 0 and named in that tree) — this milestone fills in real implementations
inside the existing structure, it doesn't add new modules.

---

## 8. Explicitly not decided here

The actual Surface Nets implementation, vertex/normal generation, and chunk-boundary stitching are
M1.3's work, not this milestone's — §4 above only goes as far as defining the *interface* M1.3
consumes (paletted material IDs, no stored density, occupancy read directly off material identity) and
deliberately stops there rather than reaching into the next milestone's design.

---

## 9. Patch note

Replace `PHASE_1_BRIEF.md`'s M1.2 line —
> `world/chunk` (paletted storage, pmr-pooled, per `PROJECT_BRIEF.md` §5) and `world/generation`...
— with:
```
**M1.2 — World generation.** Superseded by M1_2_BRIEF.md — see that document for the real
paletted-storage scheme (§1), concrete FastNoise2 usage including the determinism mitigation
(§2), coordinate math (§3), the fill algorithm (§4), and the ECS integration boundary (§5).
```

And in `PROJECT_BRIEF.md` §5, append a forward-reference rather than editing the existing flat-array
description (it's still correct, just now understood as one tier of a larger scheme):
```
See M1_2_BRIEF.md §1 for the full palette scheme this flat-array description turns out to be
the ceiling case of.
```

---

## Sources

FastNoise2's own repository, wiki (`Understanding-Noise-Types/FAQ` for the Perlin/Simplex/SuperSimplex
tradeoffs, quoted accurately not verbatim), and the `fastnoise2` Rust crate's documentation (a 1:1
wrapper over the same C++ API surface, used here to confirm the real `New<T>()`/
`NewFromEncodedNodeTree`/`GenUniformGrid2D` call shapes and the `OutputMinMax` return value) for
everything in §2. C++20's two's-complement/arithmetic-right-shift standardization (stable,
already-finalized language rules, not independently re-searched this pass since it doesn't change) for
§3.

**In-repo**: `PROJECT_BRIEF.md` §5 (memory/ownership, reconciled in §1 above), §8 (world generation,
made concrete in §2 and §4), §10 (testing, extended in §6); `PHASE_1_BRIEF.md` §7 (folder structure,
unchanged per §7 above); `release-codegen-and-tradeoffs.md` §9 (the run-once-vs-hot-loop heuristic,
applied to the noise-type and SIMD-pinning tradeoffs in §2.2 and §2.5); and `SKILL.md`'s own
"check formal claims against known failure classes" rule, applied in §2.5 to a library-determinism
question rather than a proof.
