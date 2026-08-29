# src/engine/Rust

Every Rust crate in this project lives here, as members of one Cargo
workspace rooted at this folder's own Cargo.toml. Before this move, Rust
code lived in a top-level `rust/` folder (`rust/rapier_bridge/`,
`rust/bevy_bridge/`); before that, the physics bridge lived nested at
`src/engine/physics/rapier_bridge/`. This is the second, and intended-to-be-
final, home for this code. A stale reference to either older path anywhere
in this repo's docs or comments should be fixed to point here.

## What lives here, and why it's named this way

```text
src/engine/Rust/
|-- Cargo.toml           # workspace root -- pins cxx/cxx-build once for every member
|-- Cargo.lock           # shared lockfile; keep committed, build with --locked
|-- rust-toolchain.toml  # one toolchain pin for the whole workspace
|-- physics/             # was rapier_bridge/ -- Rapier 2D+3D rigid-body physics
`-- bevy/                # was bevy_bridge/ -- headless Bevy renderer, off by default
```

Each crate folder is named after what it is inside the engine (`physics`,
`bevy`), not after "bridge" -- crossing an FFI boundary into C++ is already
established once, by this folder's own name. Cargo package names
(`biofuel_rapier_bridge`, `biofuel_bevy_bridge`) are unchanged -- a
workspace member's folder name and its package name are independent, and
this project's build is wired to the package name.

| This folder | C++-side counterpart |
|---|---|
| `src/engine/Rust/physics/` | `src/engine/physics/` |
| `src/engine/Rust/bevy/` | `src/engine/bevy/` |

## No shared crate between `physics` and `bevy` (yet, deliberately)

`physics/src/handles.rs` packs a Rapier `(index, generation)` handle pair
into a single `u64` -- a small, genuinely reusable pattern a future,
larger Bevy-ECS boundary will plausibly also want (Bevy's own `Entity` type
is built the same way internally). We have not factored this into a shared
crate: today there is exactly one consumer, the future consumer doesn't
exist yet, and its actual needs aren't designed (see this repo's separate
future-Bevy-boundary notes -- it may even expose Bevy's own built-in
entity bit-packing directly rather than reimplementing this scheme).

**Trigger for revisiting this**: the moment a second crate needs an
identical index+generation-packed-into-u64 scheme, extract
`pack_handle`/`unpack_handle` from `physics/src/handles.rs` -- not the rest
of that file, which is typed directly against `rapier2d`/`rapier3d` handle
types and isn't reusable as-is -- into a small shared crate then, with that
consumer's real requirements in hand.

## Adding a new crate

Add a sibling folder next to `physics/` and `bevy/`, append its name to
`members` in this folder's Cargo.toml, and add one
`corrosion_import_crate`/`corrosion_add_cxxbridge` pair to the root
`CMakeLists.txt`. Nothing else needs to change.

## The zero-unsafe/zero-panic rule (physics crate specifically)

`physics/` builds with `panic = "abort"` because a Rust panic cannot unwind
across the cxx FFI boundary -- a panic anywhere in this crate takes down the
entire game process. **This crate has zero `unsafe`, zero
`.unwrap()`/`.expect()`, and zero `panic!` anywhere in `src/`, including its
own tests, enforced by `#[forbid(unsafe_code)]` on every hand-written module
and checked by direct inspection.** Every bridge function handles invalid
input (a stale handle, an out-of-range index, a NaN/infinite float) by
returning an `Option`-shaped result to C++, never by panicking.

`bevy/` does not carry the same guarantee: it calls into Bevy's own APIs,
some of which can panic internally on invalid engine state even without
this crate's own code calling `.unwrap()` anywhere. Its absence of literal
`unsafe`/`.unwrap()`/`panic!` today is a real property, but not the same
designed-in zero-panic guarantee `physics/` carries -- and unlike `physics/`,
`bevy/` can't use a crate-wide `#[forbid(unsafe_code)]` at all: its
hand-written code lives at the crate root alongside the `#[cxx::bridge]`
`ffi` module rather than in its own separate module, and that macro's own
generated glue needs `unsafe` to cross the ABI. See the comment above
`bevy/src/lib.rs`'s module declarations for the full reasoning.
