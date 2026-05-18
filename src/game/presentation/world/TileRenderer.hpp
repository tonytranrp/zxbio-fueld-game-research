#pragma once

#include "engine/core/Types.hpp"
#include "engine/core/units/EngineUnits.hpp"
#include "game/gameplay/FarmState.hpp"
#include <raylib.h>
#include <optional>
#include <string>

namespace biofuel::game::presentation::world {

// ---------------------------------------------------------------------------
// TileRenderResult — output of one TileRenderer::render() frame
// ---------------------------------------------------------------------------
struct TileRenderResult {
    /// Tile coordinate the mouse is currently hovering over, if any.
    std::optional<engine::core::units::TileCoord> hoveredTile;
};

// ---------------------------------------------------------------------------
// TileRenderer — renders the FarmState tile grid as coloured rectangles.
//
// Frustum culling: only tiles whose screen-space rectangle overlaps the
// viewport are drawn.  Grid lines are drawn between every tile.
//
// Hover detection: converts the current raylib mouse position into tile
// coordinates accounting for the camera offset, and checks bounds.
// ---------------------------------------------------------------------------
class TileRenderer {
public:
    static constexpr i32 kDefaultTileSize = 32;
    static constexpr i32 kGridLineThickness = 1;

    // ---- Colour lookup ----
    [[nodiscard]] static Color colorForTileType(gameplay::TileType type) noexcept;
    [[nodiscard]] static const char* tileTypeName(gameplay::TileType type) noexcept;

    // ---- Per-frame render ----
    //
    // farm        — the FarmState whose tile grid is drawn.
    // camOffsetX  — camera pan X in screen pixels (positive scrolls right).
    // camOffsetY  — camera pan Y in screen pixels (positive scrolls down).
    // tileSize    — width/height of each tile in screen pixels.
    //
    // Returns the tile currently under the mouse cursor (if any).
    [[nodiscard]] TileRenderResult render(
        const gameplay::FarmState& farm,
        f32 camOffsetX, f32 camOffsetY,
        i32 tileSize = kDefaultTileSize) const;
};

} // namespace biofuel::game::presentation::world
