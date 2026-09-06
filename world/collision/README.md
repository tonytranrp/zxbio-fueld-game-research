# world/collision — body-vs-world collision

The camera's physical presence (`docs/goals.md` Group AA): an upright axis-aligned body swept
through a world that answers one question, "does this box overlap anything solid?".

## Rules of this folder

- **The world is a concept, not a class.** `solid_query.hpp`'s `SolidQuery` is the whole contract;
  `aabb_sweep.hpp` is a template over it. Tests use a plane-and-wall fake; the app uses
  `TerrainCollider`; a query over the sparse-brick octree slots in later without touching the sweep.
- **The sweep is axis-separated and bisected**, never analytic: it asks the query, it does not
  reason about its geometry. That is what keeps it correct for any query shape (heightfield, boxes,
  voxels) at the cost of ~40 box tests per frame -- cheap against a cached height grid.
- **`TerrainCollider` agrees with what is drawn.** It applies the sparse tree's own voxelization
  rule (a voxel is solid iff its bottom is at or below the surface height at its min corner) to the
  same height function and the same deterministic tree placements, over a fine local cache. It does
  not read the rendered tree, so it never depends on the renderer's current LOD or rebuild lag.
- A body that starts inside solid is never trapped: it moves unblocked until free.

## Files

| file | what |
|---|---|
| `solid_query.hpp` | `Aabb`, the `SolidQuery` concept |
| `aabb_sweep.hpp` | `move_and_slide` (y, then x, then z; ledge step-up) |
| `terrain_collider.hpp` | the analytic world as a query: cached height grid + trunk boxes |
