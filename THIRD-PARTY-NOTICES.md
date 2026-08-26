# Third-Party Notices

This project depends on the open-source libraries listed below. Every license was verified
directly against the dependency's own source at the exact pinned version this project uses
(C++ libraries: the `LICENSE` file at the pinned CPM tag/commit; Rust crates: the `license`
field published on crates.io for the exact locked version in `Cargo.lock`). No license is
stated from memory.

All licenses in this dependency tree are permissive (MIT, Apache-2.0, zlib, Unlicense,
BSD-style, Unicode-3.0). There is no copyleft (GPL/LGPL/AGPL/MPL) and no source-available or
non-commercial license anywhere in the tree. Every dependency here is compatible with
closed-source commercial distribution of the built game.

This file does not yet state a license for this project's own code — see the note at the
bottom.

## C++ dependencies

| Dependency | Version | License | Notes |
|---|---|---|---|
| [raylib](https://github.com/raysan5/raylib) | 5.5 | zlib | Shared library; `raylib.dll` ships with the game. On Windows, vendored as raylib's own official prebuilt binary under `third_party/raylib-5.5-win64-msvc16/` (see that directory's `PROVENANCE.md`) instead of CPM-built from source, for build speed. Other platforms still build from source via CPM. |
| [EnTT](https://github.com/skypjack/entt) | 3.14.0 | MIT | Copyright Michele Caini. Fetched via CPM.cmake. |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.11.3 | MIT | Copyright Niels Lohmann. Fetched via CPM.cmake. |
| [Taskflow](https://github.com/taskflow/taskflow) | 4.0.0 | MIT | Copyright Dr. Tsung-Wei Huang / UW-Madison. GitHub's license detector shows NOASSERTION because of a nonstandard license-file title; the text is standard MIT. Fetched via CPM.cmake. |
| [spdlog](https://github.com/gabime/spdlog) | 1.14.1 | MIT | Copyright Gabi Melman. Fetched via CPM.cmake. |
| [Pipeline-c-](https://github.com/tonytranrp/Pipeline-c-) | commit `d8bc48f` | MIT | Copyright 2026 tonytranrp (this project's own author). Vendored as source under `third_party/pipeline-c/` (checked out at this commit) for in-tree customization, rather than fetched via CPM.cmake. |
| [Corrosion](https://github.com/corrosion-rs/corrosion) | 0.6.1 | MIT | Copyright Andrew Gaspar. Build-time only (CMake/Cargo integration) — not shipped in the binary. Fetched via CPM.cmake. |
| [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) | 0.40.2 | MIT | Copyright Lars Melchior. Build-time only — not shipped in the binary. |
| [sccache](https://github.com/mozilla/sccache) | 0.17.0 | MPL-2.0 | Optional local dev tool (compiler-output cache), not a dependency of the project itself — nothing from it is fetched, linked, or shipped in the binary. Only relevant if a contributor has it installed. |

## Rust dependencies (`src/engine/physics/rapier_bridge`, statically linked into the game binary)

The physics bridge crate is built as a `staticlib` and linked directly into the executable, so
every crate below ships inside the binary and its notice obligations attach to the
distributed product.

| Crate | License |
|---|---|
| rapier2d, rapier3d 0.32.0 | Apache-2.0 |
| parry2d, parry3d 0.26.1 | Apache-2.0 |
| nalgebra 0.34.2, nalgebra-macros 0.3.0 | Apache-2.0 |
| simba 0.9.1 | Apache-2.0 |
| approx 0.5.1 | Apache-2.0 |
| codespan-reporting 0.13.1 | Apache-2.0 (build-time, via cxx-build) |
| glamx 0.1.3 | MIT OR Apache-2.0 |
| glam (all locked versions, 0.14.0–0.32.1) | MIT OR Apache-2.0 |
| cxx, cxx-build, cxxbridge-cmd, cxxbridge-flags, cxxbridge-macro 1.0.194 | MIT OR Apache-2.0 |
| foldhash 0.2.0 | Zlib |
| bytemuck 1.25.0 | Zlib OR Apache-2.0 OR MIT |
| safe_arch 0.7.4 | Zlib OR Apache-2.0 OR MIT |
| wide 0.7.33 | Zlib OR Apache-2.0 OR MIT |
| byteorder 1.5.0 | Unlicense OR MIT |
| termcolor 1.4.1 | Unlicense OR MIT |
| winapi-util 0.1.11 | Unlicense OR MIT |
| libm 0.2.16 | MIT |
| ordered-float 5.3.0 | MIT |
| slab 0.4.12 | MIT |
| strsim 0.11.1 | MIT |
| unicode-ident 1.0.24 | (MIT OR Apache-2.0) AND Unicode-3.0 |
| unicode-width 0.2.2 | MIT OR Apache-2.0 |
| allocator-api2, anstyle, arrayvec, autocfg, bit-vec, bitflags, cc, clap, clap_builder, clap_lex, downcast-rs, either, ena, equivalent, find-msvc-tools, hash32, hashbrown, heapless, indexmap, link-cplusplus, log, matrixmultiply, num-bigint, num-complex, num-derive, num-integer, num-rational, num-traits, paste, proc-macro2, profiling, profiling-procmacros, quote, rawpointer, robust, rstar, rustc-hash, scratch, serde, serde_core, serde_derive, shlex, smallvec, spade, stable_deref_trait, static_assertions, syn, thiserror, thiserror-impl, typenum, vec_map, windows-link, windows-sys | MIT OR Apache-2.0 |
| biofuel_rapier_bridge (this project's own crate) | Not yet declared — `publish = false`, no `license` field set. |

Rapier's own documentation explicitly states it is free and open-source under Apache-2.0 and
targets commercial game use.

## Distribution checklist (not yet done)

Before shipping a binary build of this game (even an itch.io or Steam upload), the following
should happen — this file covers only the *attribution* half:

1. **Decide this project's own license** and add a top-level `LICENSE` file. This is a
   decision for the project owner, not something inferred from the dependency tree.
2. **Retain copyright and license notices** for the dependencies above, since MIT and
   Apache-2.0 both require it at distribution time. This file, shipped alongside the binary
   (or surfaced in an in-game "Open Source Licenses" screen), satisfies that for the
   dependencies listed here. Apache-2.0 (the Dimforge/rapier stack) additionally requires
   including a full copy of the Apache-2.0 license text and stating that the code is
   unmodified from upstream — both true here.
3. **Follow the asset-attribution policy** already documented in `assets/models/README.md`
   for any third-party models, fonts, or audio actually shipped (none are in the repo yet).
4. Optionally regenerate this file mechanically in the future with `cargo install cargo-about`
   or `cargo bundle-licenses` for the Rust side, if the dependency set grows large enough that
   hand-maintenance becomes error-prone.
