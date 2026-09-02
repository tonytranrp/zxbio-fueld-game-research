# voxel_engine

A high-performance sparse-voxel rendering engine for [Bevy](https://bevyengine.org/), built around
GPU ray marching through a brick-grid voxel structure rather than triangle meshes. Real, measured
scale: a single scene with **2.15 billion voxels** (1024 procedurally-generated chunks) rendered at
~38fps on a laptop RTX 4070, no streaming, no tricks beyond what's described below.

## Quick start

```rust
use bevy::color::LinearRgba;
use bevy::prelude::*;
use voxel_engine::{
    spawn_voxel_chunk, VoxelChunk, VoxelEnginePlugin, VoxelFlycamPlugin, VoxelId, VoxelMaterial,
    VoxelMaterialInfo, VoxelPalette,
};

fn main() {
    App::new()
        .add_plugins(DefaultPlugins)
        .add_plugins((VoxelEnginePlugin, VoxelFlycamPlugin))
        .add_systems(Startup, setup)
        .run();
}

fn setup(
    mut commands: Commands,
    mut meshes: ResMut<Assets<Mesh>>,
    mut materials: ResMut<Assets<VoxelMaterial>>,
    mut images: ResMut<Assets<Image>>,
    mut buffers: ResMut<Assets<bevy::render::storage::ShaderBuffer>>,
) {
    let mut chunk = VoxelChunk::new(UVec3::splat(128));
    chunk.set(UVec3::new(10, 10, 10), VoxelId::new(1));

    let mut palette = VoxelPalette::new();
    palette.set(VoxelId::new(1), VoxelMaterialInfo { color: LinearRgba::new(0.8, 0.25, 0.2, 1.0) });

    spawn_voxel_chunk(
        &mut commands,
        &mut meshes,
        &mut materials,
        &mut images,
        &mut buffers,
        &chunk,
        &palette,
        Transform::IDENTITY,
    );

    commands.spawn(Camera3d::default());
}
```

See `examples/voxel_scene.rs` (hand-placed shapes + procedural terrain, three scenes side by side)
and `examples/voxel_world.rs` (a real multi-chunk world, including the billion-voxel stress test
below) for complete, actually-compiled reference code — the snippet above is deliberately minimal
and kept small specifically to stay easy to keep accurate; those example files are what actually
gets built and tested every time this crate changes.

## Turn on real GPU performance: `DepthPrepass` + `OcclusionCulling`

**Read this before judging this engine's performance from a naive setup.** The ray-marching
fragment shader `discard`s pixels that miss and never writes an explicit depth — on essentially
every GPU architecture that disables hardware early-Z entirely unless the render target already
has a clean depth buffer to test against. Add two components to your own camera to get one back:

```rust
use bevy::core_pipeline::prepass::DepthPrepass;
use bevy::render::occlusion_culling::OcclusionCulling;

commands.spawn((
    Camera3d::default(),
    DepthPrepass,
    OcclusionCulling, // requires DepthPrepass to be present; both are free, built into Bevy 0.19
    Transform::from_xyz(0.0, 0.0, 10.0).looking_at(Vec3::ZERO, Vec3::Y),
));
```

Real, measured effect on this engine's own 16-chunk test grid, ground-level camera, everything else
identical: **~22fps with neither component, ~79fps with just `DepthPrepass`.** This is not a minor
tweak — it's usually the single biggest performance lever available, and the library can't add it
for you: it doesn't create or own your camera entity (see "Design notes" below).

## What's actually built

- **Sparse brick-grid storage** (`VoxelChunk`) — dense per-brick occupancy counts plus a coarser
  mip hierarchy above them (Teardown's own documented technique), so both the CPU reference marcher
  (`cast_ray`) and the GPU shader can skip large empty regions in one step instead of visiting every
  voxel.
- **GPU ray marching** (`VoxelMaterial`) — an N-level (mip → brick → voxel) iterative DDA marcher in
  WGSL, Lambertian shading, hard shadows, embedded automatically by `VoxelEnginePlugin`.
- **Procedural terrain** (`fill_heightmap_terrain`, `PerlinNoise`, `HeightmapParams`) — hand-rolled
  Perlin noise (not the `noise` crate — kept in-house so it stays swappable for a SIMD backend
  later) driving heightmap terrain generation. World-coordinate-aware: pass a `world_origin` so
  multiple chunks generate seamlessly-continuous terrain across their shared boundary instead of
  each restarting from its own local origin — see that function's own doc comment, and
  `examples/voxel_world.rs` for spawning a real multi-chunk world this way. There's no `World` or
  chunk-manager type in this crate — a caller loops over grid coordinates and calls
  `spawn_voxel_chunk` once per chunk; see "Design notes" for why that's deliberate.
- **Distance-based LOD** (`VoxelMaterial::set_lod_distance`) — beyond a world-space distance you
  choose, the marcher stops at the brick level and shades with that brick's precomputed average
  color instead of resolving individual voxels. Off by default (existing behavior unchanged unless
  you opt in). Real measured tradeoff on the same 16-chunk grid: 22fps with LOD off, up to ~62fps
  with it forced everywhere (the ceiling — not usable as-is, since it also flattens near-camera
  detail); a realistic threshold recovers somewhere between those depending how aggressively you
  set it. There's no single right default — it's a genuine quality/performance tradeoff only you
  can judge with your own content on screen, which is exactly why this method exists instead of the
  library picking a value for you.
- **Editing after spawn** (`VoxelMaterial::update_from_chunk`) — call `VoxelChunk::set` on your own
  chunk data, then `update_from_chunk` to re-upload the change to the SAME already-spawned entity
  (mutates the existing GPU assets in place via `Assets::get_mut`, so nothing needs to be despawned
  or re-spawned). A real, tested end-to-end path (build → spawn → edit → re-upload), not just an
  API that compiles — see `render::material::tests::update_from_chunk_reuploads_the_edited_voxel_
  and_lod_data_in_place` for the exact scenario it verifies.
- **CPU reference marcher** (`cast_ray`) — the same core N-level hierarchical traversal as the GPU
  shader, validated independently on the CPU first. Useful for picking (find which voxel a ray
  hits) to drive an edit via `update_from_chunk` above. Not a complete behavioral mirror of the GPU
  shader: it always resolves the exact voxel a ray hits and has no notion of the GPU's distance-
  based LOD, so picking at a distance where LOD is visually active will report a more precise
  answer than what's actually rendered on screen at that point — see `cast_ray`'s own doc comment
  for the full reasoning.
- **Debug flycam** (`VoxelFlycamPlugin`) — WASD + mouse-look, for examples and quick iteration; not
  required if you bring your own camera controller.

## Real numbers, not vendor figures

All measured on this project's own hardware (RTX 4070 Laptop GPU), never assumed:

| Scene | Voxels | FPS |
|---|---|---|
| 16 chunks, `DepthPrepass`+`OcclusionCulling`, ground-level camera | 33.5M | ~79 (from ~22 without those two components) |
| 256 chunks, birds-eye camera | 537M | ~50 |
| 1024 chunks, birds-eye camera | **2.15 billion** | ~38, no crash, no OOM |

The 1024-chunk scene measured ~2.8GiB VRAM — real headroom on an 8GB GPU, not close to exhausted at
this scale. Reproduce it yourself:

```bash
VOXEL_WORLD_GRID_SIZE=32 cargo run --release --example voxel_world --features dev_tools \
  --config profile.release.lto=false --config profile.release.codegen-units=16
```

(The `--config` flags work around this project's own `lto = "fat"` release profile — tuned for the
library's real shipped runtime performance — running LLVM out of memory when linking an example
this large; they're not needed for normal `cargo build --release` of your own project depending on
this crate as a library.)

## Design notes worth knowing before you build on this

- **Chunks must not spatially overlap.** Depth-based rendering (opaque sorting, the prepass,
  occlusion culling) relies entirely on each chunk's bounding-cuboid mesh geometry depth, since the
  shader never writes an explicit per-pixel depth. This is exactly correct for non-overlapping
  chunks — a ray marched inside one chunk can only ever hit a point within that chunk's own cuboid
  extent, so cuboid depth ordering always matches the true ray-marched ordering — but silently wrong
  if two chunks' cuboids overlap. See `spawn_voxel_chunk`'s own doc comment for the full reasoning.
- **No `World`/chunk-manager abstraction exists**, deliberately. Multi-chunk worlds today are "call
  `spawn_voxel_chunk` in a loop with the right `world_origin`/`Transform` per chunk" (see
  `examples/voxel_world.rs`) — no registry, no streaming, no loading/unloading. Real measurement
  (2.15 billion voxels at real, comfortable VRAM headroom) suggests that's not yet a limitation
  worth building speculative infrastructure for; revisit if a real use case needs otherwise.
- **No CPU-side SIMD** in this crate today. Investigated and deliberately deprioritized: after
  fixing `VoxelChunk`'s own bulk-fill bookkeeping (the actual dominant cost), noise sampling is
  ~20% of terrain-generation time — a real but modest ceiling that hasn't yet justified the
  complexity of a `pulp`-based SIMD backend. The noise implementation is hand-rolled (not the
  `noise` crate) specifically so that door stays open later without a rewrite.

## Why not hardware ray tracing / an existing voxel crate?

Verified during design (Sept 2026): wgpu's hardware ray tracing is still explicitly experimental,
and Bevy's own Solari raytracer is mesh-only with no voxel primitive. The proven, currently-shipping
approach for voxel-specific rendering is DDA ray marching through a brick grid in a shader — that's
what this engine does. No existing crate (`VoxelHex`, `voxelis`) was a good fit: both are either
stalled, version-lagging, or storage-only with a data model that doesn't match this engine's flat
brick-grid.

## License

Dual-licensed under [MIT](LICENSE-MIT) or [Apache-2.0](LICENSE-APACHE), at your option.
