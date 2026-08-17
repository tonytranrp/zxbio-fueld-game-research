# src

The source tree is split between reusable engine code and game-specific code.
Generated output stays outside `src/`; use top-level `build/` or `out/build/`.

## Folder map

```text
src/
|-- main.cpp        program entry point
|-- CMakeLists.txt  source target, generated registries, embedded shaders
|-- engine/         reusable app/runtime/render/media/input systems
`-- game/           Fuel Farm screens, presentation, models, data, gameplay
```

`engine/` owns code that should be useful to more than one screen or game
feature. `game/` owns Fuel Farm behavior, screens, and domain-specific data.
When code starts as a debug screen experiment but becomes reusable, move the
math/state/resource ownership into `engine/` and leave only UI workflow in
`game/`.

## Coding standards

- Prefer root-relative includes from `src`, such as
  `engine/runtime/Runtime.hpp` or `game/screens/main_menu/MainMenuScreen.hpp`.
- Use project aliases from `engine/core/Types.hpp` for fixed-width values
  (`f32`, `u32`, `usize`) inside project code.
- Runtime access goes through `biofuel::engine::runtime::Runtime`; do not add a
  second global facade.
- Register services, events, shaders, and assets through the typed module
  macros instead of manual lists.
- Keep raw Raylib resource ownership in managers, caches, or RAII helpers.
- Use `constexpr` for layout constants, timing constants, and stable labels.
- Keep feature flags build-safe: optional systems must compile away or become
  no-ops when disabled.

## Adding a new area

1. Create the smallest folder that matches the ownership boundary.
2. Add a local `README.md` before the folder grows.
3. Add headers and sources to `src/CMakeLists.txt`.
4. If the folder exposes a runtime feature, add a typed service/event/asset
   module so the generated registries can see it.

Example:

```cpp
// Good include style from any source under src/
#include "engine/runtime/Runtime.hpp"
#include "game/screens/main_menu/MainMenuScreen.hpp"
```
