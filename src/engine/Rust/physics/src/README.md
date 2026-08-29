# src/engine/Rust/physics/src

The CXX bridge implementation lives here. `lib.rs` hosts the `#[cxx::bridge]`
contract itself (shared structs + fn signatures -- cxx's macro processes this
mod's items directly and can't `include!` it from elsewhere); every actual
implementation lives in the other files below, organized by concern.

## Current contents

```text
src/engine/Rust/physics/src/
|-- README.md
|-- lib.rs                   # the cxx::bridge contract; wires the modules below to it
|-- handles.rs                # opaque u64 handle packing/unpacking
|-- convert.rs                # bridge-struct <-> Rapier math/enum conversions
|-- world2d.rs                # 2D physics world (currently unused by any game-side system)
|-- world3d.rs                # 3D physics world (backs ExplorationScreen)
|-- character_controller.rs   # kinematic character controller, split out of world3d.rs
|-- raycast3d.rs               # 3D raycasting, split out of world3d.rs
`-- tests.rs                  # #[cfg(test)] only
```

## Coding standards

- Do not expose raw Rapier handles directly; pack them into stable `u64`
  values (see `handles.rs`).
- Do not let Rust-owned collections cross into C++; expose indexed reads.
- Keep tests close to the bridge when they validate Rapier behavior
  directly.
- No `unsafe`, no `.unwrap()`/`.expect()`, no `panic!` in any module here
  outside `tests.rs` -- each hand-written module (everything except the
  `ffi` mod in `lib.rs`) is individually `#[forbid(unsafe_code)]`; see
  `src/engine/Rust/README.md` for why it's scoped per-module rather than
  crate-wide.
