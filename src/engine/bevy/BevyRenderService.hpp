#pragma once

#include "engine/core/Types.hpp"
#include <raylib.h>
#include <memory>

namespace biofuel::engine::bevy {

// -----------------------------------------------------------------------------
// BevyRenderService - owns the headless Bevy renderer and the raylib texture it
// composites into.
//
// Mirrors VideoManager/VideoFfmpegBackend's shape: a singleton with
// init()/shutdown()/update(), privately owning raw raylib Texture2D lifetime,
// exposing only a read accessor (getFrameTexture()) so callers never touch
// that lifetime directly. Bevy renders offscreen (no OS window, no shared
// surface with raylib's own OpenGL context - see src/engine/Rust/bevy's own
// README for why); this service pulls the rendered pixels back across the
// cxx bridge once per fixed-step tick and uploads them via UpdateTexture(),
// the same "external per-frame pixel producer" shape VideoFfmpegBackend
// already uses for decoded ffmpeg frames.
// -----------------------------------------------------------------------------
class BevyRenderService {
public:
    [[nodiscard]] static BevyRenderService& instance() noexcept;

    void init();
    void shutdown() noexcept;
    void update(f32 dt);

    // Accumulates a mouse-look delta for the next update() call. Safe to call
    // more than once per fixed-step tick (e.g. once per real frame while the
    // fixed-step loop catches up) - deltas accumulate and are consumed/reset
    // by the next update().
    void addLookDelta(f32 dx, f32 dy) noexcept;

    [[nodiscard]] Texture2D getFrameTexture() const noexcept;

    BevyRenderService(const BevyRenderService&) = delete;
    BevyRenderService& operator=(const BevyRenderService&) = delete;
    BevyRenderService(BevyRenderService&&) = delete;
    BevyRenderService& operator=(BevyRenderService&&) = delete;

private:
    BevyRenderService() = default;
    ~BevyRenderService() noexcept;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    Texture2D m_texture{};
    bool m_initialized = false;
    f32 m_pendingLookDx = 0.0f;
    f32 m_pendingLookDy = 0.0f;
};

} // namespace biofuel::engine::bevy
