#pragma once

#include "engine/core/Types.hpp"
#include "engine/world/TerrainGenerator.hpp"
#include <raylib.h>
#include <vector>

namespace biofuel::engine::world {

// =============================================================================
// VoxelTile — a single cuboid in the 3D world. Position is the center of the
// cuboid; size.y is the full height. The tile sits on the XZ plane.
// =============================================================================
struct VoxelTile {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 size{1.0f, 1.0f, 1.0f};
    Color color{200, 200, 200, 255};
    bool visible = true;
};

// =============================================================================
// VoxelChunk — a square sub-region of the world grid. Tiles are stored in
// row-major order within the chunk. The chunk tracks whether its geometry
// needs to be rebuilt.
// =============================================================================
struct VoxelChunk {
    i32 originX = 0;
    i32 originZ = 0;
    i32 size = 0;
    std::vector<VoxelTile> tiles;
    bool dirty = true;

    [[nodiscard]] bool empty() const noexcept { return tiles.empty(); }

    [[nodiscard]] const VoxelTile& at(i32 localX, i32 localZ) const noexcept {
        return tiles[static_cast<usize>(localZ * size + localX)];
    }

    [[nodiscard]] VoxelTile& at(i32 localX, i32 localZ) noexcept {
        return tiles[static_cast<usize>(localZ * size + localX)];
    }
};

// =============================================================================
// VoxelChunkRenderer — manages chunked rendering of world-height cuboids.
//
// Divides the world grid into chunks of CHUNK_SIZE×CHUNK_SIZE tiles. Each
// chunk independently tracks its dirty state. Only chunks within viewDistance
// of the camera are submitted for rendering.
//
// Current render path: DrawCubeV (immediate mode). Upgrade path: instanced
// rendering via DrawMeshInstanced for large worlds.
// =============================================================================
class VoxelChunkRenderer {
public:
    static constexpr i32 kDefaultChunkSize = 16;

    VoxelChunkRenderer() = default;

    // -------------------------------------------------------------------------
    // Initialize the renderer with world dimensions and chunk subdivision.
    // Must be called before populateFromHeightmap or render.
    // -------------------------------------------------------------------------
    void init(i32 worldWidth, i32 worldHeight, i32 chunkSize = kDefaultChunkSize);

    // -------------------------------------------------------------------------
    // Populate all chunks from a heightmap and biome map. Each tile gets a
    // cuboid whose height comes from the HeightmapData and whose color comes
    // from the BiomeMap classification. Tile size on XZ is 1.0 world unit.
    // -------------------------------------------------------------------------
    void populateFromHeightmap(
        const HeightmapData& heightmap,
        const BiomeMap& biomes);

    // -------------------------------------------------------------------------
    // Update chunk visibility based on camera position. Chunks farther than
    // viewDistance from camera are marked invisible.
    // -------------------------------------------------------------------------
    void updateVisibility(const Vector3& cameraPosition, f32 viewDistance);

    // -------------------------------------------------------------------------
    // renderSolid — draws all visible tiles as filled cubes using DrawCubeV.
    // Call between BeginMode3D / EndMode3D.
    // -------------------------------------------------------------------------
    void renderSolid() const;

    // -------------------------------------------------------------------------
    // renderWireframe — draws all visible tiles as wireframe cubes.
    // Call between BeginMode3D / EndMode3D.
    // -------------------------------------------------------------------------
    void renderWireframe() const;

    // -------------------------------------------------------------------------
    // Query
    // -------------------------------------------------------------------------
    [[nodiscard]] i32 worldWidth() const noexcept { return m_worldWidth; }
    [[nodiscard]] i32 worldHeight() const noexcept { return m_worldHeight; }
    [[nodiscard]] i32 chunkSize() const noexcept { return m_chunkSize; }
    [[nodiscard]] i32 chunkCountX() const noexcept { return m_chunksX; }
    [[nodiscard]] i32 chunkCountZ() const noexcept { return m_chunksZ; }
    [[nodiscard]] usize totalChunks() const noexcept { return m_chunks.size(); }
    [[nodiscard]] usize visibleTileCount() const noexcept;

private:
    [[nodiscard]] i32 chunkIndex(i32 cx, i32 cz) const noexcept {
        return cz * m_chunksX + cx;
    }

    [[nodiscard]] bool isChunkInRange(i32 cx, i32 cz,
                                      const Vector3& camPos,
                                      f32 viewDist) const noexcept;

    [[nodiscard]] static Color biomeColor(BiomeMaterial mat) noexcept;

    i32 m_worldWidth = 0;
    i32 m_worldHeight = 0;
    i32 m_chunkSize = kDefaultChunkSize;
    i32 m_chunksX = 0;
    i32 m_chunksZ = 0;
    std::vector<VoxelChunk> m_chunks;
};

} // namespace biofuel::engine::world
