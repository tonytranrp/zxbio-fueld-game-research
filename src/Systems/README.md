# Systems

`Systems/` contains runtime systems that do engine or gameplay work without owning UI screens.

## Current state

Today the authored system in this folder is:

```text
Systems/
|-- README.md
`-- Input/
    |-- InputSystem.hpp
    `-- InputSystem.cpp
```

Additional gameplay systems can be added here later, but this README documents only the code that exists now.

## Current conventions

- prefer concrete system types with static methods when the system has no owned lifetime
- keep system headers lean and implementation details in `.cpp`
- route cross-system communication through events instead of direct UI coupling
- keep Raylib polling and platform boundary work close to the system that owns it

## InputSystem

`InputSystem::poll()` gathers keyboard, mouse, wheel, and close-request state from Raylib each frame and publishes matching events through the shared dispatcher.
