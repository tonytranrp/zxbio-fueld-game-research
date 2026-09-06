# Materials as components (Group AC, 2026-09-06) — decision log

The user's structural ask from the same round as the Lin-look pass (`research/lin-look-log.md`,
quoted there as its own group): "blocks and block types or colors are not components in their own
files but hardcoded offsets" — they want one self-contained file per material composed into a
compile-time registry, not parallel tables and switch statements kept in sync by comment. This
supersedes goal 113's written decision ("a constexpr table, not a runtime registry with virtual
dispatch... a registry with dynamic registration is solving a problem this project doesn't have",
`research/voxel-representation-redesign.md:205-220`, `docs/goals.md:246-247`) without reversing it:
the result is still a constexpr table with zero runtime dispatch, just composed from per-material
files instead of one hand-written array.

## 0. Method

A read-only survey agent (live, 2026-09-05 — the same pass's other agent researched the Lin-look
rendering techniques, `research/lin-look-log.md` §0) walked every consumer of `MaterialID` and
`world::chunk::block_type.hpp`'s `kBlockTable` and reported, file:line, every hardcoded site: the
enum and its three literal-8/7 ties, the Group P table's five fields and which of them had zero real
consumers, two independent GPU color palettes plus a third CPU-tool copy, three hand-mirrored copies
of the terrain band constants, two display-name switch statements, and the "Air == 0" assumption
baked into the brick/tree/vertex GPU formats. Its output is the schema below and the site-by-site
"depends on" table this pass closed one row at a time; the full report is preserved in the session
transcript (`8c15adf8-.../subagents/agent-abed5f53a9bb4d99f.jsonl`) rather than re-derived here.

## 1. The schema: what a `MaterialDef` actually is

`world/materials/include/world/materials/material_def.hpp` defines the component contract as a
`MaterialDefinition` concept (every def is checked against it at composition time, not discovered at
the first broken instantiation):

```cpp
template <typename T>
concept MaterialDefinition = requires(const TerrainQuery& q) {
    { T::name } -> std::convertible_to<const char*>;
    { T::albedo } -> std::convertible_to<Color>;
    { T::phase } -> std::convertible_to<Phase>;
    { T::shading } -> std::convertible_to<Shading>;
    { T::liquid } -> std::convertible_to<LiquidPhysics>;
    { T::yields_to_trees } -> std::convertible_to<bool>;
    { T::overrides_terrain } -> std::convertible_to<bool>;
    { T::fills(q) } -> std::same_as<bool>;
};
```

Three fields collapse what the survey found as separate, partly-dead booleans:

- **`Phase`** (`Gas | Solid | Liquid | Foliage`) replaces the old table's independent `is_solid` /
  `is_liquid` bools — a material could previously claim both or neither with nothing checking it.
  `is_solid()`, `is_liquid()`, and `is_occupied()` (solid-or-liquid: the mesher's "needs a mesh
  boundary against open air" question, deliberately distinct from `is_solid()`'s gameplay-collision
  meaning, where water must not count) are now derived methods, not stored bits.
- **`Shading`** (`Lit | Water | Foliage`) replaces the shaders' `== 3u` / `== 5u` material-ID
  literals. A material says WHICH model applies; a renderer still owns HOW that model looks (§5).
- **`LiquidPhysics`** (`buoyancy_acceleration`, `drag`, `swim_equilibrium_depth`) moves the walk
  camera's swim constants (goal 79) onto the Water component itself instead of living as free
  globals in `spectator_camera.hpp` that happened to describe water.

Two of the survey's sixteen attributes were **not** carried forward:

- **`supports_growth` / `hardness`** — the old table declared both (`block_type.hpp:22-29`, both
  commented "reserved") and the survey confirmed zero consumers of either. `MaterialDef` does not
  have these fields. The tree growth gate is `kTreeMinHeight` / slope on the heightmap (unrelated to
  material identity), and mining/hardness has no code to attach to yet. Re-adding either is a new
  member in `MaterialDef` and every one of the eight `defs/*.hpp` files when something actually reads
  it — not a placeholder carried forward speculatively on the chance it will.
