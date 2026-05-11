# Data/event

The event layer is built on `entt::dispatcher` and stores only event definitions plus the manager that owns the dispatcher.

## Current folders

```text
Data/event/
|-- EventManager.hpp
|-- EventManager.cpp
|-- animation/
|-- input/
|-- model/
|-- mouse/
|-- screen/
`-- window/
```

## Event domains

| Domain | File | Purpose |
|---|---|---|
| `animation/` | `AnimationEvents.hpp` | Screen transition started/completed (carries `screenName` from `Screen::getName()`) |
| `input/` | `InputEvents.hpp` | Keyboard and mouse button events |
| `model/` | `ModelEvents.hpp` | Model lifecycle and animation events |
| `mouse/` | `MouseEvents.hpp` | Mouse movement, wheel, and position events |
| `screen/` | `ScreenEvents.hpp` | Screen stack navigation events |
| `window/` | `WindowEvents.hpp` | Window resize, focus, and close events |

## Rules

- event structs are plain data in `.hpp` files
- the manager owns lifecycle; event folders do not have behavior classes
- keep event names specific to the domain that fires them
- prefer project aliases in event payloads unless the external API shape is naturally a raw type

## Adding an event

1. Put the new struct in the matching domain folder.
2. Include that header where the event is produced or consumed.
3. Trigger it through `Data::eventBus().trigger(...)`.

Keep the event API small and explicit. If a payload needs many unrelated fields, it usually means the event is doing too much.
