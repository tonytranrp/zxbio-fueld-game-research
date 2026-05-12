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
