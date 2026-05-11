# Systems

`Systems/` contains runtime systems that do engine or gameplay work without owning UI screens.

## Current state

Today the authored system in this folder is:

```text
Systems/
|-- README.md
|-- Idle/
|   |-- IdleTrigger.hpp
|   `-- README.md
|-- Input/
|   |-- InputSystem.hpp
|   `-- InputSystem.cpp
|-- Model/
|   |-- ModelSystem.hpp
|   |-- ModelSystem.cpp
|   `-- README.md
`-- Window/
    |-- DragHandler.hpp
    `-- README.md
```

Additional gameplay systems can be added here later, but this README documents only the code that exists now.

## Current conventions

- prefer concrete system types with static methods when the system has no owned lifetime
- keep system headers lean and implementation details in `.cpp`
- route cross-system communication through events instead of direct UI coupling
- keep Raylib polling and platform boundary work close to the system that owns it

## InputSystem

`InputSystem::poll()` gathers keyboard, mouse, wheel, and close-request state from Raylib each frame and publishes matching events through the shared dispatcher.

## ModelSystem

`ModelSystem` owns typed runtime model assets, startup preload tasks, model instances, and the first animation-state layer for 3D model usage.

Use it when code needs to:

- preload a model during startup
- create a runtime model instance
- attach model-local shaders
- drive local direct animation calls
- receive future event-driven animation requests
ugh the shared dispatcher.

## ModelSystem

`ModelSystem` owns typed runtime model assets, startup preload tasks, model instances, and the first animation-state layer for 3D model usage.

Use it when code needs to:

- preload a model during startup
- create a runtime model instance
- attach model-local shaders
- drive local direct animation calls
- receive future event-driven animation requests
