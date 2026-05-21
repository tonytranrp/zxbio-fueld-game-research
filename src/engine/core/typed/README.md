# engine/core/typed

Compile-time registry building blocks live here. Service, event, asset, shader,
and screen registries build on these primitives.

## Current contents

```text
engine/core/typed/
|-- Meta.hpp          typelists, registries, uniqueness checks
|-- ModuleExport.hpp  marker macro consumed by CMake generation
`-- TypedModule.hpp   registry-module helper macro
```

## How to use it

Most code should not include these files directly. Domain folders expose higher
level macros such as `BIOFUEL_SERVICE_MODULE` or `BIOFUEL_EVENT_MODULE`.

When a new category needs generated registry support, define a module wrapper:

```cpp
BIOFUEL_TYPED_REGISTRY_MODULE(MyModule, FirstTag, SecondTag)
BIOFUEL_TYPED_MODULE(my_category, AppMyRegistry, my_namespace::MyModule)
```

`ModuleExport.hpp` is intentionally a marker. CMake scans macro invocations and
generates registry headers from them.

## Coding standards

- Keep this code header-only and consteval-friendly.
- Avoid runtime allocation and runtime registration.
- Preserve unique type checks; duplicate tags should fail at compile time.
- Do not add game-specific tags here.

## Dependency audit: compile-time registry helpers

The current `TypeList` / `Registry` layer is intentionally small and produces
project-specific diagnostics at the call sites that validate generated service,
event, asset, shader, and screen registries. Do not add a dependency unless it
deletes local code or strengthens a weak contract at compile time.

| Candidate | Decision | Rationale |
| --- | --- | --- |
| Boost.MP11 | Defer | It could replace pieces of `Meta.hpp`, but the local surface is under 100 lines and keeps readable project-specific `static_assert` messages. Revisit only if registry algorithms grow beyond contains/index/concat/unique validation. |
| Boost.PFR | Reject for this layer | There is no aggregate reflection, field serialization, or comparison code here to remove. |
| Boost.SML | Reject for this layer | The typed registry layer is not a runtime state machine. |
| `magic_enum` | Defer | Useful only if enum name/count/string conversion becomes a real API need; current typed registries do not do enum string conversion. |
| `frozen` | Defer | Useful only if registries need constexpr runtime lookup tables; current consumers use type membership and generated aliases. |
| `glaze` | Reject for this layer | JSON/config serialization is not part of the typed registry primitives. |
| `fmt` | Reject as direct dependency | `spdlog` already brings its bundled fmt use for logging; no direct formatting need exists in this layer. |
| Highway/SIMD helpers | Reject | No measured hot loop exists in this compile-time metadata code. |

Existing dependencies are kept where they already own a domain: EnTT backs the
event dispatcher, Taskflow backs task scheduling, Pipeline-c- backs gameplay
pipeline validation, Raylib backs rendering/audio/windowing, and
Corrosion/Rapier backs physics. None of those replace the local registry
metadata helpers.