- **Transparency / alpha** — confirmed by the survey as not existing anywhere (`mesh_extractor.cpp`'s
  comment: water stays deliberately opaque, goal 29's decision); still doesn't exist. Not
  reintroduced as a `MaterialDef` field either.

The vertex's 4th byte (AO / water depth / tree jitter, goal 110) and the 256-material storage
ceiling (`RegistryOf`'s `static_assert(size <= 256, ...)`) are both out of scope for what a material
*is* — they're encoding decisions the registry respects, not properties it stores.

## 2. The registry: id-from-position, not id-then-lookup

`registry.hpp`'s `RegistryOf<Defs...>` is a variadic pack of `MaterialDefinition`-constrained types,
not a container: `table` is `constexpr std::array<MaterialDef, size>{make_def<Defs>()...}`, built
once at compile time. The mechanism that actually kills "must stay in enum order" as a real
invariant instead of a comment is in `materials.hpp` — `MaterialID`'s enumerators are *derived* from
the pack's own order:

```cpp
using Registry = RegistryOf<defs::Air, defs::Stone, defs::Dirt, defs::Water, defs::Wood,
                            defs::Leaves, defs::Sand, defs::Grass>;
enum class MaterialID : std::uint8_t {
    Air = detail::id_of<defs::Air>(),       // Registry::index_of<defs::Air>()
    Stone = detail::id_of<defs::Stone>(),
    ...
};
```

There is no second place that could drift from the pack: `kMaterialCount` is `Registry::size`, and
adding a material is one new `defs/` file, one entry in the `RegistryOf<...>` pack, and one
enumerator — the `static_assert` immediately after the enum catches a forgotten enumerator (count
mismatch), and `index_of`'s own `static_assert` catches a type that isn't in the pack at all.
`RegistryOf` asserts `table[0].phase == Phase::Gas` at compile time, which is what makes "Air is
component 0" (the brick occupancy mask, the node array, the chunk palette default, the shader miss
material — every one of the survey's "Air == 0" sites) a build failure to violate, not a convention.
Still exactly what goal 113 asked for: a constexpr table, zero runtime dispatch, `properties_of()`
an array index with no bounds check — the composition changed, not the mechanism.

## 3. One band rule instead of three hand-mirrored copies

Before this pass, `terrain_fill.cpp`, `terrain_sampler.{hpp,cpp}`, and `aim_query.cpp` each carried
their own `kBeachBand` / `kSoilDepth` / `kGrassMaxSlope` and their own `if (depth == 0) ... else if
(depth <= kSoilDepth) ...` cascade, three "update together" comments doing the synchronization work
a compiler should. `terrain_query.hpp` now holds the constants once (`TerrainBands`) and a plain
data carrier (`TerrainQuery`: surface height, voxel bottom, voxel edge, sea level, beach, grassy —
the same six numbers every caller already had); each material's `fills(TerrainQuery)` claims its own
band (Stone's steep-or-deep test, Dirt's soil band, Sand's whole-soil-depth-at-the-beach rule, Grass's
one-voxel skin, Water's below-sea-and-not-terrain rule, Air's above-everything rule), and
`Registry::terrain_index` is a linear search for whichever one returns true.

This turns "the three copies agree" from an invariant enforced by a comment into a proposition
`test_registry.cpp` actually checks two ways: `old_chunk_rule` and `old_sampler_rule` are the exact
pre-Group-AC logic kept verbatim as oracles, and `terrain_material()` is checked against both across
full grids (surface -8..12 integer meters × worldY -12..16 at 1 m edge for the chunk oracle; surface
×0.37 steps × bottom ×0.11 steps at three sub-meter edges for the sampler oracle — every
`beach`/`grassy` combination each). A separate test proves the predicates are mutually exclusive and
exhaustive on their own terms — `Registry::terrain_claims(q) == 1` for every combination of three
voxel edges (1 m down to the finest sparse-brick leaf, 0.0078125 m), 13 surface heights, 121 voxel
bottoms, and both booleans — so the search never double-claims or drops a voxel, independent of
whether it happens to match the two retired code paths.

One asymmetry survives on purpose: `terrain_fill.cpp`'s whole-chunk short-circuit reasons about the
soil depth in whole voxels (goal 161), so it keeps a `constexpr std::int32_t kSoilDepth =
static_cast<std::int32_t>(TerrainBands::soil_depth)` with a `static_assert` that the truncation is
exact — the chunk path's integer view of the one shared constant, not a fourth copy of the constant
itself.

## 4. Tree voxelization priority as data, not an inlined boolean

`terrain_sampler.cpp` had two identical inline copies of `terrainSolid = current != Air && current
!= Water; replace = !terrainSolid || tree == Wood` (voxelizing a canopy, and reading back the
material at a point). Both are now `world::materials::tree_replaces(current, tree)`, reading
`properties_of(current).yields_to_trees || properties_of(tree).overrides_terrain` — Air and Water
are the two materials a tree may displace, Wood is the one tree material that displaces anything
(the trunk is sunk into the ground on purpose). `test_registry.cpp` checks this over the full cross
product of the 8 materials × {Wood, Leaves} against the old inlined expression, so the rule reads as
two named per-material facts instead of a comparison a reader has to reverse-engineer.

## 5. The renderer side: a record replaces two palettes and four literals

`render/diligent/detail/material_macros.hpp` is the shader-facing half. `MaterialRecord` (a
`float4`: rgb albedo, and the shading model as a small integer in `.w`) is generated once from
`Registry::table`; `pso_terrain.cpp` and `svo_renderer.cpp` both upload the same
`detail::kMaterialRecords` instead of each looping over `kBlockTable` independently (the mesh
renderer's `kMaterialColors` and the svo renderer's per-frame `cb->materialColors[i] = ...` loop were
two separate readings of one table before; now one array, read twice). `add_material_macros` passes
`MATERIAL_COUNT` and one `MAT_SHADING_*` macro per shading model to every shader at creation time
(`ShaderMacroHelper`, matched in both `create_shader` call sites) — `terrain.vsh.hlsl`,
`terrain.psh.hlsl`, and `svo_march.psh.hlsl` all replaced `g_MaterialColors[8]` /
`min(m, 7u)` / `== 3u` / `== 5u` with `g_Materials[MATERIAL_COUNT]` and a `MaterialShading(id) ==
MAT_SHADING_WATER` / `MAT_SHADING_FOLIAGE` test. A shader that misspells a macro name now fails to
compile instead of silently reading the wrong material — the failure mode the four literals
depended on comments to prevent.

What deliberately did **not** move: `world/materials/README.md`'s rule is "a material says WHICH
shading model, a renderer owns HOW it looks" — `ShadeWater`'s fresnel/ripple/depth-tint math, the
foliage wind-sway displacement, and all **three** independently-tuned water body colors (mesh path
`(0.22,0.46,0.48)`/`(0.03,0.14,0.30)` depth-lerp, svo path `(0.06,0.22,0.36)` flat, CPU tool
`(0.10,0.28,0.40)` flat — the survey found these as three distinct values before this pass) stay
exactly that: three renderers' own tuned constants, not unified into the registry. Unifying them was
never the ask, and the mesh path's depth tint and the svo path's flat color were each judged against
viewed captures in the Lin-look pass for reasons specific to their own reprojection/shading pipeline.

`tools/svo_render` (the CPU reference) picked up the same record (`props.albedo`, `props.shading ==
Shading::Water`) and, as a side effect of having the shading model as a comparable value instead of
a raw `MaterialID`, gained two debug views it did not have before: `--view material` and `--view
distance`, matching names the GPU path already had (`--debug-view`'s list in CLAUDE.md).

## 6. An intentional loud break: fields became methods

`properties_of(id).is_liquid` (a bool field on the old `BlockProperties`) is now
`properties_of(id).is_liquid()` (a method derived from `Phase`) — every call site the survey found
(`mesh_extractor.cpp`'s water-depth scan, twice) had to change, and a caller that still reads it as a
field fails to compile rather than silently comparing a function pointer as truthy. Deliberate: the
alternative (keep a stored `is_liquid` bool in sync with `Phase` by convention) reintroduces exactly
the "two things that must agree, kept in sync by comment" shape this whole pass exists to remove.

## 7. What was decided against, with the reason

- **Carrying `supports_growth`/`hardness` forward as dead fields** (§1): re-added only when a real
  consumer needs one, in `MaterialDef` and every `defs/*.hpp`, not spent now on the chance of a
  future mining/growth system.
- **Reintroducing transparency/alpha**: still not a real attribute anywhere; goal 29's "water stays
  opaque" stands.
- **Unifying the three water body colors, or the mesh/svo shading models generally**: renderers keep
  owning HOW a shading model looks; only WHICH one applies is registry-driven (§5).
- **A runtime registry with dynamic registration or virtual dispatch**: goal 113's decision re-cited,
  not reversed — `RegistryOf<Defs...>` is a compile-time type list, `properties_of` an array index,
  no vtable, no registration call anywhere in the tree.
- **Widening the vertex's 4th byte now**: goal 110's policy (widen to 16B before adding a fourth
  meaning) is unrelated to what a material record contains and was left untouched.

## 8. Verification

`world_materials_tests` (`world/materials/tests/test_registry.cpp`, new): 10 `TEST_CASE`s plus 7
file-scope `static_assert`s — enumerator reachability and name uniqueness/non-emptiness over all 8
materials, `is_occupied`/`is_solid` against the survey's own occupancy/solid sets, liquid physics
nonzero on Water alone, shading dispatch matching Water/Leaves/else, tree-voxelization priority over
the full 8×2 cross product, terrain-band exhaustiveness over a
3-edge×13-surface×121-bottom×2×2 grid, byte-for-byte equivalence against both retired band-rule
oracles over comparably sized grids, and the three band constants' exact values. Every existing
consumer test the survey flagged as freezing count/order/behavior (`test_brick.cpp`,
`test_samplers.hpp`, `test_mesh_extractor.cpp`, `test_baked_ao.cpp`, `test_terrain_fill.cpp`,
`test_terrain_sampler.cpp`, `test_ray_trace.cpp`, `test_aim_query.cpp`,
`test_spectator_camera.cpp`, `test_tree_decoration.cpp`) needed no behavioral changes — only
`test_brick.cpp` and `test_samplers.hpp` had a literal `8`/`7` replaced with `kMaterialCount` so they
stay correct if a material is ever added, matching the design intent rather than the tests forcing
it. `test_block_type.cpp` is deleted with `block_type.hpp` (the table it tested no longer exists —
its assertions are now `test_registry.cpp`'s, restated as component properties instead of table
rows).

Full incremental build (`windows-relwithdebinfo`, Community 14.51, Ninja): clean, 93/93 targets,
zero errors and no new warnings beyond the pre-existing per-TU `/Ob1`→`/Ob2` command-line notice
every translation unit in this tree already prints. `ctest`: **119/119 passed** (10.0 s) — up from
the pre-pass 113 (`test_block_type.cpp` and the table it tested retired; `world_materials_tests`
adds its 10 cases; the net difference is smaller than 10 because a few of the deleted file's cases
had no equivalent to re-add, their assertions now living as properties `test_registry.cpp` states
about components instead of rows). No behavioral fix was needed after the build — the refactor was
correct on the first full build+test pass.

## 9. Sources

The read-only material-site survey (research agent, live, 2026-09-05 — run alongside the Lin-look
pass's own rendering-technique research, `research/lin-look-log.md` §0/§6); goal 113
(`research/voxel-representation-redesign.md` §6, `docs/goals.md:246-247`), the decision this pass
cites and stays inside of; `CLAUDE.md`'s record of the palette count moving 6→8 (Group M), which is
why `kMaterialCount` rather than a literal was already the load-bearing quantity everywhere except
the four sites this pass closed; the `cpp-heavy-templates` skill's house style (concepts constraining
every template parameter, a compile-time registry over a variadic pack, `Rule of Zero` — every
`MaterialDef` is a plain aggregate with no owned resources) as the shape this implementation follows.
