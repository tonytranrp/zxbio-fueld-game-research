# game/presentation/world

2D presentation of the farm world. This folder renders the `FarmState` tile grid
as a flat, top-down grid of coloured rectangles — the 2D view of the world,
separate from the 3D voxel gameplay.

## Current contents

```text
game/presentation/world/
`-- TileRenderer.hpp/.cpp   draws the FarmState tile grid + reports the hovered tile
```

## TileRenderer

Renders the farm's tile grid and resolves mouse hover. Pure presentation: it
reads `FarmState` and draws, holding no world state of its own.

- `render(farm, camOffsetX, camOffsetY, tileSize)` — draws each tile as a
  coloured rectangle with grid lines, frustum-culled to the viewport. Returns a
  `TileRenderResult` whose `hoveredTile` is the `TileCoord` under the mouse (if
  any), computed from the mouse position and camera offset.
- `colorForTileType(TileType)` / `tileTypeName(TileType)` — static lookups for a
  tile type's draw colour and display name.

Constants: `kDefaultTileSize` (32 px) and `kGridLineThickness` (1 px).

## How it is used

Screens that show the 2D farm view call `render()` once per frame with the
current `FarmState` and camera pan, then use the returned hovered tile to drive
selection/placement UI.
