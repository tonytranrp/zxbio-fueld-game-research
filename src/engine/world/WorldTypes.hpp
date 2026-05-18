#pragma once

#include "engine/core/Types.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace biofuel::engine::world {

// =============================================================================
// Enums — all enum class, u8 backing, verify with static_assert
// =============================================================================

enum class WorldDimension : u8 {
    Flat2D,
    Heightmap2_5D,
    Full3D,
};
static_assert(sizeof(WorldDimension) == 1,
              "WorldDimension must be 1 byte (u8 backing)");

enum class TileMaterial : u8 {
    Dirt,
    Stone,
    Grass,
    Water,
    Sand,
    Snow,
    CropBed,
    Concrete,
};
static_assert(sizeof(TileMaterial) == 1,
              "TileMaterial must be 1 byte (u8 backing)");

// =============================================================================
// Coordinate types — simple aggregates, trivially copyable
// =============================================================================

struct TileCoord {
    i32 x = 0;
    i32 y = 0;

    [[nodiscard]] constexpr bool operator==(const TileCoord&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const TileCoord&) const noexcept = default;
    [[nodiscard]] constexpr auto operator<=>(const TileCoord&) const noexcept = default;
};
static_assert(sizeof(TileCoord) == 8,
              "TileCoord must be exactly 8 bytes (i32 + i32)");
static_assert(std::is_trivially_copyable_v<TileCoord>,
              "TileCoord must be trivially copyable");

struct ChunkCoord {
    i32 cx = 0;
    i32 cy = 0;

    [[nodiscard]] constexpr bool operator==(const ChunkCoord&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const ChunkCoord&) const noexcept = default;
    [[nodiscard]] constexpr auto operator<=>(const ChunkCoord&) const noexcept = default;
};
static_assert(sizeof(ChunkCoord) == 8,
              "ChunkCoord must be exactly 8 bytes (i32 + i32)");
static_assert(std::is_trivially_copyable_v<ChunkCoord>,
              "ChunkCoord must be trivially copyable");

struct WorldCoord3D {
    i32 x = 0;
    i32 y = 0;
    i32 z = 0;

    [[nodiscard]] constexpr bool operator==(const WorldCoord3D&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const WorldCoord3D&) const noexcept = default;
    [[nodiscard]] constexpr auto operator<=>(const WorldCoord3D&) const noexcept = default;
};
static_assert(sizeof(WorldCoord3D) == 12,
              "WorldCoord3D must be exactly 12 bytes (i32 + i32 + i32)");
static_assert(std::is_trivially_copyable_v<WorldCoord3D>,
              "WorldCoord3D must be trivially copyable");

// =============================================================================
// Tile data — the atomic unit of world state
// =============================================================================

#pragma pack(push, 1)
struct TileData {
    TileMaterial material = TileMaterial::Dirt;
    i16 height = 0;       // elevation level (for 2.5D/3D)
    u8 flags = 0;         // bit flags: ramp, water, occupied, etc.

    [[nodiscard]] constexpr bool operator==(const TileData&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const TileData&) const noexcept = default;
};
#pragma pack(pop)
static_assert(sizeof(TileData) == 4,
              "TileData must pack to 4 bytes (u8 material + i16 height + u8 flags)");
static_assert(std::is_trivially_copyable_v<TileData>,
              "TileData must be trivially copyable");

// =============================================================================
// Chunk — spatial partition unit, SIZE x SIZE tiles
// =============================================================================

struct Chunk {
    static constexpr i32 SIZE = 16;
    static constexpr i32 TOTAL_TILES = SIZE * SIZE;  // 256

    std::array<std::array<TileData, SIZE>, SIZE> tiles{};
    bool dirty = true;

    [[nodiscard]] constexpr const TileData& tileAt(const i32 tx, const i32 ty) const noexcept {
        return tiles[static_cast<usize>(ty)][static_cast<usize>(tx)];
    }

    [[nodiscard]] constexpr TileData& tileAt(const i32 tx, const i32 ty) noexcept {
        return tiles[static_cast<usize>(ty)][static_cast<usize>(tx)];
    }
};
static_assert(sizeof(Chunk) >= Chunk::TOTAL_TILES * sizeof(TileData),
              "Chunk must be large enough to hold TOTAL_TILES TileData entries");
static_assert(Chunk::SIZE == 16,
              "Chunk::SIZE must be 16 per spatial partition design");

// =============================================================================
// WorldConfig — compile-time-friendly configuration
// =============================================================================

struct WorldConfig {
    i32 widthTiles = 64;
    i32 heightTiles = 64;
    f32 tileSizeMeters = 1.0f;
    WorldDimension dimension = WorldDimension::Heightmap2_5D;

    [[nodiscard]] constexpr bool operator==(const WorldConfig&) const noexcept = default;

    [[nodiscard]] constexpr i32 chunksX() const noexcept {
        return (widthTiles + Chunk::SIZE - 1) / Chunk::SIZE;
    }

    [[nodiscard]] constexpr i32 chunksY() const noexcept {
        return (heightTiles + Chunk::SIZE - 1) / Chunk::SIZE;
    }

    [[nodiscard]] constexpr i32 totalTiles() const noexcept {
        return widthTiles * heightTiles;
    }
};

// =============================================================================
// Compile-time constants — enforced at compile-time, zero runtime overhead
// =============================================================================

