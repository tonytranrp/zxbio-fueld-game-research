# world/svo — sparse-brick octree (micro-voxel representation)

The resolution-independent world representation behind the micro-voxel pivot
(`docs/goals.md` Group W, `research/micro-voxel-pivot-log.md`): an octree over a power-of-two
world region whose leaves are 8×8×8 **bricks** of material bytes (+ a 512-bit occupancy mask),
with homogeneous boxes collapsed to single-word solid leaves and air boxes absent entirely.
Distance-based LOD lives *in the tree* — brick resolution halves with every doubling of distance
from the build center — which is what makes a 512 m region at 7.8 mm voxels near the camera fit
in a few hundred MB instead of exabytes.

## Rules of this folder

- **Two flat `uint32` arrays are the whole format.** `BrickTree::nodes` and `::bricks` are uploaded
  to the GPU verbatim; `tree_layout.hpp` (node words) and `brick.hpp` (brick words) are the
  contract, and `render/diligent/shaders/svo_march.psh.hlsl` reads them bit-for-bit. Change a
  layout only together with the shader and `ray_trace.cpp`. Layout v2 (Group Z): every internal
  node and brick leaf carries one **attribute word** after its header — the area-weighted average
  normal of its exposed faces (int8 x3) and its volume coverage (uint8), built bottom-up in the
  same pass as the tree — and the child slots follow it.
- **`ray_trace.cpp` is the reference marcher.** The HLSL mirrors it statement for statement. A
  traversal change lands here first, passes `test_ray_trace.cpp`'s brute-force oracle, and is only
  then ported. `TraceParams` is the whole contract for secondary rays: they judge LOD by distance
  from their OWN origin (`t_offset` stays 0 — judging from the eye self-hit the node containing the
  origin, the shadow-ring bug), and descend nodes under `lod_coverage_threshold` (35% for
  shadow/AO, 0 for primary rays so silhouettes stay closed) instead of hitting them.
- **Samplers are sound or they are wrong.** `VoxelSampler::classify` may answer Air/Solid only
  when every voxel of the box at any voxel size would sample that way; the builder never checks.
  `TerrainSampler` proves its rules against dense sampling in `test_terrain_sampler.cpp`.
- **The tree is immutable after `build_tree`.** Camera movement rebuilds it (in the background,
  double-buffered); editing is the HashDAG-shaped follow-up, deliberately not this structure.
- `detail/` holds the `build_tree` template body. It is instantiated for `TerrainSampler` in
  `src/tree_builder.cpp`; tests instantiate it for their analytic samplers.

## Files

| file | what |
|---|---|
| `brick.hpp` | 8³ brick word layout + `Brick` value type |
| `tree_layout.hpp` | node header encoding, `TreeGeometry` (region/level math) |
| `brick_tree.hpp` | the built tree (flat arrays + point queries + stats) |
| `sampler.hpp` | `Box`, `BoxClassification`, the `VoxelSampler` concept |
| `height_field.hpp` | conservative min/max surface-height pyramid + 1 m slope |
| `terrain_sampler.hpp` | the real world as a sampler (heightmap banding + implicit trees) |
| `tree_builder.hpp` | `build_tree` (parallel, LOD-aware) |
| `ray_trace.hpp` | CPU reference marcher + brute-force oracle |

`tools/svo_render` renders whole frames with the reference marcher (the GPU-less CI proof and the
GPU diff baseline); `--lod-center x,y,z` builds the LOD around a point other than the eye (the
deterministic "camera moved since the last rebuild" reproduction) and `--view NAME` renders one
shading term (`lit|ao|normal|facenormal|level|steps|coverage|cubepx|smooth|lodcube|material|distance`,
the same names as the app's `--debug-view`).
