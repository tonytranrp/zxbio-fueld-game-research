# engine/core

Core types and tiny cross-system helpers live here. This folder is intentionally
small because everything in the project can include it.

## Current contents

```text
engine/core/
|-- Types.hpp
|-- units/
`-- typed/
```

## How to use it

Use the fixed-width aliases and transparent hash for project code:

```cpp
#include "engine/core/Types.hpp"

std::unordered_map<std::string, i32, biofuel::TransparentHash, std::equal_to<>> ids;
f32 progress = 0.0f;
usize selected = 0U;
```

`LoadingTaskQueue` (visible, weighted startup work for a loading screen) lives
in `engine/tasks/LoadingTask.hpp` now — see `engine/tasks/README.md` for its
usage example. It depends on `TaskManager`, which is why it moved out of this
dependency-light folder.

Use `core/units` wrappers at subsystem boundaries:

```cpp
using namespace biofuel::engine::core::units;

WorldMeters2D world = toWorldMeters(ScreenPixels2D{96.0f, 64.0f}, PixelToMeterScale{32.0f});
TileCoord tile = toTileCoord(ScreenPixels2D{96.0f, 64.0f}, TileSizePixels{32.0f});
```

## Coding standards

- Keep this folder dependency-light.
- No Raylib ownership, services, events, or screen types belong in `core/`.
- Unit wrappers may expose explicit Raylib interop helpers, but they must not own
  Raylib resources.
- Prefer simple structs and free-standing aliases over broad utility classes.
- Anything that needs a subsystem include should live outside `core/`.
