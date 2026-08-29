#include "engine/game/GameWorldService.hpp"

#include "biofuel_game_ecs_cxx/lib.h"
#include <algorithm>
#include <array>

namespace biofuel::engine::gameworld {

namespace {

// A fixed cap on how many objects one frame's batch read-back can return --
// mirrors physics/'s own truncation-safe body_poses_3d pattern: read_game_objects
// never writes past this buffer's length, so a level that somehow grew past
// this would silently drop the extra objects rather than crash. Real usage
// today is 17 (the ported ExplorationLevel geometry) plus, from the next
// phase, one demo entity -- this leaves generous headroom.
constexpr usize MAX_GAME_OBJECTS = 256;

[[nodiscard]] Vector3 toVector3(const BridgeVec3& value) noexcept {
    return Vector3{value.x, value.y, value.z};
}

[[nodiscard]] Quaternion toQuaternion(const BridgeQuat& value) noexcept {
    return Quaternion{value.x, value.y, value.z, value.w};
}

[[nodiscard]] Color toColor(const std::array<u8, 4>& rgba) noexcept {
    return Color{rgba[0], rgba[1], rgba[2], rgba[3]};
}

} // namespace

struct GameWorldService::Impl {
    explicit Impl(rust::Box<GameWorld> w) noexcept
        : world(std::move(w)) {}

    rust::Box<GameWorld> world;
};

GameWorldService& GameWorldService::instance() noexcept {
    static GameWorldService service{};
    return service;
}

GameWorldService::~GameWorldService() noexcept {
    if (m_initialized) {
        shutdown();
    }
}

void GameWorldService::init() {
    if (m_initialized) {
        return;
    }

    m_impl = std::make_unique<Impl>(new_game_world());
    m_objects.reserve(MAX_GAME_OBJECTS);
    m_initialized = true;
}

void GameWorldService::shutdown() noexcept {
    if (!m_initialized) {
        return;
    }

    m_objects.clear();
    m_impl.reset();
    m_initialized = false;
}

void GameWorldService::update(const f32 dt) {
    if (!m_initialized) {
        return;
    }

    step_game(*m_impl->world, dt);

    std::vector<BridgeGameObject> batch(MAX_GAME_OBJECTS);
    const rust::Slice<BridgeGameObject> batchSlice{batch.data(), batch.size()};
    const u64 count = read_game_objects(*m_impl->world, batchSlice);

    m_objects.clear();
    for (u64 i = 0; i < count && i < MAX_GAME_OBJECTS; ++i) {
        const BridgeGameObject& object = batch[i];
        m_objects.push_back(GameObjectSnapshot{
            .kind = static_cast<GameObjectKind>(object.entity_kind),
            .position = toVector3(object.position),
            .rotation = toQuaternion(object.rotation),
            .halfExtents = toVector3(object.half_extents),
            .color = toColor(object.color_rgba),
        });
    }
}

} // namespace biofuel::engine::gameworld
