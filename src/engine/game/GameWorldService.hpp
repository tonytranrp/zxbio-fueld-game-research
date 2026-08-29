#pragma once

#include "engine/core/Types.hpp"
#include "engine/game/GameTypes.hpp"
#include <memory>
#include <span>
#include <vector>

namespace biofuel::engine::gameworld {

// -----------------------------------------------------------------------------
// GameWorldService - owns the headless Bevy ECS + native Rapier physics world
// that drives real game objects (level geometry, player-adjacent entities).
//
// Mirrors BevyRenderService's shape: a singleton with init()/shutdown()/
// update(), privately owning the Rust-side GameWorld, exposing only a read
// accessor (objects()) so callers never touch the FFI boundary directly.
// Unlike BevyRenderService, nothing here renders internally -- update()
// steps Bevy's ECS and native Rapier physics, then reads back a flat batch
// of object transforms for this class's caller (ExplorationScreen) to draw
// with Raylib.
// -----------------------------------------------------------------------------
class GameWorldService {
public:
    [[nodiscard]] static GameWorldService& instance() noexcept;

    void init();
    void shutdown() noexcept;
    void update(f32 dt);

    [[nodiscard]] std::span<const GameObjectSnapshot> objects() const noexcept { return m_objects; }

    GameWorldService(const GameWorldService&) = delete;
    GameWorldService& operator=(const GameWorldService&) = delete;
    GameWorldService(GameWorldService&&) = delete;
    GameWorldService& operator=(GameWorldService&&) = delete;

private:
    GameWorldService() = default;
    ~GameWorldService() noexcept;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::vector<GameObjectSnapshot> m_objects;
    bool m_initialized = false;
};

} // namespace biofuel::engine::gameworld
