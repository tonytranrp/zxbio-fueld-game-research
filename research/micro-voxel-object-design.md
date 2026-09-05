# Micro-Voxel Decorative Object Design (Group R, goal 124)

Design record for `research/voxel-representation-redesign.md` §2.3's own item: small decorative
objects (flowers, berries, small rocks) modeled as a second, finer voxel grid local to each
object, greedy-meshed the same way terrain is — the actual mechanism that produces image 2's
individually-recognizable berry cubes, as opposed to shrinking the whole world's voxel size.

## Format: MicroGrid

A fixed-size cube of `MaterialID`, e.g. `kMicroGridSize = 8` per axis (512 cells) — small enough to
cost nothing, large enough to read as a genuine cluster of cubes rather than one oversized voxel.

Critically, this is **never stored per-instance**. A persistent 512-byte buffer per decorative
object would scale badly (thousands of objects across a static world), and it would also duplicate
`tree_decoration.cpp`'s own established philosophy: nothing about a tree is stored either — every
tree's shape/size/jitter is *derived* from `placement_key` at mesh time and thrown away
immediately after. A `MicroGrid` is the same: a transient, stack-allocated buffer filled by a pure
procedural function `generate_micro_grid(ObjectType, key) -> MicroGrid`, meshed, and discarded.

## Placement: a new pass, composing with trees rather than replacing them

Trees and micro-voxel ground objects are two independent decoration passes over the same chunk,
both appending into the same final mesh — exactly how trees already append after terrain. A future
chunk can carry both a forest canopy and a berry-studded floor at once.

The new pass gets its **own** file (`ground_object_decoration.hpp/.cpp`, name TBD at
implementation time) rather than folding into `tree_decoration.cpp`, because the two are
parametrically different in a way that would otherwise force one file to serve two masters:

- **Candidate density**: ground cover reads naturally denser than trees. Trees use an 8-voxel
  candidate cell (`kCell = 8`, 4×4 candidates per chunk); ground objects want something finer —
  likely a 4-voxel cell (8×8 candidates per chunk) — tuned once real output is viewed, not assumed.
- **Height/slope mask**: same shape of rule trees already use (reject water/beach, reject steep
  slopes) but its own thresholds — copying tree's exact constants would be coincidence, not design.

Both passes reuse the same `placement_key`-style deterministic hash `tree_decoration.cpp` already
established (seed, chunk, grid cell → a `uint64_t` key sliced into a jitter position, an object-type
selector, and per-instance variation bits) — worth factoring into a small shared utility both files
call, rather than a second hand-copied hash function drifting from the first over time.

## Meshing: Group Q's algorithm shape, not its chunk-scale machinery

A micro-object is fully self-contained — anything outside its own `kMicroGridSize`³ bounds is
always Air, with no cross-chunk halo to sample. Running the *real* `extract_mesh` (a full 32-layer,
6-sweep pass over `NeighborCache`/`ChunkStore`) against an object that only occupies an 8³ corner of
that space would pay a whole chunk's meshing cost — measured at 5-7ms per real chunk
(`benchmarks/baselines/2026-09-04-blocky-mesh.json`) — for a handful of triangles. Dozens of these
per chunk (a realistic ground-cover density) would multiply that into a real, avoidable cost.

The design is a **new, small, standalone mesher** scoped to a fixed small grid: the same
face-culling + greedy-merge shape and the same algebraically-verified winding derivation from
`mesh_extractor.cpp` (axisU=(axis+1)%3, axisV=(axis+2)%3's cyclic-basis property holds at any grid
size, not just 32), but sampling directly from the in-memory `MicroGrid` array with a trivial
bounds check (`out of [0,size) on any axis => Air`) instead of `NeighborCache`'s cross-chunk
resolution. Output is a small, self-contained vertex/index list in the object's own local space,
which the placement pass offsets into world/chunk-local position and appends into the chunk's real
mesh buffer — the same append pattern `tree_decoration.cpp`'s `push_octahedron`/`push_trunk`
helpers already use, just fed by a real mesher's output instead of hand-built primitive vertices.

Baked per-face-corner AO carries over unchanged in shape (SS2.2/goal 123's scheme already treats
"outside the sampled region" as Air-equivalent for edge cells, which is exactly a micro-grid's own
boundary condition) — no new AO design needed, just applied at a smaller scale.

Whether this new mesher shares literal code with `mesh_extractor.cpp`'s sweep (e.g. a shared
template parametrized on grid size + a sampler concept, per `templates-and-metaprogramming.md`'s
own guidance on generic algorithms over a small, compile-time-known shape question) or is a
lean, independent reimplementation of the same shape is an implementation-time call for goal 125,
not fixed here — the algorithmic *shape* is what this design commits to, not the exact code-sharing
mechanism.

## First concrete object type: a berry cluster

- A roughly-spherical blob of Leaves-material voxels filling the grid's center (radius ~2-3 cells
  of an 8-cell grid), with 3-6 individual bright berry voxels placed on the blob's outer surface at
  angle/radius offsets derived from the instance key (deterministic per placement, varied across
  instances the same way tree shape/jitter already are).
- Needs a genuinely new material, not a reused one — the berries are the entire visual point of
  this object (matching image 2's read directly), and goal 93's own precedent ("each new material
  ties to a real terrain/gameplay feature, not added speculatively") is satisfied here as directly
  as it gets. Proposed: `MaterialID::Berry = 8`, `kBlockTable` row `{color {0.75, 0.10, 0.15},
  is_solid=true, is_liquid=false, supports_growth=false, hardness=0.2}` (soft, decoration-only,
  matching Wood/Leaves' own precedent of `is_solid=true` for inert decoration geometry).

## Memory bound (goal 126) — how it will be checked, not assumed

The `MicroGrid` buffer itself is transient (stack-allocated during meshing, freed immediately after
— zero persistent cost). The only persistent cost is the resulting triangles in the chunk's own
mesh buffer, the same accounting trees already go through. The real, measurable question goal 126
asks is bounded by two countable things: the placement grid's candidate density per chunk (a known
constant once tuned) and the pre-mask acceptance rate (trees currently accept ~37% of candidates,
`(key & 0xFF) >= 96`) — both will be measured directly against real output once goal 125 lands, not
assumed safe from this design's own reasoning alone.

## Sequencing

124 (this document) → 125 (implement the berry cluster, view a dump, confirm it reads as
individually-recognizable small cube clusters per image 2) → 126 (measure the real memory/vertex
cost against a real chunk, same standard as every other cost claim in this project's history).
