# world/materials — materials as components

Every voxel material is one self-contained component file under `defs/` (`docs/goals.md` Group AC,
`research/materials-as-components.md`), composed into a compile-time registry in `materials.hpp`.
Header-only, no dependencies; the lowest layer of `world/`.

## Rules of this folder

- **One material, one file, all of its properties.** A def is a struct of `static constexpr`
  members satisfying the `MaterialDefinition` concept (`material_def.hpp`): display name, albedo,
  phase (gas / solid / liquid / foliage), shading model, liquid physics, the two tree-voxelization
  flags, and `fills()` — where the terrain bands place it. A property a consumer needs that is not
  there is a new member in `MaterialDef` and in every def, never a new `== MaterialID::X` at the
  consumer.
- **The registry order is the id.** `MaterialID`'s enumerators are DERIVED from
  `Registry::index_of<Def>()`; nothing elsewhere may hardcode a count, an index, or an order. The
  renderers size their palettes from `kMaterialCount` and pass `MATERIAL_COUNT` / `MAT_SHADING_*`
  to the shaders as macros (`render/diligent/detail/material_macros.hpp`), so no shader carries a
  literal id.
- **Air is component 0** (`RegistryOf` asserts it): the empty voxel of the brick masks, the node
  array, the chunk palette, and the shader miss path.
- **The band rule is a search, not a switch.** `terrain_material()` asks every component's
  `fills()`; the predicates must stay mutually exclusive and exhaustive — `test_registry.cpp`
  proves both over a grid and proves the result equals the two old hand-written rules
  (`fill_terrain`'s integer one and the sampler's meters one) byte for byte.
- **A material says WHICH shading model, a renderer owns HOW it looks.** The mesh path's
  depth-tinted water and the svo path's noise-rippled water were tuned by viewed captures and differ
  on purpose; they stay in the shaders.
- Ids are 8-bit everywhere (brick bytes, node headers bits 16..23, the 12-byte vertex); the
  registry refuses more than 256 components. `properties_of()` does not bounds-check — synthetic
  ids outside the registry (test_chunk_voxels.cpp's palette tests) must never reach it.

## Files

| file | what |
|---|---|
| `material_def.hpp` | `MaterialDef` record, `Phase`, `Shading`, `LiquidPhysics`, the `MaterialDefinition` concept |
| `terrain_query.hpp` | `TerrainBands` (the one copy of the band constants) and `TerrainQuery` |
| `registry.hpp` | `RegistryOf<Defs...>`: the table, `index_of`, the terrain search, the static checks |
| `materials.hpp` | the composition, `MaterialID`, `kMaterialCount`, `properties_of`, `name_of`, `is_occupied`, `terrain_material`, `tree_replaces` |
| `defs/*.hpp` | one component per material |

`world/chunk/material.hpp` re-exports `MaterialID` and `kMaterialCount` into `world::chunk` so the
rest of the tree keeps its spelling.
