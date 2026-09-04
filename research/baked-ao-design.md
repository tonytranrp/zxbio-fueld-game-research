# Baked voxel AO — design note (goals.md goal 10, written before touching mesh_extractor.cpp)

## The researched technique, and why it can't be copied verbatim

The reference scheme (0fps.net "Ambient occlusion for Minecraft-like worlds"; thenumb.at's Exile
note) is defined for **flat-shaded axis-aligned cube quads**: each of a quad's 4 corners tests its
3 adjacent solid/air neighbors (side1, side2, corner) and maps occluder-count {0,1,2,3} to one of 4
AO levels, stored **per-face-corner, not per shared vertex**, because a cube-mesh vertex is shared
by faces of *different orientations* whose occluder sets differ — sharing one value across them
bleeds darkness around corners.

This mesher is **Naive Surface Nets** (`mesh_extractor.cpp`): one vertex per active cell, placed at
the mean of its edge crossings, shared via the index buffer across every quad that references its
cell, with smooth area-weighted vertex normals. There are no per-face corners to store — the
per-face-corner subtlety is an artifact of flat-quad topology that this topology does not have: a
surface-nets vertex represents one point on a smooth surface, and interpolating a single AO value
across its incident triangles is exactly as legitimate as interpolating its single smooth normal.
So the honest adaptation is **per-vertex (per-cell) AO**, not a forced per-face-corner encoding
that would require abandoning index sharing and re-deriving the 12-byte vertex contract.

## The adapted scheme (v1)

AO source = the cell's own 8-corner solidity count `s`, already computed by `CellSample::compute`
for the surface-crossing test — **zero additional voxel samples**, and automatically cross-chunk
correct because those corners already go through `NeighborCache`'s padded sampling.

`s` measures local enclosure of the vertex:

| s (solid corners of the cell) | geometry it means            | AO level |
|-------------------------------|------------------------------|----------|
| 1–3                           | convex (peak, ridge, edge)   | 1.00     |
| 4                             | flat ground / plain wall     | 1.00     |
| 5                             | shallow crease               | 0.85     |
| 6                             | deep crease / valley corner  | 0.70     |
| 7                             | pit / concave corner         | 0.55     |

Mapping rule: `ao = 1 - max(0, s - 4) * 0.15`. Four discrete levels, mirroring the 0fps scheme's
4-level quantization; the GPU interpolates between vertices across each triangle exactly as the
0fps scheme interpolates its corner values across a quad. Flat ground **must** map to 1.0 (a
naive linear map over s∈[1,7] would uniformly dim all flat terrain by ~22% — that is global
dimming, not occlusion), so only the concave half of the range darkens.

Known limitation, accepted for v1: this is 1-voxel-scale concavity — wide-bowl valleys won't
darken, only creases/corners/pits. If the viewed dump (goal 13's check) shows the effect is too
subtle, the escalation path is a 4×4×4 outer-shell solid count through the same `NeighborCache`
(56 extra samples/vertex, still cheap) — not a screen-space pass; that question is Group F's.

## Storage (goal 11)

- CPU `world::meshing::Vertex`: new `float ao` (1.0 default).
- GPU `GpuVertexCompressed`: the existing **pad byte at offset 11** becomes `ao` as UNORM8 —
  layout stays exactly 12 bytes; the frozen `static_assert`s are extended (deliberately, per the
  goal) with `offsetof(..., ao) == 11`, not loosened. New `ATTRIB3` = 1×VT_UINT8 normalized.
- Shader: VS passes AO through; PS multiplies it into the final lit color (goal 13), composing
  with the hemisphere ambient (goal 14) rather than replacing it.

## Test (goal 12)

Hand-constructed arrangements in a unit test, asserting the mapping table above at exact cells:
a flat floor cell (s=4 → 1.0), a two-wall inside crease (s=6 → 0.70), and a three-wall concave
pit corner (s=7 → 0.55, darkest of the levels), plus monotonicity (pit < crease < flat).
