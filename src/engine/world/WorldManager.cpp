#include "engine/world/WorldManager.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace biofuel::engine::world {

namespace {

using json = nlohmann::json;

// JSON schema version for forward compatibility
inline constexpr i32 kWorldSaveVersion = 1;

// --- JSON helpers ---

[[nodiscard]] json configToJson(const WorldConfig& config) {
    const char* dimStr = "Heightmap2_5D";
    switch (config.dimension) {
    case WorldDimension::Flat2D:       dimStr = "Flat2D"; break;
    case WorldDimension::Heightmap2_5D: dimStr = "Heightmap2_5D"; break;
    case WorldDimension::Full3D:       dimStr = "Full3D"; break;
    }
    return json{
        {"widthTiles",     config.widthTiles},
        {"heightTiles",    config.heightTiles},
        {"tileSizeMeters", config.tileSizeMeters},
        {"dimension",      dimStr},
    };
}

[[nodiscard]] WorldConfig configFromJson(const json& obj, const WorldConfig& fallback) {
    WorldConfig config = fallback;
    if (obj.contains("widthTiles") && obj["widthTiles"].is_number_integer()) {
        config.widthTiles = obj["widthTiles"].get<i32>();
    }
    if (obj.contains("heightTiles") && obj["heightTiles"].is_number_integer()) {
        config.heightTiles = obj["heightTiles"].get<i32>();
    }
    if (obj.contains("tileSizeMeters") && obj["tileSizeMeters"].is_number()) {
        config.tileSizeMeters = obj["tileSizeMeters"].get<f32>();
    }
    if (obj.contains("dimension") && obj["dimension"].is_string()) {
        const std::string dim = obj["dimension"].get<std::string>();
        if (dim == "Flat2D") {
            config.dimension = WorldDimension::Flat2D;
        } else if (dim == "Heightmap2_5D") {
            config.dimension = WorldDimension::Heightmap2_5D;
        } else if (dim == "Full3D") {
            config.dimension = WorldDimension::Full3D;
        }
    }

    // Clamp to sensible bounds
    config.widthTiles = std::max(1, std::min(config.widthTiles, kMaxTilesPerDimension));
    config.heightTiles = std::max(1, std::min(config.heightTiles, kMaxTilesPerDimension));
    return config;
}

[[nodiscard]] json tileToJson(const TileCoord coord, const TileData& tile) {
    return json{
        {"x", coord.x},
        {"y", coord.y},
        {"material", static_cast<u8>(tile.material)},
        {"height",   tile.height},
        {"flags",    tile.flags},
    };
}

[[nodiscard]] TileData tileFromJson(const json& obj, const TileData& fallback) {
    TileData data = fallback;
    if (obj.contains("material") && obj["material"].is_number_integer()) {
        const u8 mat = obj["material"].get<u8>();
        if (mat < 8U) {
            data.material = static_cast<TileMaterial>(mat);
        }
    }
    if (obj.contains("height") && obj["height"].is_number_integer()) {
        data.height = static_cast<i16>(obj["height"].get<i32>());
    }
    if (obj.contains("flags") && obj["flags"].is_number_integer()) {
        data.flags = static_cast<u8>(obj["flags"].get<u32>());
    }
    return data;
}

[[nodiscard]] json chunkToJson(const ChunkCoord cCoord, const Chunk& chunk) {
    json tilesArray = json::array();
    const TileCoord origin = chunkOrigin(cCoord);
    const TileData defaultTile{};

    for (i32 ty = 0; ty < Chunk::SIZE; ++ty) {
        for (i32 tx = 0; tx < Chunk::SIZE; ++tx) {
            const TileData& tile = chunk.tileAt(tx, ty);
            if (tile == defaultTile) {
                continue; // Skip default tiles to save space
            }
            tilesArray.push_back(tileToJson(TileCoord{origin.x + tx, origin.y + ty}, tile));
        }
    }

    return json{
        {"cx",    cCoord.cx},
        {"cy",    cCoord.cy},
        {"tiles", std::move(tilesArray)},
    };
}

} // namespace

// =============================================================================
// Creation / destruction
// =============================================================================

WorldID WorldManager::createWorld(const WorldConfig& config) {
    return m_system->createWorld(config);
}

bool WorldManager::destroyWorld(const WorldID id) {
    return m_system->destroyWorld(id);
}

bool WorldManager::setCurrentWorld(const WorldID id) noexcept {
    return m_system->setCurrentWorld(id);
}

WorldID WorldManager::currentWorld() const noexcept {
    return m_system->currentWorld();
}