/// Maximum number of chunks along X axis (supports world width up to 4096 tiles)
inline constexpr i32 kMaxChunksX = 256;

/// Maximum number of chunks along Y axis (supports world height up to 4096 tiles)
inline constexpr i32 kMaxChunksY = 256;

/// Maximum tiles per world dimension (16 tiles per chunk × 256 chunks)
inline constexpr i32 kMaxTilesPerDimension = kMaxChunksX * Chunk::SIZE;  // 4096

/// Maximum tiles per world (square world)
inline constexpr i32 kMaxTilesPerWorld = kMaxTilesPerDimension * kMaxTilesPerDimension;

// =============================================================================
// Coordinate conversion utilities — constexpr, zero-overhead
// =============================================================================

[[nodiscard]] constexpr ChunkCoord tileToChunk(const TileCoord tile) noexcept {
    return ChunkCoord{
        .cx = tile.x >= 0 ? tile.x / Chunk::SIZE : (tile.x + 1) / Chunk::SIZE - 1,
        .cy = tile.y >= 0 ? tile.y / Chunk::SIZE : (tile.y + 1) / Chunk::SIZE - 1,
    };
}

[[nodiscard]] constexpr i32 tileLocalX(const TileCoord tile) noexcept {
    const i32 rem = tile.x % Chunk::SIZE;
    return rem < 0 ? rem + Chunk::SIZE : rem;
}

[[nodiscard]] constexpr i32 tileLocalY(const TileCoord tile) noexcept {
    const i32 rem = tile.y % Chunk::SIZE;
    return rem < 0 ? rem + Chunk::SIZE : rem;
}

[[nodiscard]] constexpr TileCoord chunkOrigin(const ChunkCoord chunk) noexcept {
    return TileCoord{
        .x = chunk.cx * Chunk::SIZE,
        .y = chunk.cy * Chunk::SIZE,
    };
}

// =============================================================================
// ChunkCoord hashing — for std::unordered_map
// =============================================================================

struct ChunkCoordHash {
    [[nodiscard]] usize operator()(const ChunkCoord c) const noexcept {
        // FNV-1a style 32-bit combine into size_t
        const u32 xBits = static_cast<u32>(c.cx);
        const u32 yBits = static_cast<u32>(c.cy);
        return (static_cast<usize>(xBits) * 0x9E3779B9U)
             ^ (static_cast<usize>(yBits) * 0x85EBCA77U);
    }
};

// =============================================================================
// Compile-time verification bundle
// =============================================================================

namespace detail {

// --- Verify world dimension to chunk math ---
constexpr bool testChunkMath = []() constexpr {
    // Chunk(0,0) covers tiles [0,0] to [15,15]
    constexpr TileCoord origin{0, 0};
    constexpr TileCoord corner{15, 15};
    static_assert(tileToChunk(origin).cx == 0 && tileToChunk(origin).cy == 0,
                  "Tile (0,0) must be in chunk (0,0)");
    static_assert(tileToChunk(corner).cx == 0 && tileToChunk(corner).cy == 0,
                  "Tile (15,15) must be in chunk (0,0)");

    // Chunk(1,0) covers tiles [16,0] to [31,15]
    constexpr TileCoord nextChunk{16, 0};
    static_assert(tileToChunk(nextChunk).cx == 1 && tileToChunk(nextChunk).cy == 0,
                  "Tile (16,0) must be in chunk (1,0)");

    // Negative coordinates
    constexpr TileCoord negTile{-1, -1};
    static_assert(tileToChunk(negTile).cx == -1 && tileToChunk(negTile).cy == -1,
                  "Tile (-1,-1) must be in chunk (-1,-1)");
    static_assert(tileLocalX(negTile) == 15 && tileLocalY(negTile) == 15,
                  "Local coords for (-1,-1) must be (15,15)");

    return true;
}();

// --- Verify WorldConfig helpers ---
constexpr bool testWorldConfig = []() constexpr {
    constexpr WorldConfig cfg64{64, 64, 1.0f, WorldDimension::Heightmap2_5D};
    static_assert(cfg64.chunksX() == 4, "64 tiles / 16 = 4 chunks");
    static_assert(cfg64.chunksY() == 4, "64 tiles / 16 = 4 chunks");
    static_assert(cfg64.totalTiles() == 4096, "64*64 = 4096 tiles");

    constexpr WorldConfig cfgNonMultiple{50, 30, 1.0f, WorldDimension::Flat2D};
    static_assert(cfgNonMultiple.chunksX() == 4, "50 tiles / 16 = ceil(3.125) = 4 chunks");
    static_assert(cfgNonMultiple.chunksY() == 2, "30 tiles / 16 = ceil(1.875) = 2 chunks");

    return true;
}();

// --- Verify type sizes ---
static_assert(sizeof(TileMaterial) == 1, "TileMaterial enum must be 1 byte");
static_assert(std::is_trivially_copyable_v<WorldConfig>, "WorldConfig must be trivially copyable");

// --- Verify kMax constants ---
static_assert(kMaxChunksX == 256, "kMaxChunksX must be 256");
static_assert(kMaxTilesPerDimension == 4096, "16*256 must be 4096");
static_assert(kMaxTilesPerWorld == 4096 * 4096, "Maximum square world tile count");

} // namespace detail

} // namespace biofuel::engine::world
