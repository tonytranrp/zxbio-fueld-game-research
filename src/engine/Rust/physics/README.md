# rust/rapier_bridge

Rust owns the Rapier integration in this crate. CMake imports it through
Corrosion, generates the CXX bridge, and links the resulting static library into
the C++ game executable. This crate is a member of the `rust/` Cargo workspace
(moved here from `src/engine/physics/rapier_bridge/`) alongside `bevy_bridge`.

## Current contents

```text
rust/rapier_bridge/
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
cargo test --manifest-path rust\rapier_bridge\Cargo.toml --locked
```

## Coding standards

- Keep Rapier sets, pipelines, and query internals private to Rust.
- Keep bridge structs small and copyable.
- Reserve handle value `0` as invalid for C++ callers.
- Keep `Cargo.lock` committed and use locked Cargo builds.
