# Micro-voxel pivot: sparse-brick octree + GPU ray marching (2026-09-05)

Decision log for docs/goals.md Groups W (CPU core), X (GPU renderer) and Y (measurements). The
request: "John Lin style sub-cm instead of blocks like currently right now", accompanied by a
research brief on micro-voxel data structures (SVO/SVDAG/HashDAG/bricked octrees, attribute
compression, GigaVoxels streaming, LOD). This file records what was built, what was decided
against, and every real number behind those decisions.

## 0. The stack mismatch in the brief, named up front

The brief was written against the deprecated Rust/Bevy/VoxelHex track ("keep VoxelHex", wgpu
`EXPERIMENTAL_RAY_QUERY`). This repository is the C++/DiligentEngine track. Every *technique* in
the brief transfers (the papers are stack-agnostic); every *library recommendation* does not. The
bricked-SVO architecture the brief recommends was therefore built here directly, in `world/svo`,
rather than adopted from any crate.

## 1. Why the representation had to change (not the shading)

The shipping world was 1 m voxels rendered as greedy-merged meshes. Mesh size scales with the
*square* of linear resolution; sub-centimeter voxels (1/128 m = 7.8 mm) are 128x finer per axis,
so the same world would need ~16,000x the triangles -- rasterization is asymptotically the wrong
tool at that scale (brief §1.2). John Lin's own renderer is a GPU ray marcher over a sparse
structure: cost scales with screen pixels, not voxel count. That is the whole pivot: **the world
became a sparse-brick octree, and the renderer became a fullscreen ray march.**

## 2. Design decisions

### 2.1 Structure: bricked SVO, SVDAG node layout, no DAG dedup (yet)

- 8x8x8 **bricks** at the leaves (brief §7.1's sweet spot): 512 material bytes + a 512-bit
  occupancy mask (64 B) = 576 B. The mask makes the marcher's per-step test touch 1-2 cache
  lines instead of 512 B, and answers "anything here" without reading materials.
- **Node words** in the SVDAG paper's layout (brief §3.3: 4-byte header + one 4-byte pointer per
  set child bit, 8-36 B per node), with the header's spare bits carrying a node KIND (internal /
  brick leaf / solid leaf) and a representative MATERIAL for LOD-cube shading. Homogeneous solid
  boxes are one word; air is absent entirely.
- **No DAG deduplication in this pass.** The brief's own numbers say why: dedup wins 26-576x on
  regular, tile-like content and 1.2x on irregular geometry (the Hairball case). Noise terrain at
  sub-cm is the irregular case -- surface bricks are essentially unique. Measured instead of
  assumed: see §4 for where memory actually goes (it is the surface bricks, which dedup would not
  touch). Interning-based dedup stays a goal with the brief's 16-tile unit test as its check.

### 2.2 LOD lives in the tree, and the tree is rebuilt around the camera

Sub-cm everywhere is impossible (a 256 m region at 7.8 mm on this terrain would be ~25M surface
bricks, >12 GB). The builder therefore stops subdividing a box once its brick voxel edge is
`<= max(finest, distance * finest / lod_radius)` -- full resolution within `lod_radius` (4 m),
halving per doubling of distance beyond, i.e. roughly constant screen-space voxel size, the same
criterion the Laine-Karras marcher applies at traversal time. Because a box's distance is its
nearest point, no box is ever coarser than any point inside it would ask for (tested).

