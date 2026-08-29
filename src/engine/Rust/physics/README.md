# src/engine/Rust/physics

Rust owns the Rapier integration in this crate. CMake imports it through
Corrosion, generates the CXX bridge, and links the resulting static library
into the C++ game executable. This crate is a member of the
`src/engine/Rust/` Cargo workspace (originally `src/engine/physics/
rapier_bridge/`, then `rust/rapier_bridge/`, consolidated here alongside
`bevy` -- see `src/engine/Rust/README.md`).

This folder's own package name is still `biofuel_rapier_bridge` -- unchanged
by either move.

## Current contents

```text
src/engine/Rust/physics/
|-- Cargo.toml
|-- build.rs
|-- README.md
`-- src/
```

## How to use it

C++ code should not include generated CXX headers directly. Use
`src/engine/physics/PhysicsSystem.hpp`; it hides this crate behind typed
handles and descriptors.

Run Rust-side verification with:

```powershell
cargo test --manifest-path src\engine\Rust\physics\Cargo.toml --locked
```

## Coding standards

- Keep Rapier sets, pipelines, and query internals private to Rust.
- Keep bridge structs small and copyable.
- Reserve handle value `0` as invalid for C++ callers.
- Keep `Cargo.lock` committed and use locked Cargo builds.
- No `unsafe`, no `.unwrap()`/`.expect()`, no `panic!` anywhere in this
  crate outside its own tests -- enforced by `#[forbid(unsafe_code)]` on
  every hand-written module; see `src/engine/Rust/README.md` for why.
