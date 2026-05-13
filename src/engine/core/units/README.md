# engine/core/units

Strong coordinate and measurement wrappers live here. Use these types at system
boundaries so screen pixels, physics meters, normalized camera coordinates, and
tile coordinates do not get mixed accidentally.

## Current contents

```text
engine/core/units/
|-- EngineUnits.hpp
`-- README.md
```

## How to use it

```cpp
#include "engine/core/units/EngineUnits.hpp"

using namespace biofuel::engine::core::units;

PixelToMeterScale scale{.pixelsPerMeter = 32.0f};
ScreenPixels2D cursor{96.0f, 64.0f};
WorldMeters2D world = toWorldMeters(cursor, scale);
TileCoord tile = toTileCoord(cursor, TileSizePixels{.value = 32.0f});
```

Use explicit Raylib interop only at the boundary:

```cpp
Vector2 raylibPoint = world.toVector2();
WorldMeters2D typed = WorldMeters2D::fromVector2(raylibPoint);
```

## Unit ownership

- `ScreenPixels2D`: UI and render-space pixel positions.
- `WorldMeters2D` / `WorldMeters3D`: physics and meter-style world positions.
- `NormalizedCameraCoord2D` / `NormalizedCameraCoord3D`: camera or hand-tracking
  values in normalized image space.
- `TileCoord`: integer tile-grid coordinates.
- `PixelToMeterScale`: explicit conversion policy between screen pixels and
  meter-style world units.

## Coding standards

- Do not add implicit conversions between unit spaces.
- Convert once at subsystem boundaries, then pass typed values deeper in.
- Keep wrappers small, trivially copyable, and dependency-light.
- Use `.toVector2()` / `.toVector3()` only when calling Raylib APIs.
