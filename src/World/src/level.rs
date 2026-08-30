//! Static level geometry -- ground, perimeter fence, barn shell, scattered
//! crates/drums/hay, one landmark placeholder. Box positions/half-extents/
//! colors are a straight port of `src/game/screens/exploration/
//! ExplorationLevel.cpp`'s own box list (also already ported once into
//! `Engine/game/src/level.rs`'s `level_boxes()` -- this is intentionally a
//! third copy, not a shared dependency: see this crate's `Cargo.toml` for
//! why `World/` stays a standalone workspace, and `ExplorationLevel.cpp`'s
//! own doc comment for the established "keep these in sync by hand"
//! precedent already in this codebase). Unlike both prior copies, this one
//! spawns real `bevy_pbr` mesh/material entities directly (Phase 1/2's
//! raylib-DrawCube and Engine/game's C++-FFI-DTO paths don't apply here --
//! `World/` renders itself).
#![forbid(unsafe_code)]

use crate::physics::RapierPhysics;
use bevy::app::App;
use bevy::asset::Assets;
use bevy::color::Color;
use bevy::math::primitives::Cuboid;
use bevy::mesh::{Mesh, Meshable};
use bevy::pbr::{MeshMaterial3d, StandardMaterial};
use bevy::prelude::{Component, Mesh3d, Transform};
use rapier3d::prelude as r3;

// The native Rapier body backing this entity's collision -- Fixed for every
// entity level.rs spawns (all static geometry; nothing here moves). Not
// read yet -- kept on the entity for a future per-object query (e.g. a
// pick/interact system) rather than one this phase's code needs itself;
// same "attached for a later reader" precedent as Engine/game/src/
// components.rs's KIND_DEMO constant.
#[allow(dead_code)]
#[derive(Component, Clone, Copy)]
pub(crate) struct RapierBody(pub(crate) r3::RigidBodyHandle);

struct LevelBox {
    center: r3::Vector,
    half_extents: r3::Vector,
    color_rgba: [u8; 4],
}

fn level_boxes() -> Vec<LevelBox> {
    const GROUND_HALF_SIZE: f32 = 14.0;
    const BOUNDARY_HALF_HEIGHT: f32 = 1.5;
    const BOUNDARY_THICKNESS: f32 = 0.15;
    const BARN_WALL_HEIGHT: f32 = 1.6;
    const BARN_WALL_THICKNESS: f32 = 0.15;
    const BARN_HALF_WIDTH: f32 = 4.5;
    const BARN_HALF_DEPTH: f32 = 5.5;
    const BARN_CENTER_Z: f32 = 9.0;
    const DOOR_HALF_WIDTH: f32 = 1.1;

    const GROUND_COLOR: [u8; 4] = [110, 100, 80, 255];
    const BEIGE: [u8; 4] = [211, 176, 131, 255];
    const BARN_COLOR: [u8; 4] = [150, 90, 60, 255];
    const DARKBROWN: [u8; 4] = [76, 63, 47, 255];
    const CRATE_COLOR: [u8; 4] = [170, 140, 90, 255];
    const DRUM_COLOR: [u8; 4] = [90, 90, 95, 255];
    const HAY_COLOR: [u8; 4] = [190, 170, 110, 255];
    const LANDMARK_COLOR: [u8; 4] = [140, 60, 50, 255];

    let b = GROUND_HALF_SIZE;
    let back_z = BARN_CENTER_Z + BARN_HALF_DEPTH;
    let front_z = BARN_CENTER_Z - BARN_HALF_DEPTH;
    let front_segment_half_width = (BARN_HALF_WIDTH - DOOR_HALF_WIDTH) * 0.5;
    let front_segment_offset = DOOR_HALF_WIDTH + front_segment_half_width;

    let bx = |cx: f32, cy: f32, cz: f32, hx: f32, hy: f32, hz: f32, color: [u8; 4]| LevelBox {
        center: r3::Vector::new(cx, cy, cz),
        half_extents: r3::Vector::new(hx, hy, hz),
        color_rgba: color,
    };

    vec![
        // Ground (top surface at y=0).
        bx(0.0, -0.1, 0.0, GROUND_HALF_SIZE, 0.1, GROUND_HALF_SIZE, GROUND_COLOR),
        // Perimeter boundary (bounds the walkable space).
        bx(0.0, BOUNDARY_HALF_HEIGHT, -b, b, BOUNDARY_HALF_HEIGHT, BOUNDARY_THICKNESS, BEIGE),
        bx(0.0, BOUNDARY_HALF_HEIGHT, b, b, BOUNDARY_HALF_HEIGHT, BOUNDARY_THICKNESS, BEIGE),
        bx(-b, BOUNDARY_HALF_HEIGHT, 0.0, BOUNDARY_THICKNESS, BOUNDARY_HALF_HEIGHT, b, BEIGE),
        bx(b, BOUNDARY_HALF_HEIGHT, 0.0, BOUNDARY_THICKNESS, BOUNDARY_HALF_HEIGHT, b, BEIGE),
        // Barn shell: back + two side walls solid; front split around a doorway gap.
        bx(0.0, BARN_WALL_HEIGHT, back_z, BARN_HALF_WIDTH, BARN_WALL_HEIGHT, BARN_WALL_THICKNESS, BARN_COLOR),
        bx(-BARN_HALF_WIDTH, BARN_WALL_HEIGHT, BARN_CENTER_Z, BARN_WALL_THICKNESS, BARN_WALL_HEIGHT, BARN_HALF_DEPTH, BARN_COLOR),
        bx(BARN_HALF_WIDTH, BARN_WALL_HEIGHT, BARN_CENTER_Z, BARN_WALL_THICKNESS, BARN_WALL_HEIGHT, BARN_HALF_DEPTH, BARN_COLOR),
        bx(-front_segment_offset, BARN_WALL_HEIGHT, front_z, front_segment_half_width, BARN_WALL_HEIGHT, BARN_WALL_THICKNESS, BARN_COLOR),
        bx(front_segment_offset, BARN_WALL_HEIGHT, front_z, front_segment_half_width, BARN_WALL_HEIGHT, BARN_WALL_THICKNESS, BARN_COLOR),
        // Flat roof cap.
        bx(0.0, BARN_WALL_HEIGHT * 2.0 + 0.1, BARN_CENTER_Z, BARN_HALF_WIDTH + 0.2, 0.1, BARN_HALF_DEPTH + 0.2, DARKBROWN),
        // Scattered obstacles for scale reference and movement variety.
        bx(-3.0, 0.35, -2.0, 0.35, 0.35, 0.35, CRATE_COLOR),
        bx(-2.0, 0.35, -2.6, 0.35, 0.35, 0.35, CRATE_COLOR),
        bx(3.2, 0.45, -3.0, 0.3, 0.45, 0.3, DRUM_COLOR),
        bx(3.9, 0.45, -3.4, 0.3, 0.45, 0.3, DRUM_COLOR),
        bx(-4.5, 0.4, 3.0, 0.6, 0.4, 0.4, HAY_COLOR),
        // Landmark placeholder -- swap for the Meshy-generated fuel silo once it exists.
        bx(9.0, 2.5, -9.0, 1.0, 2.5, 1.0, LANDMARK_COLOR),
    ]
}

