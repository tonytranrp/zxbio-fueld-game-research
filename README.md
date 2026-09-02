# voxel_engine

A high-performance sparse-voxel rendering engine for [Bevy](https://bevyengine.org/), built around
GPU ray marching through a brick-grid voxel structure rather than triangle meshes.

## Quick start

```rust
use bevy::prelude::*;
use voxel_engine::{
    spawn_voxel_chunk, VoxelChunk, VoxelEnginePlugin, VoxelFlycamPlugin, VoxelId, VoxelMaterial,
    VoxelPalette,
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

    let palette = VoxelPalette::default();

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
}
```

## Status

Milestone 1: static voxel scenes, GPU DDA ray marching, basic lighting + shadows. Procedural
generation, destruction, vegetation, water, and world streaming are later milestones — the storage
layout is deliberately shaped so none of them require a redesign.

## Why not hardware ray tracing / an existing voxel crate?

Verified during design (Sept 2026): wgpu's hardware ray tracing is still explicitly experimental,
and Bevy's own Solari raytracer is mesh-only with no voxel primitive. The proven, currently-shipping
approach for voxel-specific rendering is DDA ray marching through a brick grid in a shader — that's
what this engine does. No existing crate (`VoxelHex`, `voxelis`) was a good fit: both are either
stalled, version-lagging, or storage-only with a data model that doesn't match this engine's flat
brick-grid.

## License

Dual-licensed under [MIT](LICENSE-MIT) or [Apache-2.0](LICENSE-APACHE), at your option.
