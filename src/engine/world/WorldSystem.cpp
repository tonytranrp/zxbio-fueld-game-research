#include "engine/world/WorldSystem.hpp"
#include <algorithm>
#include <cassert>

namespace biofuel::engine::world {

// Precomputed stride: width × height in tiles, used by hot getTile/setTile paths.
// Avoids re-multiplying on every bounds check.
namespace {
constexpr i32 kChunkStride = Chunk::SIZE; // tiles per chunk axis
} // namespace

// =============================================================================
// Destructor
// =============================================================================

WorldSystem::~WorldSystem() noexcept {
    shutdown();
}

// =============================================================================
// Lifecycle
// =============================================================================

void WorldSystem::init(const WorldConfig& config) {
    if (m_initialized) {
        return;
    }
    [[maybe_unused]] const WorldID defaultWorld = createWorld(config);
    (void)defaultWorld;
    m_initialized = true;
}

void WorldSystem::shutdown() noexcept {
    m_worlds.clear();
    m_currentWorld = kInvalidWorldID;
    m_nextWorldID = kDefaultWorldID.value;
    m_initialized = false;
}

// =============================================================================
// World lifecycle
// =============================================================================

WorldID WorldSystem::createWorld(const WorldConfig& config) {
    const WorldID id = allocateWorldID();
    auto instance = std::make_unique<WorldInstance>(config, id);
    m_worlds[id] = std::move(instance);
    m_currentWorld = id;
    return id;
}

bool WorldSystem::destroyWorld(const WorldID id) {
    if (!worldExists(id)) {
        return false;
    }
    // Refuse to destroy the last remaining world
    if (m_worlds.size() <= 1U) {
        return false;
    }

    m_worlds.erase(id);

    // If we destroyed the current world, pick another
    if (m_currentWorld == id) {
        m_currentWorld = m_worlds.begin()->first;
    }
    return true;
}

bool WorldSystem::setCurrentWorld(const WorldID id) noexcept {
    if (!worldExists(id)) {
        return false;
    }
    m_currentWorld = id;
    return true;
}

bool WorldSystem::worldExists(const WorldID id) const noexcept {
    return m_worlds.find(id) != m_worlds.end();
}

// =============================================================================
// World config
// =============================================================================

const WorldConfig* WorldSystem::worldConfig() const noexcept {
    const WorldInstance* world = currentWorldInstance();
    if (!world) {
        return nullptr;
    }
    return &world->config;
}

const WorldConfig* WorldSystem::worldConfig(const WorldID id) const noexcept {
    const WorldInstance* world = getWorld(id);
    if (!world) {
        return nullptr;
    }
    return &world->config;
}

// =============================================================================
// Tile access (current world)
// =============================================================================

const TileData* WorldSystem::getTile(const TileCoord coord) const noexcept {
    return getTile(m_currentWorld, coord);
}

TileData* WorldSystem::getTile(const TileCoord coord) noexcept {
    return getTile(m_currentWorld, coord);
}

bool WorldSystem::setTile(const TileCoord coord, const TileData& data) noexcept {
    return setTile(m_currentWorld, coord, data);
}

// =============================================================================
// Tile access (specific world)
// =============================================================================

const TileData* WorldSystem::getTile(const WorldID id, const TileCoord coord) const noexcept {
    if (!inBounds(id, coord)) {
        return nullptr;
    }
    const ChunkCoord cCoord = tileToChunk(coord);
    const WorldInstance* world = getWorld(id);
    if (!world) {
        return nullptr;
    }
    auto it = world->chunks.find(cCoord);
    if (it == world->chunks.end()) {
        return nullptr;
    }
    const i32 tx = tileLocalX(coord);
    const i32 ty = tileLocalY(coord);
    return &it->second->tileAt(tx, ty);
}

TileData* WorldSystem::getTile(const WorldID id, const TileCoord coord) noexcept {
    if (!inBounds(id, coord)) {
        return nullptr;
    }
    const ChunkCoord cCoord = tileToChunk(coord);
    WorldInstance* world = getWorld(id);
    if (!world) {
        return nullptr;
    }
    auto it = world->chunks.find(cCoord);
    if (it == world->chunks.end()) {
        return nullptr;
    }
    const i32 tx = tileLocalX(coord);
    const i32 ty = tileLocalY(coord);
    return &it->second->tileAt(tx, ty);
}

bool WorldSystem::setTile(const WorldID id, const TileCoord coord, const TileData& data) noexcept {
    if (!inBounds(id, coord)) {
        return false;
    }
    Chunk* chunk = getChunkOrLoad(id, tileToChunk(coord));
    if (!chunk) {
        return false;
    }
    const i32 tx = tileLocalX(coord);
    const i32 ty = tileLocalY(coord);
    chunk->tileAt(tx, ty) = data;
    chunk->dirty = true;
    return true;
}

// =============================================================================
// Chunk access (current world)
// =============================================================================

const Chunk* WorldSystem::getChunk(const ChunkCoord coord) const noexcept {
    return getChunk(m_currentWorld, coord);
}

Chunk* WorldSystem::getChunk(const ChunkCoord coord) noexcept {
    return getChunk(m_currentWorld, coord);
}

Chunk* WorldSystem::getChunkOrLoad(const ChunkCoord coord) noexcept {
    return getChunkOrLoad(m_currentWorld, coord);
}

// =============================================================================
// Chunk access (specific world)
// =============================================================================

const Chunk* WorldSystem::getChunk(const WorldID id, const ChunkCoord coord) const noexcept {
    const WorldInstance* world = getWorld(id);
    if (!world) {
        return nullptr;
    }
    auto it = world->chunks.find(coord);
    if (it == world->chunks.end()) {
        return nullptr;
    }
    return it->second.get();
}

Chunk* WorldSystem::getChunk(const WorldID id, const ChunkCoord coord) noexcept {
    WorldInstance* world = getWorld(id);
    if (!world) {
        return nullptr;
    }
    auto it = world->chunks.find(coord);
    if (it == world->chunks.end()) {
        return nullptr;
    }
    return it->second.get();
}

Chunk* WorldSystem::getChunkOrLoad(const WorldID id, const ChunkCoord coord) noexcept {
    WorldInstance* world = getWorld(id);
    if (!world) {
        return nullptr;
    }
    auto it = world->chunks.find(coord);
    if (it != world->chunks.end()) {
        return it->second.get();
    }

    // Validate chunk coordinate against world bounds
    if (coord.cx < 0 || coord.cy < 0
        || coord.cx >= world->config.chunksX()
        || coord.cy >= world->config.chunksY()) {
        return nullptr;
    }

    auto chunk = std::make_unique<Chunk>();
    Chunk* result = chunk.get();
    world->chunks[coord] = std::move(chunk);
    return result;
}

// =============================================================================
// Bounds checking
// =============================================================================

bool WorldSystem::inBounds(const TileCoord coord) const noexcept {
    return inBounds(m_currentWorld, coord);
}

bool WorldSystem::inBounds(const WorldID id, const TileCoord coord) const noexcept {
    const WorldInstance* world = getWorld(id);
    if (!world) {
        return false;
    }
    return coord.x >= 0 && coord.y >= 0
        && coord.x < world->config.widthTiles
        && coord.y < world->config.heightTiles;
}

// =============================================================================
// Private helpers
// =============================================================================

BIOFUEL_FORCE_INLINE WorldSystem::WorldInstance* WorldSystem::getWorld(const WorldID id) noexcept {
    auto it = m_worlds.find(id);
    if (it == m_worlds.end()) {
        return nullptr;
    }
    return it->second.get();
}

BIOFUEL_FORCE_INLINE const WorldSystem::WorldInstance* WorldSystem::getWorld(const WorldID id) const noexcept {
    auto it = m_worlds.find(id);
    if (it == m_worlds.end()) {
        return nullptr;
    }
    return it->second.get();
}

BIOFUEL_FORCE_INLINE WorldSystem::WorldInstance* WorldSystem::currentWorldInstance() noexcept {
    return getWorld(m_currentWorld);
}

BIOFUEL_FORCE_INLINE const WorldSystem::WorldInstance* WorldSystem::currentWorldInstance() const noexcept {
    return getWorld(m_currentWorld);
}

WorldID WorldSystem::allocateWorldID() noexcept {
    // Find the next available ID, wrapping around if needed
    const u32 start = m_nextWorldID;
    do {
        const WorldID candidate{m_nextWorldID};
        ++m_nextWorldID;
        if (m_nextWorldID == kInvalidWorldID.value) {
            m_nextWorldID = kDefaultWorldID.value;
        }
        if (!worldExists(candidate)) {
            return candidate;
        }
    } while (m_nextWorldID != start);
    // All IDs exhausted (shouldn't happen with kMaxWorlds = 32)
    assert(false && "WorldSystem: all WorldIDs exhausted");
    return kInvalidWorldID;
}

} // namespace biofuel::engine::world
