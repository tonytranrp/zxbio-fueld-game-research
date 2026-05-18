#pragma once

#include "engine/world/WorldTypes.hpp"
#include <memory>
#include <optional>
#include <unordered_map>

namespace biofuel::engine::world {

// =============================================================================
// WorldID — opaque handle identifying a world instance
// =============================================================================

struct WorldID {
    u32 value = 0U;

    [[nodiscard]] explicit constexpr operator bool() const noexcept { return value != 0U; }
    [[nodiscard]] constexpr bool operator==(const WorldID&) const noexcept = default;
    [[nodiscard]] constexpr bool operator!=(const WorldID&) const noexcept = default;
};
static_assert(sizeof(WorldID) == 4,
              "WorldID must be exactly 4 bytes (u32)");
static_assert(std::is_trivially_copyable_v<WorldID>,
              "WorldID must be trivially copyable");

/// Sentinel value: no world / invalid world handle
inline constexpr WorldID kInvalidWorldID{0U};

/// Default world ID used when no explicit world is specified
inline constexpr WorldID kDefaultWorldID{1U};

// =============================================================================
// WorldID hashing — for std::unordered_map
// =============================================================================

struct WorldIDHash {
    [[nodiscard]] usize operator()(const WorldID id) const noexcept {
        return static_cast<usize>(id.value);
    }
};

// =============================================================================
// WorldSystem — engine service backend for world state management
//
// Owns all world state (chunks, tile data). Registered as a static engine
// service via BIOFUEL_STATIC_SERVICE. Supports multiple concurrent worlds
// identified by WorldID. Provides tile-level and chunk-level access with
// bounds checking and lazy chunk creation.
// =============================================================================

class WorldSystem {
public:
    WorldSystem() = default;
    ~WorldSystem() noexcept;

    WorldSystem(const WorldSystem&) = delete;
    WorldSystem& operator=(const WorldSystem&) = delete;
    WorldSystem(WorldSystem&&) = delete;
    WorldSystem& operator=(WorldSystem&&) = delete;

    // --- Lifecycle ---

    /// Initialize the world system with a default world config.
    /// Creates the default world (kDefaultWorldID) and sets it as current.
    /// Must be called before any tile/chunk operations.
    void init(const WorldConfig& config);

    /// Shutdown the world system, releasing all world instances and chunks.
    void shutdown() noexcept;

    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }

    // --- World lifecycle (delegated from WorldManager) ---

    /// Create a new world with the given config. Returns a unique WorldID.
    /// The new world becomes the current world.
    [[nodiscard]] WorldID createWorld(const WorldConfig& config);

    /// Destroy a world and all its chunks. Cannot destroy the last remaining world.
    /// Returns false if the world doesn't exist or is the last world.
    bool destroyWorld(WorldID id);

    /// Set the active/current world for subsequent tile/chunk operations.
    /// Returns false if the world doesn't exist.
    bool setCurrentWorld(WorldID id) noexcept;

    /// Get the currently active world ID.
    [[nodiscard]] WorldID currentWorld() const noexcept { return m_currentWorld; }

    /// Check if a world exists.
    [[nodiscard]] bool worldExists(WorldID id) const noexcept;

    /// Get the number of active worlds.
    [[nodiscard]] usize worldCount() const noexcept { return m_worlds.size(); }

    // --- World config ---

    /// Get the config for the current world. Returns nullptr if not initialized.
    [[nodiscard]] const WorldConfig* worldConfig() const noexcept;

    /// Get the config for a specific world. Returns nullptr if world doesn't exist.
    [[nodiscard]] const WorldConfig* worldConfig(WorldID id) const noexcept;

    // --- Tile access (current world) ---

    /// Get read-only tile data at world coordinates. Returns nullptr if out of bounds.
    [[nodiscard]] const TileData* getTile(TileCoord coord) const noexcept;

    /// Get mutable tile data at world coordinates. Returns nullptr if out of bounds.
    [[nodiscard]] TileData* getTile(TileCoord coord) noexcept;

    /// Set tile data at world coordinates. Returns false if out of bounds.
    /// Lazily creates the chunk if it doesn't exist.
    bool setTile(TileCoord coord, const TileData& data) noexcept;

    // --- Tile access (specific world) ---

    [[nodiscard]] const TileData* getTile(WorldID id, TileCoord coord) const noexcept;
    [[nodiscard]] TileData* getTile(WorldID id, TileCoord coord) noexcept;
    bool setTile(WorldID id, TileCoord coord, const TileData& data) noexcept;

    // --- Chunk access (current world) ---

    /// Get an existing chunk. Returns nullptr if chunk doesn't exist.
    [[nodiscard]] const Chunk* getChunk(ChunkCoord coord) const noexcept;
    [[nodiscard]] Chunk* getChunk(ChunkCoord coord) noexcept;

    /// Get an existing chunk or lazily create it.
    [[nodiscard]] Chunk* getChunkOrLoad(ChunkCoord coord) noexcept;

    // --- Chunk access (specific world) ---

    [[nodiscard]] const Chunk* getChunk(WorldID id, ChunkCoord coord) const noexcept;
    [[nodiscard]] Chunk* getChunk(WorldID id, ChunkCoord coord) noexcept;
    [[nodiscard]] Chunk* getChunkOrLoad(WorldID id, ChunkCoord coord) noexcept;

    // --- Bounds checking ---

    /// Check if a tile coordinate is within the current world bounds.
    [[nodiscard]] bool inBounds(TileCoord coord) const noexcept;

    /// Check if a tile coordinate is within a specific world's bounds.
    [[nodiscard]] bool inBounds(WorldID id, TileCoord coord) const noexcept;

    // --- Compile-time constants ---

    /// Maximum number of simultaneous worlds supported.
    static constexpr usize kMaxWorlds = 32;

private:
    struct WorldInstance {
        WorldConfig config;
        std::unordered_map<ChunkCoord, std::unique_ptr<Chunk>, ChunkCoordHash> chunks;
        WorldID id;

        explicit WorldInstance(WorldConfig cfg, WorldID worldId)
            : config(cfg), id(worldId) {}
    };

    [[nodiscard]] WorldInstance* getWorld(WorldID id) noexcept;
    [[nodiscard]] const WorldInstance* getWorld(WorldID id) const noexcept;
    [[nodiscard]] WorldInstance* currentWorldInstance() noexcept;
    [[nodiscard]] const WorldInstance* currentWorldInstance() const noexcept;
    [[nodiscard]] WorldID allocateWorldID() noexcept;

    std::unordered_map<WorldID, std::unique_ptr<WorldInstance>, WorldIDHash> m_worlds;
    WorldID m_currentWorld = kInvalidWorldID;
    u32 m_nextWorldID = kDefaultWorldID.value;
    bool m_initialized = false;
};

// =============================================================================
// Compile-time verification
// =============================================================================

namespace detail {

static_assert(WorldSystem::kMaxWorlds >= 1,
              "WorldSystem must support at least 1 world");
static_assert(!static_cast<bool>(kInvalidWorldID),
              "kInvalidWorldID must evaluate to false in boolean context");
static_assert(static_cast<bool>(kDefaultWorldID),
              "kDefaultWorldID must evaluate to true in boolean context");

} // namespace detail

} // namespace biofuel::engine::world
