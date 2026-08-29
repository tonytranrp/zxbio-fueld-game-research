# src/engine/Rust/bevy

A small, headless, tech-demo-only Bevy render bridge, off by default
(`BIOFUEL_ENABLE_BEVY_BRIDGE`). It runs a real `bevy::app::App` with no OS
window, renders each frame into an offscreen `Image` target, and hands the
raw pixels back to C++ for `BevyRenderService` to upload into a raylib
texture -- the same "external per-frame pixel producer" shape
`VideoFfmpegBackend` uses for decoded video frames.

This crate is a member of the `src/engine/Rust/` Cargo workspace
(originally `rust/bevy_bridge/`). Its package name is still
`biofuel_bevy_bridge`; it builds as a `cdylib` (not a `staticlib` like
`physics/`) because two separately-compiled Rust staticlibs cannot both link
into one C++ binary.

**This crate is unrelated to, and not superseded by, any future Bevy-ECS
game-logic boundary.** It renders a Bevy-authored scene and displays the
finished image; a future ECS-logic boundary (see this repo's separate
Bevy-boundary notes) would flow entity/transform data the other direction
entirely. If that future crate is ever built, it needs a name clearly
distinct from "bevy" -- which this folder now occupies -- to avoid exactly
the confusion this note exists to prevent.

Unlike `physics/`, this crate's hand-written code (`build_headless_app` and
the `new_renderer`/`step_frame`/`frame_pixels`/etc. functions) lives at the
crate root alongside the `#[cxx::bridge]` `ffi` module rather than in its
own separate module -- so it can't use the same per-module
`#[forbid(unsafe_code)]` scoping `physics/` uses (see that crate's own
lib.rs for the pattern). A crate-wide forbid was tried here and reverted:
cxx's macro expansion needs `unsafe` to cross the ABI regardless. This
crate's own hand-written code has zero `unsafe` today, same as `physics/`,
just not compiler-enforced.

## How to use it

Use `src/engine/bevy/BevyRenderService.hpp`; do not include the generated
CXX header directly.

```powershell
cargo test --manifest-path src\engine\Rust\bevy\Cargo.toml --locked
```
