#include "game/presentation/world/TileRenderer.hpp"
#include "engine/graphics/Render.hpp"

namespace biofuel::game::presentation::world {

namespace {

[[nodiscard]] constexpr Color toColor(const gameplay::TileRenderColor color) noexcept {
    return Color{color.r, color.g, color.b, color.a};
}

} // namespace

// ---------------------------------------------------------------------------
// Colour lookup
// ---------------------------------------------------------------------------

Color TileRenderer::colorForTileType(const gameplay::TileType type) noexcept {
    if (!gameplay::isKnownTileType(type)) {
        return Color{128, 128, 128, 255};
    }
    return toColor(gameplay::tileRenderColor(type));
}

const char* TileRenderer::tileTypeName(const gameplay::TileType type) noexcept {
    if (!gameplay::isKnownTileType(type)) {
        return "Unknown";
    }
    return gameplay::tileTypeName(type).data();
}

// ---------------------------------------------------------------------------
// Render
// ---------------------------------------------------------------------------

TileRenderResult TileRenderer::render(
    const gameplay::FarmState& farm,
    const f32 camOffsetX, const f32 camOffsetY,
    const i32 tileSize) const
{
    using namespace ::biofuel::engine::graphics;

    const i32 screenW = Renderer::screenWidth();
    const i32 screenH = Renderer::screenHeight();
    const usize gridW = farm.width();
    const usize gridH = farm.height();
    const f32 tileSizeF = static_cast<f32>(tileSize);

    // ---- Draw filled tiles (frustum culled) ----
    for (usize ty = 0; ty < gridH; ++ty) {
        const f32 screenY = static_cast<f32>(ty) * tileSizeF - camOffsetY;

        // Frustum cull entire row if it's off-screen vertically
        if (screenY + tileSizeF < 0.0f) continue;
        if (screenY > static_cast<f32>(screenH)) break; // rows are monotonic increasing

        for (usize tx = 0; tx < gridW; ++tx) {
            const f32 screenX = static_cast<f32>(tx) * tileSizeF - camOffsetX;

            // Frustum cull horizontally
            if (screenX + tileSizeF < 0.0f) continue;
            if (screenX > static_cast<f32>(screenW)) continue;

            const gameplay::Tile* tile = farm.tileAt(tx, ty);
            const Color fillColor = tile
                ? colorForTileType(tile->type)
                : Color{64, 64, 64, 255};

            const i32 sx = static_cast<i32>(screenX);
            const i32 sy = static_cast<i32>(screenY);

            Renderer::drawRect(sx, sy, tileSize, tileSize, fillColor);
            Renderer::drawRectLines(sx, sy, tileSize, tileSize,
                                    Color{48, 48, 48, 255});
        }
    }

    // ---- Hover detection ----
    TileRenderResult result{};

    const i32 mouseX = GetMouseX();
    const i32 mouseY = GetMouseY();

    // Only detect hover if mouse is within the screen
    if (mouseX >= 0 && mouseX < screenW && mouseY >= 0 && mouseY < screenH) {
        const f32 worldX = static_cast<f32>(mouseX) + camOffsetX;
        const f32 worldY = static_cast<f32>(mouseY) + camOffsetY;

        const i32 tileX = static_cast<i32>(worldX / tileSizeF);
        const i32 tileY = static_cast<i32>(worldY / tileSizeF);

        if (tileX >= 0 && static_cast<usize>(tileX) < gridW &&
            tileY >= 0 && static_cast<usize>(tileY) < gridH) {
            result.hoveredTile = engine::core::units::TileCoord{tileX, tileY};

            // Highlight the hovered tile
            const f32 hlX = static_cast<f32>(tileX) * tileSizeF - camOffsetX;
            const f32 hlY = static_cast<f32>(tileY) * tileSizeF - camOffsetY;
            Renderer::drawRectLines(
                static_cast<i32>(hlX), static_cast<i32>(hlY),
                tileSize, tileSize,
                Color{255, 255, 100, 220}); // bright yellow highlight
        }
    }

    return result;
}

} // namespace biofuel::game::presentation::world