The tree is **immutable and rebuilt whole** on a background thread whenever the camera leaves the
inner half of the finest ring (`SvoWorld`), then swapped in (new immutable GPU buffers; Diligent
defers the old ones' destruction). Whole-rebuild was chosen over incremental subtree reuse
deliberately: measure first. Measured rebuild cost at the shipping default (512 m region,
7.8 mm): **0.6-1.3 s on 16 threads, 28-60 ms upload** (§4). Incremental reuse is goal 158.

### 2.3 The sampler is the existing generator, generalized to meters

`TerrainSampler` uses the same `HeightmapGenerator` (FastNoise2, SSE2-pinned for determinism)
and the same banding rules as `fill_terrain`, with integer-voxel depths turned into meters
(surface: depth < voxelEdge; soil: depth < 3 m + voxelEdge; else stone; water where the bottom is
at or below sea level). The occupancy rule was kept **bit-for-bit compatible** with `fill_terrain`
(a voxel is solid iff its bottom face is at or below the column's surface, sampled at the voxel's
min corner) so that at 1 m the sampler reproduces the shipped chunks exactly --
`test_terrain_sampler.cpp` proves it over 117,600 voxels against `fill_terrain` itself. (Columns
with a negative surface height are skipped there: `fill_terrain` truncates toward zero instead of
flooring, so its underwater terrain sits one voxel higher -- a real, pre-existing quirk, goal 161.)

Trees became **implicit shapes**: placement moved from `app/` into
`world/generation/tree_placement`, and the trunk box + octahedron lobes are now one definition
the mesh emitter and the voxelizer both consume. At 8 mm a conifer is a real conifer.

### 2.4 Box classification: sound, then fast

The builder never checks a sampler's Air/Solid verdicts, so they must be *sound* (a wrong one is a
hole). `HeightField` is a min/max pyramid over corner-sampled surface heights with a per-cell
margin (half the cell's own corner range + a floor proportional to the cell size), verified by
dense re-sampling (24,000 random footprints, 0 violations, at both 0.5 m and 1/16 m cells).

Where the build time went, in order of what was found (all single-thread, 128 m region, 7.8 mm):
| change | single-thread build | why |
|---|---|---|
| first working version | 21.2 s | global margin (3.6 m on this terrain's cliffs) made everything within a 7 m band "Mixed"; 4 slope noise-grids per brick |
| per-cell local margins | -- | cut sampled bricks ~2x |
| slope from the height field's corners | 9.8 s | removed 4 of 5 FastNoise2 grid calls per surface brick; exact at integer columns so the `fill_terrain` equivalence still holds |
| tiered focus fields (1/16 m near, 1/8 m mid) | 9.7 s | tighter bands where the bricks are smallest |
| exact convex tree tests (not AABB) | 9.7 s | no measurable change -- trees were never the cost |
| per-level sampled/kept histogram | -- | the diagnostic that found the real cause below |
| **solid soil-band rule** | **4.5 s** | 800K of the 1.04M finest-level bricks sampled were homogeneous *Dirt*: the classifier could only prove "Stone" 3 m below the surface, so the whole 3 m soil band (48 brick layers at 6.25 cm) was "Mixed" |
| deeper parallel split (level 5, 32K jobs) | 16 threads: 12.4 s -> 2.0 s | with distance LOD nearly all work sits in the few subtrees around the camera; 512 jobs left ~4 running while the pool idled |

Two lessons worth keeping: (1) the two "obvious" costs (noise calls, trees) were each measured
and found irrelevant before the histogram found the real one; (2) a sound classification rule
that proves *which* homogeneous material fills a box (not just "solid, somewhere below") removed
more work than any micro-optimization.

### 2.5 Traversal: integer-cell stepping, mirrored CPU/GPU

`ray_trace.cpp` is the reference: stack-based octree descent where every cell transition forces
the exit axis's integer coordinate exactly and only derives the other axes from the ray position
(clamped into the exited cell), then pops to the deepest common ancestor via the XOR bit-width of
the old/new cell coordinates; Amanatides-Woo DDA inside bricks; Laine-Karras early-out when a
child's edge projects under a pixel. Validated by a brute-force finest-voxel DDA oracle over 7,000
random rays (uniform and mixed-LOD trees, origins inside and outside, axis-aligned-plane rays):
0 mismatches. `svo_march.psh.hlsl` is that function ported statement for statement; the D3D12
(FXC) path additionally forbids writing a runtime-indexed vector component, so every `v[axis] =`
became a masked vector write.

### 2.6 Renderer integration

One fullscreen pass writes `SV_Depth` from the hit position (sky at far depth on miss), so the
existing RGBA16F -> bloom -> tonemap chain and the ImGui overlay compose unchanged. Shading is the
terrain pass's model plus what a marcher gets cheaply: a traced sun-shadow ray and 4 short
hemisphere AO rays per hit. Both are per-frame switchable (`--no-shadows`, `--no-ao`).

One real bug found by the CPU reference, not by staring at code: the first GPU frame was sky
only. `tools/svo_render` at the same pose showed terrain, which pointed at the *data binding*, not
the traversal: Diligent's MUTABLE SRB variables accept a resource once per SRB, so the re-bind at
upload silently kept the 1-word placeholder buffers. DYNAMIC fixed it.

### 2.7 Decided against, in writing

- **HashDAG / editable DAG** (brief §2.2): the right answer for live editing; this pass ships a
  rebuilt-from-sampler world with no editing at all, so it buys nothing yet. Named as the path
  when editing arrives (goal 160).
- **Hardware ray queries**: the brief's hybrid conclusion holds on Diligent too; compute/pixel
  traversal is portable, benchmarked, and already vsync-bound at 720p.
- **4-bit / per-brick palette materials**: 2-4x memory, real, deferred with a number (goal 157) --
  the 8-bit tree fits the 8 GB card by a wide margin at the shipping default.
- **Temporal AA / supersampling**: sub-pixel voxels 2-8 m away shimmer (visible moire in every
  capture). Real, John Lin's own renderer needs TAA for the same reason; goal 159.

## 3. Verification

- 101/101 tests (was 76): brick/layout/height-field/builder/trace/sampler suites plus
  `svo_render_smoke` (a CPU frame of real terrain under the app's own local-contrast metric, in
  the GPU-less CI jobs).
- `--verify-frame` on the svo path: **48.0%** local contrast on Vulkan AND D3D12 (mesh path:
  23-30%); dumps viewed, identical between backends.
- `--walk --autofly --frames 600`: 0 ground violations, 3 background rebuilds in flight during
  the run, worst frame 61 ms (the upload hitch).

## 4. Real numbers (RTX 4070 Laptop, 1280x720, Release)

| measurement | value |
|---|---|
| world ready (svo default: 512 m, 7.8 mm, trees) | **0.56 s** (mesh path: 29.9 s) |
| tree at the default spawn pose | 338,602 bricks, 203 MB, upload 35 ms |
| tree at a hilltop pose (more surface in range) | 657,034 bricks, 395 MB, build 1.30 s, upload 61 ms |
| bricks per level, 256 m root (CPU tool) | L7 0.25 m: 44K; L8: 180K; L9: 125K; L10: 110K; L11: 147K; L12 7.8 mm: 143K |
| fps, panoramic pose, shadows+AO | 155-159 (165 Hz vsync cap) |
| fps, ground level on a hilltop, shadows+AO | 76 |
| background rebuild (walk) | 0.57-0.68 s, 28-42 ms upload |
| CPU reference render (tool), 1280x720, 16 threads | 3.1 Mrays/s with shadow rays |

Memory model check against the brief's §7.1 worksheet: ~600K bricks x 576 B ~= 350 MB + ~20 MB of
nodes at the finest-ring radius of 4 m. The brief's estimate of ~400K bricks at 230 MB assumed
~1.5 bricks per surface column; this terrain's mean slope (~1.3) makes it ~2.5-3, which is the
whole difference.
