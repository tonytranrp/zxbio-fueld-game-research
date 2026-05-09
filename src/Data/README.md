# Data

`Data/` is the lightweight access bridge around the global event manager, screen manager, and font manager.

## Files

```text
Data/
|-- Data.hpp
`-- event/
    |-- EventManager.hpp
    |-- EventManager.cpp
    |-- animation/
    |-- input/
    |-- mouse/
    |-- screen/
    `-- window/
```

## What `Data.hpp` exposes

- `Data::eventBus()` - shared `entt::dispatcher`
- `Data::events()` - `EventManager` singleton
- `Data::screens()` - `ScreenManager` singleton
- `Data::fonts()` - `FontManager` singleton

## Usage guidance

- high-level gameplay and UI code can usually include `Data/Data.hpp`
- low-level producers such as systems may include specific event headers directly when they only need event types
- events stay as plain aggregate structs; do not add behavior or constructors to them

## Type guidance

- use project aliases for project-owned numeric fields
- keep Raylib-native values when they are true boundary data, such as key codes or positions captured from input APIs
- prefer aggregate initialization when triggering events
