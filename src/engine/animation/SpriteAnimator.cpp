#include "engine/animation/SpriteAnimator.hpp"

namespace biofuel::engine::animation {

// ---------------------------------------------------------------------------
// Destructor — unload all textures
// ---------------------------------------------------------------------------
SpriteAnimator::~SpriteAnimator() {
    for (auto& [name, texture] : m_sprites) {
        UnloadTexture(texture);
    }
    m_sprites.clear();
}

// ---------------------------------------------------------------------------
// Load a sprite from a PNG file path, keyed by name
// ---------------------------------------------------------------------------
bool SpriteAnimator::loadSprite(const std::string& name, const std::string& filePath) {
    Texture2D texture = LoadTexture(filePath.c_str());
    if (texture.id == 0) {
        return false;
    }
    m_sprites[name] = texture;
    return true;
}

// ---------------------------------------------------------------------------
// Set up animation frames for a state+direction combination
// ---------------------------------------------------------------------------
void SpriteAnimator::setFrames(SpriteState state, SpriteDirection dir,
                               const std::string& frame1Name, const std::string& frame2Name,
                               u32 ticksPerFrame, u32 maxTicks) {
    auto it1 = m_sprites.find(frame1Name);
    auto it2 = m_sprites.find(frame2Name);

    SpriteFrames frames;
    frames.ticksPerFrame = ticksPerFrame;
    frames.maxTicks = maxTicks;

    if (it1 != m_sprites.end()) {
        frames.frame1 = &it1->second;
    }
    if (it2 != m_sprites.end()) {
        frames.frame2 = &it2->second;
    }

    m_frames[keyFor(state, dir)] = frames;
}

// ---------------------------------------------------------------------------
// Set a single-frame state (like idle/awake)
// ---------------------------------------------------------------------------
void SpriteAnimator::setStaticFrame(SpriteState state, const std::string& frameName) {
    auto it = m_sprites.find(frameName);
    auto idx = static_cast<usize>(static_cast<u8>(state));
    if (it != m_sprites.end() && idx < 6) {
        m_staticFrames[idx] = &it->second;
    }
}

// ---------------------------------------------------------------------------
// Update animation state — call once per game tick
// ---------------------------------------------------------------------------
void SpriteAnimator::tick() {
    ++m_tickCounter;

    // Determine the maxTicks for the current animation (if any)
    u32 maxTicks = 16;  // default fallback
    u32 key = keyFor(m_state, m_direction);
    auto it = m_frames.find(key);

    // For non-walking states, fall back to Down direction if exact match not found
    if (it == m_frames.end() && m_state != SpriteState::Walking) {
        u32 fallbackKey = keyFor(m_state, SpriteDirection::Down);
        it = m_frames.find(fallbackKey);
    }

    if (it != m_frames.end()) {
        maxTicks = it->second.maxTicks;
    }

    if (maxTicks > 0 && m_tickCounter >= maxTicks) {
        m_tickCounter = 0;
    }
}

// ---------------------------------------------------------------------------
// Set current state
// ---------------------------------------------------------------------------
void SpriteAnimator::setState(SpriteState state) {
    if (m_state != state) {
        m_state = state;
        m_tickCounter = 0;  // Reset tick on state change
    }
}

// ---------------------------------------------------------------------------
// Set current direction
// ---------------------------------------------------------------------------
void SpriteAnimator::setDirection(SpriteDirection dir) {
    if (m_direction != dir) {
        m_direction = dir;
        if (m_state == SpriteState::Walking) {
            m_tickCounter = 0;  // Reset tick on direction change while walking
        }
    }
}

// ---------------------------------------------------------------------------
// Get the current frame texture (or nullptr if not loaded)
// ---------------------------------------------------------------------------
Texture2D* SpriteAnimator::currentFrame() const {
    // Try animated frames first
    u32 key = keyFor(m_state, m_direction);
    auto it = m_frames.find(key);

    // For non-walking states, fall back to Down direction if exact match not found
    if (it == m_frames.end() && m_state != SpriteState::Walking) {
        u32 fallbackKey = keyFor(m_state, SpriteDirection::Down);
        it = m_frames.find(fallbackKey);
    }

    if (it != m_frames.end()) {
        const SpriteFrames& frames = it->second;
        if (m_tickCounter < frames.ticksPerFrame) {
            return frames.frame1;
        }
        return frames.frame2;
    }

    // Fall back to static frame
    auto idx = static_cast<usize>(static_cast<u8>(m_state));
    if (idx < 6) {
        return m_staticFrames[idx];
    }
    return nullptr;
}

}  // namespace biofuel::engine::animation
