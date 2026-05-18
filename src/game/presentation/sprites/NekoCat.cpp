#include "game/presentation/sprites/NekoCat.hpp"

#include <raylib.h>
#include <spdlog/spdlog.h>
#include <array>
#include <cstdio>
#include <cmath>
#include <string_view>

namespace biofuel::game::presentation::sprites {

// ------------------------------------------------------------------------------
// Frame-count lookup
// ------------------------------------------------------------------------------

i32 NekoCat::frameCountForState(const State state) noexcept {
    switch (state) {
    case State::Awake:     return 1;   // awake.png (single frame)
    case State::Walking:   return 2;
    case State::Scratching: return 2;
    case State::Washing:   return 2;
    case State::Yawning:   return 2;
    case State::Sleeping:  return 2;
    }
    return 2;
}

// ------------------------------------------------------------------------------
// Prefix lookup
// ------------------------------------------------------------------------------

const char* NekoCat::directionPrefix(const Direction dir) noexcept {
    switch (dir) {
    case Direction::Up:         return "up";
    case Direction::Down:       return "down";
    case Direction::Left:       return "left";
    case Direction::Right:      return "right";
    case Direction::UpLeft:     return "upleft";
    case Direction::UpRight:    return "upright";
    case Direction::DownLeft:   return "downleft";
    case Direction::DownRight:  return "downright";
    case Direction::Idle:       return "down"; // fallback: render facing down when idle
    }
    return "down";
}

const char* NekoCat::statePrefix(const State state) noexcept {
    switch (state) {
    case State::Walking:   return nullptr;  // Walking uses direction prefix
    case State::Awake:     return "awake";
    case State::Scratching: return "scratch";
    case State::Washing:   return "wash";
    case State::Yawning:   return "yawn";
    case State::Sleeping:  return "sleep";
    }
    return "awake";
}

// ------------------------------------------------------------------------------
// Lifecycle
// ------------------------------------------------------------------------------

NekoCat::~NekoCat() {
    unload();
}

NekoCat::NekoCat(NekoCat&& other) noexcept
    : m_textures(std::move(other.m_textures))
    , m_x(other.m_x)
    , m_y(other.m_y)
    , m_scale(other.m_scale)
    , m_frameTimer(other.m_frameTimer)
    , m_frameDuration(other.m_frameDuration)
    , m_currentFrame(other.m_currentFrame)
    , m_idleTimer(other.m_idleTimer)
    , m_idleStateDuration(other.m_idleStateDuration)
    , m_direction(other.m_direction)
    , m_state(other.m_state)
    , m_loaded(other.m_loaded)
{
    other.m_loaded = false;
}

NekoCat& NekoCat::operator=(NekoCat&& other) noexcept {
    if (this != &other) {
        unload();
        m_textures = std::move(other.m_textures);
        m_x = other.m_x;
        m_y = other.m_y;
        m_scale = other.m_scale;
        m_frameTimer = other.m_frameTimer;
        m_frameDuration = other.m_frameDuration;
        m_currentFrame = other.m_currentFrame;
        m_idleTimer = other.m_idleTimer;
        m_idleStateDuration = other.m_idleStateDuration;
        m_direction = other.m_direction;
        m_state = other.m_state;
        m_loaded = other.m_loaded;
        other.m_loaded = false;
    }
    return *this;
}

void NekoCat::load() {
    if (m_loaded) return;

    // Load directional walking frames: up1/up2, down1/down2, left1/left2, etc.
    static constexpr Direction kDirections[] = {
        Direction::Up, Direction::Down, Direction::Left, Direction::Right,
        Direction::UpLeft, Direction::UpRight, Direction::DownLeft, Direction::DownRight,
    };

    for (const Direction dir : kDirections) {
        const char* prefix = directionPrefix(dir);
        for (i32 frame = 1; frame <= 2; ++frame) {
            std::array<char, 256> path{};
            std::snprintf(path.data(), path.size(), "assets/sprites/neko/%s%d.png", prefix, frame);

            Texture2D tex = LoadTexture(path.data());
            if (tex.id != 0) {
                SetTextureFilter(tex, TEXTURE_FILTER_POINT);
            }

            std::array<char, 32> key{};
            std::snprintf(key.data(), key.size(), "%s%d", prefix, frame);
            m_textures[std::string{key.data()}] = tex;
        }
    }

    // Load state frames (awake, scratch, wash, yawn, sleep)
    static constexpr State kStates[] = {
        State::Awake, State::Scratching, State::Washing,
        State::Yawning, State::Sleeping,
    };

    for (const State st : kStates) {
        const char* prefix = statePrefix(st);
        const i32 count = frameCountForState(st);

        for (i32 frame = 1; frame <= count; ++frame) {
            std::array<char, 256> path{};
            if (count == 1) {
                std::snprintf(path.data(), path.size(), "assets/sprites/neko/%s.png", prefix);
            } else {
                std::snprintf(path.data(), path.size(), "assets/sprites/neko/%s%d.png", prefix, frame);
            }

            Texture2D tex = LoadTexture(path.data());
            if (tex.id != 0) {
                SetTextureFilter(tex, TEXTURE_FILTER_POINT);
            }

            std::array<char, 32> key{};
            if (count == 1) {
                std::snprintf(key.data(), key.size(), "%s", prefix);
            } else {
                std::snprintf(key.data(), key.size(), "%s%d", prefix, frame);
            }
            m_textures[std::string{key.data()}] = tex;
        }
    }

    if (m_textures.empty()) {
        spdlog::warn("NekoCat::load() — no textures loaded from assets/sprites/neko/");
    }

    m_loaded = true;
}

void NekoCat::unload() noexcept {
    if (!m_loaded) return;

    for (auto& [key, tex] : m_textures) {
        if (tex.id != 0) {
            UnloadTexture(tex);
        }
    }
    m_textures.clear();
    m_loaded = false;
}

// ------------------------------------------------------------------------------
// State transition
// ------------------------------------------------------------------------------

void NekoCat::setState(const State state) noexcept {
    if (m_state == state) return;

    m_state = state;
    m_frameTimer = 0.0f;
    m_currentFrame = 1; // 1-based frame numbering for key lookup
    m_cachedTexture = nullptr; // invalidate — new state means different texture
}

// ------------------------------------------------------------------------------
// Texture lookup
// ------------------------------------------------------------------------------

const Texture2D& NekoCat::getTextureForCurrentFrame() const noexcept {
    // Build the key: either direction-based (walking) or state-based
    const char* prefix = (m_state == State::Walking)
        ? directionPrefix(m_direction)
        : statePrefix(m_state);

    const i32 count = (m_state == State::Walking)
        ? 2
        : frameCountForState(m_state);

    std::array<char, 32> key{};
    if (count == 1) {
        std::snprintf(key.data(), key.size(), "%s", prefix);
    } else {
        std::snprintf(key.data(), key.size(), "%s%d", prefix, m_currentFrame);
    }

    // Heterogeneous lookup via TransparentHash — no std::string allocation
    auto it = m_textures.find(std::string_view{key.data()});
    if (it != m_textures.end()) {
        // Cache for subsequent render() calls at the same pose+frame
        m_cachedTexture = &it->second;
        return it->second;
    }

    // Fallback: return the first texture we find, or a static empty texture
    if (!m_textures.empty()) {
        m_cachedTexture = &m_textures.begin()->second;
        return *m_cachedTexture;
    }

    static const Texture2D kEmpty{};
    m_cachedTexture = &kEmpty;
    return kEmpty;
}

// ------------------------------------------------------------------------------
// Animation
// ------------------------------------------------------------------------------

void NekoCat::advanceAnimation(const f32 dt) noexcept {
    m_frameTimer += dt;

    const i32 count = (m_state == State::Walking)
        ? 2
        : frameCountForState(m_state);

    if (count <= 1) {
        m_currentFrame = 1;
        return;
    }

    // Model from crgimenes/neko:
    //   counter 0→max, frame 1 when count < threshold, frame 2 when count >= threshold
    //   threshold = frameDuration, max = frameDuration × 2
    // Guard against zero or negative frameDuration to prevent fmod divide-by-zero.
    const f32 safeDuration = (m_frameDuration < 0.01f) ? 0.01f : m_frameDuration;
    const f32 max = safeDuration * 2.0f;
    if (m_frameTimer >= max) {
        m_frameTimer = std::fmod(m_frameTimer, max);
    }

    const i32 newFrame = (m_frameTimer < safeDuration) ? 1 : 2;
    if (newFrame != m_currentFrame) {
        m_currentFrame = newFrame;
        m_cachedTexture = nullptr; // frame changed → texture key is stale
    }
}

// ------------------------------------------------------------------------------
// Idle state cycling
// ------------------------------------------------------------------------------

void NekoCat::advanceIdleState(const f32 dt) noexcept {
    m_idleTimer += dt;

    // Guard against zero or negative idleStateDuration to prevent fmod divide-by-zero.
    const f32 safeDuration = (m_idleStateDuration < 0.01f) ? 0.01f : m_idleStateDuration;
    if (m_idleTimer < safeDuration) return;

    // Reset timer (carry forward any overflow for smooth cycling)
    m_idleTimer = std::fmod(m_idleTimer, safeDuration);

    // Cycle to the next idle state: Awake → Scratching → Washing → Yawning → Sleeping → Awake...
    switch (m_state) {
    case State::Awake:     setState(State::Scratching); break;
    case State::Scratching: setState(State::Washing);   break;
    case State::Washing:   setState(State::Yawning);    break;
    case State::Yawning:   setState(State::Sleeping);   break;
    case State::Sleeping:  setState(State::Awake);      break;
    default:
        break;
    }
}

// ------------------------------------------------------------------------------
// Per-frame update
// ------------------------------------------------------------------------------

void NekoCat::update(const f32 dt, const Direction inputDirection) noexcept {
    // ---- Determine state ----
    if (inputDirection == Direction::Idle) {
        // Transition out of Walking, then cycle idle states
        if (m_state == State::Walking) {
            setState(State::Awake);
            m_idleTimer = 0.0f;
        } else {
            advanceIdleState(dt);
        }
    } else {
        if (m_direction != inputDirection) {
            m_direction = inputDirection;
            m_cachedTexture = nullptr; // direction changed → texture key is stale
        }
        if (m_state != State::Walking) {
            setState(State::Walking);
        }
    }

    // ---- Move position ----
    if (m_state == State::Walking) {
        const f32 norm = diagonalScale(m_direction);
        constexpr f32 kMoveSpeed = 200.0f; // pixels per second
        m_x += dirDeltaX(m_direction) * kMoveSpeed * norm * dt;
        m_y += dirDeltaY(m_direction) * kMoveSpeed * norm * dt;

        // Clamp to screen bounds
        const f32 spriteSize = 32.0f * m_scale;
        const f32 screenW = static_cast<f32>(GetScreenWidth());
        const f32 screenH = static_cast<f32>(GetScreenHeight());

        if (m_x < 0.0f) m_x = 0.0f;
        if (m_y < 0.0f) m_y = 0.0f;
        if (m_x > screenW - spriteSize) m_x = screenW - spriteSize;
        if (m_y > screenH - spriteSize) m_y = screenH - spriteSize;
    }

    // ---- Advance animation ----
    advanceAnimation(dt);
}

// ------------------------------------------------------------------------------
// Render
// ------------------------------------------------------------------------------

void NekoCat::render() const noexcept {
    if (!m_loaded) return;

    // Use cached pointer when the pose+frame haven't changed since the last
    // lookup — this is the common case (most frames reuse the same texture).
    const Texture2D& tex = m_cachedTexture
        ? *m_cachedTexture
        : getTextureForCurrentFrame();

    if (tex.id == 0) return;

    const f32 spriteSize = 32.0f * m_scale;
    DrawTexturePro(
        tex,
        Rectangle{0.0f, 0.0f, static_cast<f32>(tex.width), static_cast<f32>(tex.height)},
        Rectangle{m_x, m_y, spriteSize, spriteSize},
        Vector2{0.0f, 0.0f},
        0.0f,
        WHITE);
}

} // namespace biofuel::game::presentation::sprites
