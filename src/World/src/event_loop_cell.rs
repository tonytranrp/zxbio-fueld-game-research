//! Holds the ONE process-lifetime winit `EventLoop`.
//!
//! Critical invariant, confirmed from winit's own source (see the migration
//! plan's reentrancy research): a second `EventLoop::build()`/`new()` call
//! anywhere in the process returns `Err(RecreationAttempt)` -- the guard is
//! a process-global `static`, never reset on native targets. This project's
//! `panic = "abort"` workspace policy turns an unhandled `Err` here into an
//! instant, silent process kill, so every session -- including the first --
//! must reuse this same `EventLoop` via
//! `winit::platform::run_on_demand::EventLoopExtRunOnDemand::run_app_on_demand`,
//! never build a fresh one. Do not "simplify" this into building the
//! `EventLoop` fresh per session; that silently reintroduces the crash,
//! invisible until the player's second session.
#![forbid(unsafe_code)]

use std::cell::RefCell;
use winit::event_loop::EventLoop;

thread_local! {
    static EVENT_LOOP: RefCell<Option<EventLoop<()>>> = const { RefCell::new(None) };
}

/// Runs `f` against the persisted `EventLoop`, building it lazily on the
/// first call ever made on this thread. `Err(())` means the very first
/// build attempt itself failed (should not happen post-Phase-0 validation
/// on a normal desktop session, but handled rather than unwrapped).
pub(crate) fn with_event_loop<R>(f: impl FnOnce(&mut EventLoop<()>) -> R) -> Result<R, ()> {
    EVENT_LOOP.with(|cell| {
        let mut slot = cell.borrow_mut();
        if slot.is_none() {
            match EventLoop::new() {
                Ok(event_loop) => *slot = Some(event_loop),
                Err(_) => return Err(()),
            }
        }
        let event_loop = slot.as_mut().expect("just ensured Some above");
        Ok(f(event_loop))
    })
}
