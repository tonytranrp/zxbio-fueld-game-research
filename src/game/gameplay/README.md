# game/gameplay

Future Fuel Farm gameplay service and event contracts live here.

## Current contents

```text
game/gameplay/
|-- FutureSystems.hpp
|-- FutureServiceModule.hpp
`-- FutureEventModule.hpp
```

## Purpose

This folder defines typed placeholders for the planned economy, ecology, season,
tech, save, and game-state systems. They let the runtime registry and generated
module pipeline know where gameplay services will fit before the systems are
fully implemented.

## How to extend it

When a future system becomes real:

1. Replace the empty backend with a concrete service class.
2. Keep the existing service tag if the concept is the same.
3. Add typed events for important cross-system notifications.
4. Move domain data to `game/data/`.

```cpp
class EconomySystem {
public:
    void advanceSeason(FarmState& farm);
};

BIOFUEL_RUNTIME_SERVICE(EconomyService, "service.economy", EconomySystem,
    EconomySystem::instance());
```

## Coding standards

- This is game-specific; do not move reusable engine primitives here.
- Keep future tags stable unless the game design changes.
- Payloads should carry domain data, not UI strings.
- Avoid implementing gameplay directly in screens once systems exist.
