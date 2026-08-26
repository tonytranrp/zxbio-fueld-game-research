# C++20 Modules Build Story

Pipeline-c++ is, first and foremost, a **header-only** library: `#include <pb/pipeline.hpp>`
gives you the entire DSL, runtime, and metadata surface with zero build-system ceremony.
On toolchains that support [C++20 named modules](https://en.cppreference.com/w/cpp/language/modules),
the same surface can *also* be consumed as the named module **`pb.pipeline`**, built from
[`include/pb/pipeline.mpp`](../include/pb/pipeline.mpp).

The module build is **opt-in** and **off by default**. The default `clang-dev-ninja`
preset, the header-only consumption story, and every existing test are completely
unaffected by it.

---

## TL;DR

| | Header build (default) | Module build (opt-in) |
|---|---|---|
| How to consume | `#include <pb/pipeline.hpp>` | `import pb.pipeline;` |
| CMake target | `pb::pipeline` | `pb::module` (links `pb::pipeline`) |
| CMake option | — (always available) | `PB_BUILD_MODULE=ON` |
| Preset | `clang-dev-ninja`, `release-*`, … | `modules-ninja` |
| Toolchain floor | any C++20 compiler | CMake ≥ 3.28, module-capable compiler + generator |
| Rebuild cost | re-parses headers per TU | imports a prebuilt BMI |

Both builds expose the identical public surface in namespace `pb` — the module is a
thin curated re-export of the header library, not a fork. The smoke test
[`tests/module/use_module.cpp`](../tests/module/use_module.cpp) compiles **only** through
`import pb.pipeline;` (it includes no `<pb/...>` header) and exercises
`pb::from<>::then<>::to<>`, `pb::compile`, `engine.run`, and `engine.try_run`, proving the
two surfaces agree.

---

## The module name and what it exports

The module interface unit `include/pb/pipeline.mpp` declares:

```cpp
module;
#include <pb/pipeline.hpp>   // full library in the global module fragment
export module pb.pipeline;   // <-- the module name is `pb.pipeline`

export namespace pb {
  using core::from;
  using runtime::compile;
  using runtime::sequential;
  // … the full curated DSL + runtime + metadata surface …
}
```

The global-module-fragment `#include <pb/pipeline.hpp>` pulls the whole header-only
library into the module's purview; the `export namespace pb { using … }` block then
re-exports the curated public names. Standard-library headers the interface depends on
(`<string>`, `<tuple>`, `<variant>`, …) are included for visibility but are **not**
re-exported — consumers still `#include`/`import` those themselves, exactly as with the
header build. That is why `tests/module/use_module.cpp` still `#include`s `<memory>`,
`<type_traits>`, etc.

Consume it with:

```cpp
import pb.pipeline;

struct AddOne { using input_type = In; using output_type = Out;
                Out operator()(In) const; };

using P = pb::from<In>::then<AddOne>::to<Out>;
auto engine = pb::compile<P>(pb::sequential{});
auto out    = engine.run(In{...});
```

---

## Building the module

### Preset (recommended)

A dedicated preset turns the option on and points at a clang + Ninja toolchain:

```bash
cmake --preset modules-ninja
cmake --build --preset modules-ninja
ctest --preset modules-ninja        # runs the pb_use_module smoke (label: module)
```

`modules-ninja` inherits `clang-dev-ninja` and only adds `PB_BUILD_MODULE=ON`, so it is a
strict superset of the default developer build plus the module artifacts.

### Manual

```bash
cmake -S . -B build/mod -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DPB_BUILD_MODULE=ON
cmake --build build/mod
```

### What the CMake option does

`PB_BUILD_MODULE` (default `OFF`) guards two things:

1. A `pb_module` static library (alias `pb::module`) whose sole source is
   `include/pb/pipeline.mpp`, added via the modern
   `target_sources(... PUBLIC FILE_SET CXX_MODULES ...)` API. It links `pb::pipeline`
   **PUBLIC**, so the include directories *and* the compiled runtime translation units
   (`error.cpp`, `sequential.cpp`) propagate to anything that links `pb::module`.
2. The `pb_use_module` smoke executable in `tests/`, which links `pb::module` and is
   marked `CXX_SCAN_FOR_MODULES ON` (see caveat below).

When the option is `OFF`, neither target exists — the default build graph is byte-for-byte
what it was before.

---

## Toolchain reality / caveats

Module support in build systems is still young; the notes below record what was actually
observed on the reference toolchain for this repository.

- **Reference toolchain (verified working):** CMake 4.x (≥ 3.28 required for the stable
  `FILE_SET CXX_MODULES` support), Ninja generator, Clang 22 (llvm-mingw, UCRT,
  `x86_64-w64-windows-gnu`). On this toolchain `cmake --preset modules-ninja` +
  `cmake --build --preset modules-ninja` builds `pipeline.mpp` into a BMI, links
  `pb::module`, and `pb_use_module` passes under ctest.

- **CMake floor is 3.28, not the repo's 3.23.** `FILE_SET CXX_MODULES` only became the
  stable, supported spelling in CMake 3.28. The option is gated so that older CMake (which
  the rest of the project still supports) simply never instantiates the module target.
  Do **not** lower this expectation: configuring `PB_BUILD_MODULE=ON` on CMake < 3.28 is
  unsupported.

- **Consumer scanning must be explicit.** A translation unit that *imports* a module but
  *provides* none is, by default, compiled **unscanned** by CMake's Ninja module support —
  in which case `import pb.pipeline;` fails with *"module 'pb.pipeline' not found"*. The
  fix used here is `set_target_properties(pb_use_module PROPERTIES CXX_SCAN_FOR_MODULES ON)`.
  Downstream consumers of `pb::module` that import the module must do the same on their own
  import-only targets.

- **Generator support.** Module dependency scanning (dyndep) is wired up for **Ninja** and
  the **Visual Studio** generators. The Unix/NMake Makefiles generators do **not** support
  C++20 module scanning; use Ninja.

- **Standard-library headers are not re-exported.** This is by design (see above), so the
  module is a drop-in for the *`pb`* surface only — keep including/importing the standard
  library headers you use.

- **Compiler floor for the language feature itself:** MSVC 17.5+, Clang 17+, GCC 15+ per the
  banner in `pipeline.mpp`. The CMake `FILE_SET CXX_MODULES` plumbing additionally requires a
  module-aware *build* integration (Clang/MSVC with Ninja or VS as above).

- **The module is a convenience, not a requirement.** If your toolchain cannot build the
  module cleanly, keep `PB_BUILD_MODULE=OFF` (the default) and use the header. You lose
  nothing in capability — only the per-TU header re-parse savings.

---

## Verifying it yourself

```bash
# module build is green:
cmake --preset modules-ninja && cmake --build --preset modules-ninja
ctest --preset modules-ninja -R '^pb_use_module$'

# default header build is unaffected:
cmake --preset clang-dev-ninja && cmake --build --preset clang-dev-ninja
ctest --preset clang-dev-ninja
```
