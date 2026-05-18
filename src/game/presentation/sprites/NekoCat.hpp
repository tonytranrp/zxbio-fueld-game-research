#pragma once

#include "engine/core/Types.hpp"
#include <raylib.h>
#include <string>
#include <unordered_map>

namespace biofuel::game::presentation::sprites {

// ------------------------------------------------------------------------------
// Direction — compass bearing the cat is facing / moving toward.
// Idle means no movement; the last non-idle direction is retained for rendering.
// ------------------------------------------------------------------------------
enum class Direction : u8 {
    Up,
    Down,
    Left,
    Right,
    UpLeft,
    UpRight,
    DownLeft,
    DownRight,
    Idle,
};

// ------------------------------------------------------------------------------
// State — behavioural animation set.
// Walking uses directional sprites. All other states use a fixed sprite set.
// ------------------------------------------------------------------------------
enum class State : u8 {
    Walking,
    Awake,
    Scratching,
    Washing,
    Yawning,
    Sleeping,
};

// ------------------------------------------------------------------------------
// NekoCat — pixel-art sprite animation system for the Neko cat companion.
//
// Loads individual 32×32 PNG frames from assets/sprites/neko/ (not a spritesheet).
// Each direction has 2 animation frames.  Idle states (scratch, wash, yawn, sleep)
// also have 2 frames each; Awake is a single frame.
//
// Animation model (from crgimenes/neko):
//   counter 0→max, frame 1 when count < threshold, frame 2 when count >= threshold,
//   reset to 0 at max.  Threshold = frameDuration, max = frameDuration × 2.
//
// Usage:
//   NekoCat cat;
//   cat.load();
//   // ... each frame:
//   cat.update(dt, inputDirection);
//   cat.render();
//   // ... on shutdown:
//   cat.unload();
// ------------------------------------------------------------------------------
class NekoCat {
public:
    NekoCat() = default;
    ~NekoCat();

    // ---- Move-only: Texture2D ownership must not be duplicated ----
    NekoCat(const NekoCat&) = delete;
    NekoCat& operator=(const NekoCat&) = delete;
    NekoCat(NekoCat&& other) noexcept;
    NekoCat& operator=(NekoCat&& other) noexcept;

    // ---- Asset lifecycle ----
    //
    // load()   — Loads every PNG from assets/sprites/neko/ into GPU textures.
    //            Must be called after raylib InitWindow().
    // unload() — Releases all GPU textures.  Safe to call multiple times.
    void load();
    void unload() noexcept;

    // ---- Per-frame update ----
    //
    // dt              — delta time in seconds.
    // inputDirection  — intended movement direction from input system.
    //                   Direction::Idle preserves the current direction
    //                   but switches state to Awake (if currently Walking).
    void update(f32 dt, Direction inputDirection) noexcept;

    // ---- Render ----
    //
    // Draws the 32×32 source rect scaled by m_scale at (m_x, m_y) using
    // TEXTURE_FILTER_POINT for crisp pixel-art edges.
    void render() const noexcept;

    // ---- Setters ----
    void setDirection(Direction dir) noexcept { m_direction = dir; }
    void setState(State state) noexcept;
    void setPosition(f32 x, f32 y) noexcept { m_x = x; m_y = y; }
    void setScale(f32 scale) noexcept { m_scale = scale; }
    void setFrameDuration(f32 duration) noexcept { m_frameDuration = duration; }

    // ---- Accessors ----
    [[nodiscard]] Direction direction() const noexcept { return m_direction; }
    [[nodiscard]] State state() const noexcept { return m_state; }
    [[nodiscard]] f32 x() const noexcept { return m_x; }
    [[nodiscard]] f32 y() const noexcept { return m_y; }
    [[nodiscard]] f32 scale() const noexcept { return m_scale; }
    [[nodiscard]] f32 frameDuration() const noexcept { return m_frameDuration; }
    [[nodiscard]] i32 currentFrame() const noexcept { return m_currentFrame; }
    [[nodiscard]] bool isLoaded() const noexcept { return m_loaded; }

private:
    // ---- Texture lookup ----
    //
    // Builds a key like "up1", "scratch2", "awake" and returns the
    // matching Texture2D reference.  Precondition: load() must have
    // completed successfully.
    [[nodiscard]] const Texture2D& getTextureForCurrentFrame() const noexcept;

    // ---- Key-name helpers ----
    [[nodiscard]] static const char* directionPrefix(Direction dir) noexcept;
    [[nodiscard]] static const char* statePrefix(State state) noexcept;
    [[nodiscard]] static i32 frameCountForState(State state) noexcept;

    // ---- Animation counter ----
    void advanceAnimation(f32 dt) noexcept;

    // -----------------------------------------------------------------------
    // Texture storage
    // -----------------------------------------------------------------------
    // TransparentHash + std::equal_to<> enables heterogeneous lookup via
    // std::string_view so getTextureForCurrentFrame() can search without
    // constructing a temporary std::string on every render() call.
    std::unordered_map<std::string, Texture2D,
                       biofuel::TransparentHash,
                       std::equal_to<>> m_textures;

    // -----------------------------------------------------------------------
    // Transform
    // -----------------------------------------------------------------------
    f32 m_x = 0.0f;
    f32 m_y = 0.0f;
    f32 m_scale = 2.0f;       // 32×32 → 64×64 on screen

    // -----------------------------------------------------------------------
    // Animation state
    // -----------------------------------------------------------------------
    f32 m_frameTimer = 0.0f;
    f32 m_frameDuration = 0.15f;  // ~6.67 fps per frame, ~3.33 fps full cycle
    i32 m_currentFrame = 0;

    // -----------------------------------------------------------------------
    // Pose
    // -----------------------------------------------------------------------
    Direction m_direction = Direction::Down;
    State m_state = State::Awake;
    bool m_loaded = false;
};

}  // namespace biofuel::game::presentation::sprites
