//! cxx bridge crate: Bevy ECS + native Rapier physics driving real game
//! objects (level geometry, a demo entity), read back by C++/Raylib each
//! frame as a flat transform batch -- mirrors physics/'s own
//! `body_poses_3d` batch-readback shape exactly (`count =
//! bodies.len().min(poses.len())`, truncation-safe, no per-entity FFI
//! calls).
//!
//! Organized by concern, same reasoning as physics/lib.rs:
//! - `mod ffi` (bottom of this file) -- the cxx bridge contract. Has to
//!   stay literally in this file: cxx's macro processes the mod's items
//!   directly and doesn't expand an `include!` placed inside it.
//! - `world` -- the headless Bevy App + native rapier3d physics world.
//!
//! Every hand-written module is `#[forbid(unsafe_code)]` individually, same
//! reasoning as physics/lib.rs: cxx's own bridge macro (the `ffi` module
//! below) needs `unsafe` to cross the ABI, so a crate-wide forbid can't
//! compile through it.
#[cfg(test)]
#[forbid(unsafe_code)]
mod tests;
#[forbid(unsafe_code)]
mod world;

use world::GameWorld;

// #[allow(unused_qualifications)]: cxx's macro expansion of the shared
// structs below trips the workspace's `unused_qualifications` lint on its
// own generated code, not these declarations themselves (same false
// positive as physics/'s and bevy/'s ffi modules).
#[allow(unused_qualifications)]
#[cxx::bridge(namespace = "biofuel::engine::game")]
mod ffi {
    #[derive(Clone, Copy, Debug, Default, PartialEq)]
    struct BridgeVec3 {
        x: f32,
        y: f32,
        z: f32,
    }

    #[derive(Clone, Copy, Debug, Default, PartialEq)]
    struct BridgeQuat {
        x: f32,
        y: f32,
        z: f32,
        w: f32,
    }

    #[derive(Clone, Copy, Debug, Default)]
    struct BridgeGameObject {
        entity_kind: u8,
        position: BridgeVec3,
        rotation: BridgeQuat,
        half_extents: BridgeVec3,
        color_rgba: [u8; 4],
    }

    extern "Rust" {
        type GameWorld;

        fn new_game_world() -> Box<GameWorld>;
        fn step_game(world: &mut GameWorld, dt: f32) -> u64;
        fn read_game_objects(world: &GameWorld, out: &mut [BridgeGameObject]) -> u64;
    }
}

fn new_game_world() -> Box<GameWorld> {
    Box::new(world::new_game_world())
}

fn step_game(world: &mut GameWorld, dt: f32) -> u64 {
    world::step_game(world, dt)
}

fn read_game_objects(world: &GameWorld, out: &mut [ffi::BridgeGameObject]) -> u64 {
    world::read_game_objects(world, out)
}
