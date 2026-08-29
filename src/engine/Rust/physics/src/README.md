# engine/physics/rapier_bridge/src

The CXX bridge implementation lives here. `lib.rs` exposes a narrow Rust API for
creating 2D/3D Rapier worlds, stepping them, creating primitive colliders,
raycasting, and draining compact contact events.

## Current contents

```text
engine/physics/rapier_bridge/src/
|-- README.md
`-- lib.rs
```

## Coding standards

- Do not expose raw Rapier handles directly; pack them into stable `u64` values.
- Do not let Rust-owned collections cross into C++; expose indexed reads.
- Keep tests close to the bridge when they validate Rapier behavior directly.
