#include "engine/bevy/BevyRenderService.hpp"

#include "biofuel_bevy_bridge_cxx/lib.h"
#include <raylib.h>
#include <utility>

namespace biofuel::engine::bevy {

namespace bevy_bridge = ::biofuel::engine::bevy_bridge;

namespace {

// Fixed offscreen resolution, independent of the live window size - same
// simplification VideoFfmpegBackend already makes for decoded video frames.
// drawFullscreenTexture()'s DrawTexturePro scales this to fit whatever the
// window actually is.
constexpr u32 BEVY_RENDER_WIDTH = 1280;
constexpr u32 BEVY_RENDER_HEIGHT = 720;

} // namespace

struct BevyRenderService::Impl {
    explicit Impl(rust::Box<bevy_bridge::BevyRenderer> r) noexcept
        : renderer(std::move(r)) {}

    rust::Box<bevy_bridge::BevyRenderer> renderer;
};

BevyRenderService& BevyRenderService::instance() noexcept {
    static BevyRenderService service{};
    return service;
}

BevyRenderService::~BevyRenderService() noexcept {
    if (m_initialized) {
        shutdown();
    }
}

void BevyRenderService::init() {
    if (m_initialized) {
        return;
    }

    m_impl = std::make_unique<Impl>(bevy_bridge::new_renderer(BEVY_RENDER_WIDTH, BEVY_RENDER_HEIGHT));

    Image blank = GenImageColor(static_cast<i32>(BEVY_RENDER_WIDTH), static_cast<i32>(BEVY_RENDER_HEIGHT), MAGENTA);
    m_texture = LoadTextureFromImage(blank);
    UnloadImage(blank);

    m_initialized = m_texture.id != 0;
}

void BevyRenderService::shutdown() noexcept {
    if (!m_initialized) {
        return;
    }

    if (m_texture.id != 0) {
        UnloadTexture(m_texture);
        m_texture = {};
    }
    m_impl.reset();
    m_initialized = false;
}

void BevyRenderService::addLookDelta(const f32 dx, const f32 dy) noexcept {
    m_pendingLookDx += dx;
    m_pendingLookDy += dy;
}

void BevyRenderService::update(const f32 dt) {
    if (!m_initialized) {
        return;
    }

    const bevy_bridge::BridgeInputState input{
        .move_forward = IsKeyDown(KEY_W),
        .move_back = IsKeyDown(KEY_S),
        .move_left = IsKeyDown(KEY_A),
        .move_right = IsKeyDown(KEY_D),
        .look_dx = m_pendingLookDx,
        .look_dy = m_pendingLookDy,
    };
    m_pendingLookDx = 0.0f;
    m_pendingLookDy = 0.0f;

    bevy_bridge::step_frame(*m_impl->renderer, dt, input);

    const rust::Slice<const uint8_t> pixels = bevy_bridge::frame_pixels(*m_impl->renderer);
    const auto expectedBytes = static_cast<usize>(BEVY_RENDER_WIDTH) * static_cast<usize>(BEVY_RENDER_HEIGHT) * 4U;
    if (pixels.size() == expectedBytes) {
        UpdateTexture(m_texture, pixels.data());
    }
}

Texture2D BevyRenderService::getFrameTexture() const noexcept {
    return m_texture;
}

} // namespace biofuel::engine::bevy