bool WorldManager::worldExists(const WorldID id) const noexcept {
    return m_system->worldExists(id);
}

usize WorldManager::worldCount() const noexcept {
    return m_system->worldCount();
}

// =============================================================================
// Persistence
// =============================================================================

WorldID WorldManager::loadWorld(const std::string_view path) {
    std::ifstream file(path.data(), std::ios::binary);
    if (!file.is_open()) {
        spdlog::error("WorldManager: failed to open world file '{}'", path);
        return kInvalidWorldID;
    }

    json root;
    try {
        file >> root;
    } catch (const json::parse_error& e) {
        spdlog::error("WorldManager: JSON parse error in '{}': {}", path, e.what());
        return kInvalidWorldID;
    }

    if (!root.is_object()) {
        spdlog::error("WorldManager: world file '{}' root must be a JSON object", path);
        return kInvalidWorldID;
    }

    // Version check
    if (root.contains("version") && root["version"].is_number_integer()) {
        const i32 version = root["version"].get<i32>();
        if (version > kWorldSaveVersion) {
            spdlog::warn("WorldManager: world file '{}' has newer version {} (current: {}), attempting load",
                         path, version, kWorldSaveVersion);
        }
    }

    // Parse config
    WorldConfig config;
    const WorldConfig defaultConfig{}; // fallback
    if (root.contains("config") && root["config"].is_object()) {
        config = configFromJson(root["config"], defaultConfig);
    } else {
        config = defaultConfig;
    }

    // Create the world
    const WorldID worldId = m_system->createWorld(config);

    // Load chunks
    if (root.contains("chunks") && root["chunks"].is_array()) {
        const TileData defaultTile{};
        for (const json& chunkObj : root["chunks"]) {
            if (!chunkObj.is_object()) continue;
            if (!chunkObj.contains("cx") || !chunkObj.contains("cy")) continue;
            if (!chunkObj["cx"].is_number_integer() || !chunkObj["cy"].is_number_integer()) continue;

            const i32 cx = chunkObj["cx"].get<i32>();
            const i32 cy = chunkObj["cy"].get<i32>();
            const ChunkCoord cCoord{cx, cy};

            Chunk* chunk = m_system->getChunkOrLoad(worldId, cCoord);
            if (!chunk) continue;

            if (chunkObj.contains("tiles") && chunkObj["tiles"].is_array()) {
                for (const json& tileObj : chunkObj["tiles"]) {
                    if (!tileObj.is_object()) continue;
                    if (!tileObj.contains("x") || !tileObj.contains("y")) continue;
                    if (!tileObj["x"].is_number_integer() || !tileObj["y"].is_number_integer()) continue;

                    const i32 gx = tileObj["x"].get<i32>();
                    const i32 gy = tileObj["y"].get<i32>();
                    const TileCoord tileCoord{gx, gy};

                    if (!m_system->inBounds(worldId, tileCoord)) continue;

                    const TileData tile = tileFromJson(tileObj, defaultTile);
                    m_system->setTile(worldId, tileCoord, tile);
                }
            }
        }
    }

    spdlog::info("WorldManager: loaded world {} from '{}'", worldId.value, path);
    return worldId;
}

bool WorldManager::saveWorld(const WorldID id, const std::string_view path) const {
    const WorldConfig* config = m_system->worldConfig(id);
    if (!config) {
        spdlog::error("WorldManager: cannot save non-existent world {}", id.value);
        return false;
    }

    json root;
    root["version"] = kWorldSaveVersion;
    root["config"] = configToJson(*config);

    json chunksArray = json::array();

    // Iterate over all possible chunks within bounds
    for (i32 cy = 0; cy < config->chunksY(); ++cy) {
        for (i32 cx = 0; cx < config->chunksX(); ++cx) {
            const ChunkCoord cCoord{cx, cy};
            const Chunk* chunk = m_system->getChunk(id, cCoord);
            if (!chunk) continue;
            chunksArray.push_back(chunkToJson(cCoord, *chunk));
        }
    }

    root["chunks"] = std::move(chunksArray);

    std::ofstream file(path.data(), std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        spdlog::error("WorldManager: failed to open '{}' for writing", path);
        return false;
    }

    try {
        file << root.dump(2);
    } catch (const json::type_error& e) {
        spdlog::error("WorldManager: JSON serialization error for world {}: {}", id.value, e.what());
        return false;
    }

    spdlog::info("WorldManager: saved world {} to '{}'", id.value, path);
    return true;
}

bool WorldManager::saveCurrentWorld(const std::string_view path) const {
    return saveWorld(m_system->currentWorld(), path);
}

} // namespace biofuel::engine::world
