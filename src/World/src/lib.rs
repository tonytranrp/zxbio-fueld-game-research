//! World engine entry point.
//!
//! Phase 1 (see the migration plan artifact): a real `bevy_app::App`
//! running `bevy_render`'s own renderer, pinned to the Vulkan backend,
//! reachable from C++ through this cxx-bridged crate, built through
//! Corrosion into BiofuelGame.exe -- callable more than once per process.
//! Phase 1(a) first proved the persisted-EventLoop + `run_app_on_demand`
//! reentrancy pattern (validated standalone in the Phase-0 throwaway spike)
//! survives the real FFI/build integration using plain wgpu, in isolation
//! from Bevy; Phase 1(b), now folded into `session.rs`, replaces that with
//! the real `bevy_render` integration on top of the same proven runner.
//! Phase 3 adds real scene content on top of that: the ported
//! `ExplorationLevel` box geometry (`level.rs`) and a first-person player
//! controller (`player.rs` + `fp_camera.rs`, driven by `input_state.rs`'s
//! accumulated winit events) -- a real `Camera3d` now, not the placeholder
//! `Camera2d` clear-color-only milestone. glTF-imported models (the
//! viewmodel hands, real level art) are still a later phase.
//!
//! No crate-wide `forbid(unsafe_code)` here: cxx's bridge macro (`mod ffi`
//! below) needs unsafe internally to cross the ABI, so a crate-wide forbid
//! can't compile through it (same constraint as Engine/game/src/lib.rs).
//! Each hand-written module is `#[forbid(unsafe_code)]` individually
//! instead.

#[forbid(unsafe_code)]
mod adapter_probe;
#[forbid(unsafe_code)]
mod carbon;
#[forbid(unsafe_code)]
mod crop;
#[forbid(unsafe_code)]
mod event_loop_cell;
#[forbid(unsafe_code)]
mod fp_camera;
#[forbid(unsafe_code)]
mod fuel;
#[forbid(unsafe_code)]
mod hud;
#[forbid(unsafe_code)]
mod hydrogen;
#[forbid(unsafe_code)]
mod input_state;
#[forbid(unsafe_code)]
mod level;
#[forbid(unsafe_code)]
mod physics;
#[forbid(unsafe_code)]
mod player;
#[forbid(unsafe_code)]
mod miscanthus;
#[forbid(unsafe_code)]
mod session;
#[forbid(unsafe_code)]
mod switchgrass;
#[forbid(unsafe_code)]
mod viewmodel;
#[forbid(unsafe_code)]
mod water;

// mod ffi has to stay literally in this file: cxx's bridge macro processes
// the mod's items directly and doesn't expand an `include!` placed inside
// it (same constraint noted in Engine/game/src/lib.rs).
#[allow(unused_qualifications)]
#[cxx::bridge(namespace = "biofuel::world")]
mod ffi {
    #[derive(Clone, Copy, Debug, Default, PartialEq)]
    struct SessionInput {
        // -1 = new game. Placeholder for Phase 4's real save-slot data;
        // unread for now, kept so the C++ call site doesn't need to change
        // shape again once save data actually crosses this boundary.
        save_slot: i32,
    }

    #[derive(Clone, Copy, Debug, Default)]
    struct SessionExit {
        // A plain u8 code, not a cxx shared enum: 0 = returned to menu
        // normally, 1 = no Vulkan-capable adapter found (see
        // adapter_probe), 2 = any other clean internal failure. Matching
        // constants live in src/engine/world/WorldBridge.hpp on the C++
        // side -- keep the two in sync by hand if this ever grows past
        // three values.
        reason: u8,
    }

    extern "Rust" {
        fn run_world_session(input: SessionInput) -> SessionExit;
    }
}

fn run_world_session(input: ffi::SessionInput) -> ffi::SessionExit {
    let reason = match session::run(input.save_slot) {
        session::SessionExitReason::ReturnedToMenu => 0,
        session::SessionExitReason::VulkanUnavailable => 1,
        session::SessionExitReason::InternalError => 2,
    };
    ffi::SessionExit { reason }
}
