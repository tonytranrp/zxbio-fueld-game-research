#![forbid(unsafe_code)]
//! Plain-Rust utilities shared by every crate under `src/engine/Rust/Engine/`
//! that talks to Rapier's nalgebra-based math or needs a stable opaque-handle
//! scheme. Deliberately dependency-light and free of any one crate's own
//! `#[cxx::bridge]` types -- this crate sits upstream of `physics/` and
//! `game/`, never the other way around.
//!
//! Extracted from `physics/` once a second consumer (`game/`) needed the same
//! handle-packing scheme and math conversions -- see this workspace's own
//! `README.md` for the "wait for a second consumer" rule this followed.

pub mod convert;
pub mod handles;
