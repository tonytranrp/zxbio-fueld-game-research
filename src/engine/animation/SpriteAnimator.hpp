#pragma once

#include "engine/core/Types.hpp"
#include <raylib.h>
#include <string>
#include <unordered_map>

namespace biofuel::engine::animation {

// ---------------------------------------------------------------------------
// Direction enum for 8-directional movement
// ---------------------------------------------------------------------------
enum class SpriteDirection : u8 {
    Down,
    DownLeft,
    Left,
    UpLeft,
    Up,
    UpRight,
    Right,
    DownRight,
};

// ---------------------------------------------------------------------------
// Behavior state for the cat
// ---------------------------------------------------------------------------
enum class SpriteState : u8 {
    Idle,        // Static awake frame
    Walking,     // 2-frame walk cycle based on direction
    Scratching,  // 2-frame scratch
    Washing,     // 2-frame wash
    Yawning,     // 2-frame yawn
    Sleeping,    // 2-frame sleep
};

// ---------------------------------------------------------------------------
// A sprite animation that toggles between two frames
// ---------------------------------------------------------------------------
struct SpriteFrames {
    Texture2D* frame1 = nullptr;  // Displayed when tickCounter < ticksPerFrame
    Texture2D* frame2 = nullptr;  // Displayed when tickCounter >= ticksPerFrame
    u32 ticksPerFrame = 8;         // Ticks before switching to frame2
    u32 maxTicks = 16;             // Total ticks per cycle (counter resets after this)
};

// ---------------------------------------------------------------------------
// Main animator class
// ---------------------------------------------------------------------------
class SpriteAnimator {
public:
    SpriteAnimator() = default;
    ~SpriteAnimator();

    // Load a sprite from a PNG file path, keyed by name
    bool loadSprite(const std::string& name, const std::string& filePath);

    // Set up animation frames for a state+direction combination
    void setFrames(SpriteState state, SpriteDirection dir,
                   const std::string& frame1Name, const std::string& frame2Name,
                   u32 ticksPerFrame = 8, u32 maxTicks = 16);

    // Set a single-frame state (like idle/awake)
    void setStaticFrame(SpriteState state, const std::string& frameName);

    // Update animation state - call once per game tick
    void tick();

    // Set current state and direction
    void setState(SpriteState state);
    void setDirection(SpriteDirection dir);

    // Get the current frame texture (or nullptr if not loaded)
    [[nodiscard]] Texture2D* currentFrame() const;

    [[nodiscard]] SpriteState state() const { return m_state; }
    [[nodiscard]] SpriteDirection direction() const { return m_direction; }

private:
    std::unordered_map<std::string, Texture2D> m_sprites;
    // Key: (state, direction) -> SpriteFrames
    std::unordered_map<u32, SpriteFrames> m_frames;
    Texture2D* m_staticFrames[6] = {};  // One per SpriteState

    SpriteState m_state = SpriteState::Idle;
    SpriteDirection m_direction = SpriteDirection::Down;

    u32 m_tickCounter = 0;

    [[nodiscard]] static u32 keyFor(SpriteState s, SpriteDirection d) noexcept {
        return (static_cast<u8>(s) << 4) | static_cast<u8>(d);
    }
};

}  // namespace biofuel::engine::animation
