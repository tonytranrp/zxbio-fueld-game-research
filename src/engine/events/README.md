# engine/events

The event layer is built on `entt::dispatcher` internally, with typed event
modules as the public API.

## Current folders

```text
engine/events/
|-- EventManager.hpp/.cpp
|-- EventServiceModule.hpp
|-- animation/
|-- input/
|-- model/
|-- mouse/
|-- physics/
|-- screen/
|-- video/
`-- window/
```

## Event domains

| Domain | Purpose |
| --- | --- |
| `animation/` | Screen transition and visual effect lifecycle |
| `input/` | Keyboard and mouse button events |
| `model/` | Model lifecycle and animation commands |
| `mouse/` | Mouse movement, wheel, and position events |
| `physics/` | Rapier contacts plus engine shape lifecycle/grab events |
| `screen/` | Screen stack, transition, layer, and debug overrides |
| `video/` | Video playback start, completion, and errors |
| `window/` | Window resize, focus, and close events |

## Rules

- Event structs are plain data in `.hpp` files.
- Event folders do not own behavior classes.
- Keep event names specific to the domain that fires or consumes them.
- Prefer project aliases in payloads unless an external API shape is naturally a
  raw type.
- Each domain owns typed event tags/specs in a local `*EventModule.hpp`.
- Publish and subscribe through `engine::runtime::typed::Events`.

## Adding an event

1. Put the new struct in the matching domain folder.
2. Add the matching typed event tag/spec in the domain's `*EventModule.hpp`.
3. Add `BIOFUEL_EVENT_MODULE(...)` so the generated registry picks it up.
4. Publish it with `Events::publish<TEvent>(payload)`.

Keep the event API small and explicit. If a payload needs many unrelated fields,
the event is probably doing too much.