// Player spawn point -- matches ExplorationLevel.hpp's own m_playerSpawn
// default (0, 1, -8), just inside the fenced yard facing the barn.
pub(crate) const PLAYER_SPAWN: r3::Vector = r3::Vector::new(0.0, 1.0, -8.0);

// Takes `&mut App` and does its own three short-lived `world_mut()` calls
// (physics, then meshes, then materials, then a final spawn pass) rather
// than one call holding several resources at once: `World` only lets one
// `resource_mut::<T>()` guard live at a time per borrow (Bevy's own
// `resource_scope` exists to work around exactly this), and three
// sequential short borrows are simpler to read here than nesting it three
// levels deep for resources that don't actually need to be alive
// simultaneously -- collecting handles into plain Vecs between passes is
// enough.
pub(crate) fn spawn_level(app: &mut App) {
    let boxes = level_boxes();

    let body_handles: Vec<r3::RigidBodyHandle> = {
        let mut physics_guard = app.world_mut().resource_mut::<RapierPhysics>();
        // One explicit deref into a plain &mut RapierPhysics, then split
        // that single reference's fields below -- going through Mut<T>'s
        // DerefMut per field access (physics_guard.colliders vs. &mut
        // physics_guard.bodies) calls deref_mut() twice, which the borrow
        // checker can't prove disjoint the way it can for two field
        // projections of one plain reference (confirmed by a real E0499
        // without this; same reasoning Engine/game/src/level.rs's own
        // spawn_level documents).
        let physics: &mut RapierPhysics = &mut physics_guard;
        boxes
            .iter()
            .map(|level_box| {
                let body = r3::RigidBodyBuilder::new(r3::RigidBodyType::Fixed)
                    .translation(level_box.center)
                    .build();
                let handle = physics.bodies.insert(body);
                let collider = r3::ColliderBuilder::cuboid(
                    level_box.half_extents.x,
                    level_box.half_extents.y,
                    level_box.half_extents.z,
                );
                physics.colliders.insert_with_parent(collider, handle, &mut physics.bodies);
                handle
            })
            .collect()
    };

    let mesh_handles: Vec<bevy::asset::Handle<Mesh>> = {
        let mut meshes = app.world_mut().resource_mut::<Assets<Mesh>>();
        boxes
            .iter()
            .map(|level_box| {
                meshes.add(
                    Cuboid::new(
                        level_box.half_extents.x * 2.0,
                        level_box.half_extents.y * 2.0,
                        level_box.half_extents.z * 2.0,
                    )
                    .mesh(),
                )
            })
            .collect()
    };

    let material_handles: Vec<bevy::asset::Handle<StandardMaterial>> = {
        let mut materials = app.world_mut().resource_mut::<Assets<StandardMaterial>>();
        boxes
            .iter()
            .map(|level_box| {
                let [r, g, b, a] = level_box.color_rgba;
                materials.add(StandardMaterial {
                    base_color: Color::srgba_u8(r, g, b, a),
                    // No light entities are spawned this phase (see
                    // session.rs) -- unlit keeps every box's flat
                    // base_color visible exactly as it was under raylib's
                    // own unshaded DrawCube, instead of rendering pure
                    // black with zero light sources. Real lighting is a
                    // follow-up phase, not required to prove the level
                    // geometry itself ported correctly.
                    unlit: true,
                    ..Default::default()
                })
            })
            .collect()
    };

    let world = app.world_mut();
    for (i, level_box) in boxes.iter().enumerate() {
        world.spawn((
            Transform::from_xyz(level_box.center.x, level_box.center.y, level_box.center.z),
            Mesh3d(mesh_handles[i].clone()),
            MeshMaterial3d(material_handles[i].clone()),
            RapierBody(body_handles[i]),
        ));
    }
}
