//! Static level geometry, ported from `ExplorationLevel.cpp` -- ground,
//! fence, barn shell, scattered crates/drums/hay, one landmark placeholder.
//! Positions/half-extents/colors are a straight port of that file's own box
//! list; see it for the yard-layout rationale. Spawned here as native Rapier
//! Fixed bodies + Bevy entities instead of C++ PhysicsWorld3D calls --
//! `ExplorationLevel` itself now keeps only `playerSpawn()`.
use crate::components::{RapierBody, RenderShape, KIND_STATIC};
use crate::world::RapierPhysics;
use bevy::prelude::{Mut, Transform, World};
use rapier3d::prelude as r3;

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

pub(crate) fn spawn_level(world: &mut World) {
    world.resource_scope(|world, mut physics: Mut<RapierPhysics>| {
        // Reborrow once to a plain &mut RapierPhysics: going through Mut<T>'s
        // own Deref/DerefMut for each field access individually (rather than
        // through one concrete reference) defeats the borrow checker's
        // disjoint-field-borrow analysis for `colliders`/`bodies` below --
        // physics/world3d.rs's own attach_cuboid_3d does the same
        // `colliders.insert_with_parent(_, _, &mut bodies)` call shape
        // successfully because it's already a plain `&mut RapierWorld3D`.
        let physics = &mut *physics;
        for level_box in level_boxes() {
            let body = r3::RigidBodyBuilder::new(r3::RigidBodyType::Fixed)
                .translation(level_box.center)
                .build();
            let handle = physics.bodies.insert(body);
            let collider = r3::ColliderBuilder::cuboid(
                level_box.half_extents.x,
                level_box.half_extents.y,
                level_box.half_extents.z,
            );
            physics
                .colliders
                .insert_with_parent(collider, handle, &mut physics.bodies);

            world.spawn((
                Transform::from_xyz(level_box.center.x, level_box.center.y, level_box.center.z),
                RapierBody(handle),
                RenderShape {
                    half_extents: level_box.half_extents,
                    color_rgba: level_box.color_rgba,
                    kind: KIND_STATIC,
                },
            ));
        }
    });
}
