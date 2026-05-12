# engine/events/model

Model lifecycle and model animation command payloads live here.

## Current contents

```text
engine/events/model/
|-- ModelEvents.hpp
`-- ModelEventModule.hpp
```

## How to use it

`ModelSystem` listens for model command events such as state changes or action
playback. Use events when the caller should not hold a direct model instance.

```cpp
Events::publish<typed::model::ModelPlayAction>({
    .instanceId = selectedInstance,
    .stateName = "upgrade_pop",
});
```

Direct model instance calls are fine when ownership is already local.

## Coding standards

- Use instance IDs for commands that target a live model.
- Keep asset registration in `game/models/`, not in events.
- Events should describe intent; resource loading belongs to the model system.
- Register new tags in `ModelEventModule.hpp`.
