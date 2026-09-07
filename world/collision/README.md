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
- **`OctreeCollider` is what the svo path actually uses now** (goal 226/227, closing goal 173). It
  answers from the same `shared_ptr<const BrickTree>` the renderer marches, so the body cannot
  disagree with the pixels, and it has **no cache and therefore no edge to outrun** -- which was the
  "I clip through blocks" complaint's real cause. `TerrainCollider` above is kept as the mesh path's
  collider and as the tests' analytic second opinion.
- **A body that starts inside solid CLIMBS OUT** rather than moving unblocked. The old policy never
  trapped the player and never freed them either -- moving unblocked through solid ends every tick
  still inside, so one bad tick became a permanent state (measured: 761 inside-solid ticks, all 761
  of them that branch). It now probes upward in doubling steps to the body's own height, bisects,
  and lifts the least that works; only past that bound does the unblocked escape remain, and it
  still reports itself so the caller can count it.
- **The sub-step is derived from the body**, not a constant (`substep_for`): the sweep tests end
  positions only, so boxes a distance `d < extent` apart INTERSECT and their union has no gaps,
  making tunnelling impossible for an obstacle of any thickness. There is no "safe up to N m/s" --
  the count is unbounded, so speed buys sub-steps rather than risk.

## Files

| file | what |
|---|---|
| `solid_query.hpp` | `Aabb`, the `SolidQuery` concept |
| `aabb_sweep.hpp` | `move_and_slide` (y, then x, then z; ledge step-up) |
| `terrain_collider.hpp` | the analytic world as a query: cached height grid + trunk boxes |
| `octree_collider.hpp` | the SVO world as a query: an O(depth) walk of the tree the renderer marches |
