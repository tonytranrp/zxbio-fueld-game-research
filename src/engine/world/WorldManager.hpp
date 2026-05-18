#pragma once

#include "engine/world/WorldTypes.hpp"
#include "engine/world/WorldSystem.hpp"
#include <string_view>

namespace biofuel::engine::world {

// =============================================================================
// WorldManager — high-level world lifecycle management
//
// Provides create/destroy/load/save semantics on top of WorldSystem.
// Supports multiple concurrent worlds accessed by WorldID. Uses JSON
// for persistence of world config and tile data.
//
// WorldManager is a companion to WorldSystem, not a separate service.
// Access it via WorldManager{worldSystem} or through Runtime conveniences.
// =============================================================================

class WorldManager {
public:
    explicit WorldManager(WorldSystem& system) noexcept
        : m_system(&system) {}

    WorldManager(const WorldManager&) = delete;
    WorldManager& operator=(const WorldManager&) = delete;
    WorldManager(WorldManager&&) = default;
    WorldManager& operator=(WorldManager&&) = default;

    // --- World creation / destruction ---

    /// Create a new world with the given config. Becomes the current world.
    /// Returns a unique WorldID.
    [[nodiscard]] WorldID createWorld(const WorldConfig& config);

    /// Destroy a world and all its chunks. Cannot destroy the last world.
    /// Returns false if the world doesn't exist or is the last remaining world.
    bool destroyWorld(WorldID id);

    /// Set the active/current world for operations.
    /// Returns false if the world doesn't exist.
    bool setCurrentWorld(WorldID id) noexcept;

    /// Get the currently active world ID.
    [[nodiscard]] WorldID currentWorld() const noexcept;

    /// Check if a world exists.
    [[nodiscard]] bool worldExists(WorldID id) const noexcept;

    /// Get the number of active worlds.
    [[nodiscard]] usize worldCount() const noexcept;

    // --- Persistence ---

    /// Load a world from a JSON file. Creates a new world with the saved config
    /// and tile data. Returns the new WorldID, or kInvalidWorldID on failure.
    [[nodiscard]] WorldID loadWorld(const std::string_view path);

    /// Save a world to a JSON file. Writes world config and all non-default
    /// tile data. Returns true on success.
    bool saveWorld(WorldID id, const std::string_view path) const;

    /// Save the current world to a JSON file.
    bool saveCurrentWorld(const std::string_view path) const;

private:
    WorldSystem* m_system = nullptr;
};

} // namespace biofuel::engine::world
