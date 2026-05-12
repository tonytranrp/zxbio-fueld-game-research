# engine/custom/procedural/core

Shared procedural math and type contracts live here. This folder should stay
small and dependency-light so procedural mesh, rig, IK, material, and hand code
can include it freely.

## Current contents

```text
engine/custom/procedural/core/
`-- ProceduralTypes.hpp
```

## How to use it

Put cross-cutting procedural enums, numeric aliases, pose structs, and helper
types here only when at least two procedural folders need them.

```cpp
#include "engine/custom/procedural/core/ProceduralTypes.hpp"
```

## Coding standards

- Prefer plain structs with explicit units in field names.
- Keep Raylib includes limited to math/render types that the procedural layer
  already uses.
- Do not add renderer state or resource ownership here.
- Avoid game-specific concepts; this is engine procedural infrastructure.
