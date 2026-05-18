#include "engine/world/VoxelChunkRenderer.hpp"

#include <algorithm>
#include <cmath>

namespace biofuel::engine::world {

void VoxelChunkRenderer::init(const i32 worldWidth, const i32 worldHeight, const i32 chunkSize) {
    m_worldWidth = worldWidth;
    m_worldHeight = worldHeight;
    m_chunkSize = std::max(chunkSize, 1);

    // Compute chunk grid dimensions
    m_chunksX = (worldWidth + m_chunkSize - 1) / m_chunkSize;
    m_chunksZ = (worldHeight + m_chunkSize - 1) / m_chunkSize;

    const usize totalChunkCount = static_cast<usize>(m_chunksX) * static_cast<usize>(m_chunksZ);
    m_chunks.clear();
    m_chunks.reserve(totalChunkCount);

    for (i32 cz = 0; cz < m_chunksZ; ++cz) {
        for (i32 cx = 0; cx < m_chunksX; ++cx) {
            VoxelChunk chunk;
            chunk.originX = cx * m_chunkSize;
            chunk.originZ = cz * m_chunkSize;
            chunk.size = m_chunkSize;
            m_chunks.push_back(std::move(chunk));
        }
    }
}

void VoxelChunkRenderer::populateFromHeightmap(
    const HeightmapData& heightmap,
    const BiomeMap& biomes)
{
    if (m_chunks.empty()) {
        return;
    }

    for (auto& chunk : m_chunks) {
        const i32 endX = std::min(chunk.originX + m_chunkSize, m_worldWidth);
        const i32 endZ = std::min(chunk.originZ + m_chunkSize, m_worldHeight);
        const i32 localW = endX - chunk.originX;
        const i32 localH = endZ - chunk.originZ;

        chunk.tiles.clear();
        chunk.tiles.reserve(static_cast<usize>(localW) * static_cast<usize>(localH));

        for (i32 lz = 0; lz < localH; ++lz) {
            for (i32 lx = 0; lx < localW; ++lx) {
                const i32 wx = chunk.originX + lx;
                const i32 wz = chunk.originZ + lz;

                const f32 h = heightmap.at(wx, wz);
                const BiomeMaterial mat = biomes.at(wx, wz).material;

                VoxelTile tile;
                tile.position = {
                    static_cast<f32>(wx) + 0.5f,
                    h * 0.5f,
                    static_cast<f32>(wz) + 0.5f,
                };
                tile.size = {1.0f, h, 1.0f};
                tile.color = biomeColor(mat);
                tile.visible = true;
                chunk.tiles.push_back(tile);
            }
        }
        chunk.size = m_chunkSize; // keep nominal size; actual tile count may differ at edges
        chunk.dirty = false;
    }
}

void VoxelChunkRenderer::updateVisibility(const Vector3& cameraPosition, const f32 viewDistance) {
    for (auto& chunk : m_chunks) {
        const i32 cx = chunk.originX / m_chunkSize;
        const i32 cz = chunk.originZ / m_chunkSize;
        const bool inRange = isChunkInRange(cx, cz, cameraPosition, viewDistance);

        for (auto& tile : chunk.tiles) {
            tile.visible = inRange;
        }
    }
}

void VoxelChunkRenderer::renderSolid() const {
    for (const auto& chunk : m_chunks) {
        for (const auto& tile : chunk.tiles) {
            if (!tile.visible) {
                continue;
            }
            DrawCubeV(tile.position, tile.size, tile.color);
        }
    }
}

void VoxelChunkRenderer::renderWireframe() const {
    for (const auto& chunk : m_chunks) {
        for (const auto& tile : chunk.tiles) {
            if (!tile.visible) {
                continue;
            }
            DrawCubeWiresV(tile.position, tile.size, DARKGRAY);
        }
    }
}

usize VoxelChunkRenderer::visibleTileCount() const noexcept {
    usize count = 0;
    for (const auto& chunk : m_chunks) {
        for (const auto& tile : chunk.tiles) {
            if (tile.visible) {
                ++count;
            }
        }
    }
    return count;
}

bool VoxelChunkRenderer::isChunkInRange(
    const i32 cx, const i32 cz,
    const Vector3& camPos,
    const f32 viewDist) const noexcept
{
    // Compute the chunk center in world space
    const f32 centerX = (static_cast<f32>(cx) + 0.5f) * static_cast<f32>(m_chunkSize);
    const f32 centerZ = (static_cast<f32>(cz) + 0.5f) * static_cast<f32>(m_chunkSize);

    const f32 dx = centerX - camPos.x;
    const f32 dz = centerZ - camPos.z;
    const f32 distSq = dx * dx + dz * dz;

    return distSq <= (viewDist * viewDist);
}

Color VoxelChunkRenderer::biomeColor(const BiomeMaterial mat) noexcept {
    switch (mat) {
    case BiomeMaterial::DeepWater:    return Color{ 18,  72, 140, 255};
    case BiomeMaterial::ShallowWater: return Color{ 40, 120, 180, 255};
    case BiomeMaterial::Sand:         return Color{238, 214, 175, 255};
    case BiomeMaterial::Grass:        return Color{ 76, 153,   0, 255};
    case BiomeMaterial::Forest:       return Color{ 34, 102,  34, 255};
    case BiomeMaterial::Dirt:         return Color{139,  90,  43, 255};
    case BiomeMaterial::Stone:        return Color{128, 128, 128, 255};
    case BiomeMaterial::Snow:         return Color{245, 245, 250, 255};
    }
    return Color{200, 200, 200, 255};
}

} // namespace biofuel::engine::world
