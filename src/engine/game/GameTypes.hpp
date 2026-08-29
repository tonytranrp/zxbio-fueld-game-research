#pragma once

#include "engine/core/Types.hpp"
#include <raylib.h>

namespace biofuel::engine::gameworld {

enum class GameObjectKind : u8 {
    Static = 0,
    Demo = 1,
};

// One frame's worth of what Raylib needs to draw a single Bevy-owned game
// object -- filled from the Rust-side batch read-back (see
// GameWorldService::objects()), never constructed by hand elsewhere.
struct GameObjectSnapshot {
    GameObjectKind kind = GameObjectKind::Static;
    Vector3 position{0.0f, 0.0f, 0.0f};
    Quaternion rotation{0.0f, 0.0f, 0.0f, 1.0f};
    Vector3 halfExtents{0.5f, 0.5f, 0.5f};
    Color color = GRAY;
};

} // namespace biofuel::engine::gameworld
