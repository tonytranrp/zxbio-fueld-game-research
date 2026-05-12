# engine/core

Core types and tiny cross-system helpers live here. This folder is intentionally
small because everything in the project can include it.

## Current contents

```text
engine/core/
|-- Types.hpp
|-- LoadingTask.hpp
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

## Coding standards

- Keep this folder dependency-light.
- No Raylib ownership, services, events, or screen types belong in `core/`.
- Prefer simple structs and free-standing aliases over broad utility classes.
- Anything that needs a subsystem include should live outside `core/`.
