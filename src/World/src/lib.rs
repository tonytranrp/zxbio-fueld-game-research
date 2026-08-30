//! World engine entry point.
//!
//! Phase 1(a) scope (see the migration plan artifact): prove the persisted-
//! EventLoop + `run_app_on_demand` reentrancy pattern -- validated
//! standalone in the Phase-0 throwaway spike -- survives being the REAL
//! cxx-bridged crate, built through Corrosion into BiofuelGame.exe and
//! callable more than once per process. Renders a plain Vulkan-cleared
//! window via raw wgpu, not yet real bevy_render: isolating this step's own
//! risk (does reentrancy survive the real FFI/build integration?) from the
//! separate, more ordinary work of wiring up Bevy's own renderer on top,
//! which is the immediate next step once this one is verified.
//!
//! No crate-wide `forbid(unsafe_code)` here: cxx's bridge macro (`mod ffi`
//! below) needs unsafe internally to cross the ABI, so a crate-wide forbid
//! can't compile through it (same constraint as Engine/game/src/lib.rs).
//! Each hand-written module is `#[forbid(unsafe_code)]` individually
//! instead.

#[forbid(unsafe_code)]
mod adapter_probe;
#[forbid(unsafe_code)]
mod event_loop_cell;
#[forbid(unsafe_code)]
mod session;

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
