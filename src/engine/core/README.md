# engine/core

Core types and tiny cross-system helpers live here. This folder is intentionally
small because everything in the project can include it.

## Current contents

```text
engine/core/
|-- Types.hpp
|-- LoadingTask.hpp
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

Use `LoadingTaskQueue` when a screen needs visible, weighted startup work:

```cpp
biofuel::LoadingTaskQueue queue;
queue.add({"Compile shaders", 2.0f, [] { Runtime::shader().init(); }});
queue.processNext();
```

If the queue owns an active async task, clear it with the task manager so only
that task is cancelled:

```cpp
queue.clear(Runtime::tasks());
```

The reset implementation is intentionally private so callers cannot discard an
active async task id without first requesting cancellation through `TaskManager`.

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
