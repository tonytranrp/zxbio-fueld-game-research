#include "GamePlayScreen.hpp"

#include "engine/graphics/Render.hpp"
#include <raylib.h>

namespace biofuel::game::screens {

namespace {

constexpr Color kSkyTop{116, 170, 226, 255};
constexpr Color kSkyHorizon{206, 224, 236, 255};

} // namespace

// =============================================================================
// Lifecycle
// =============================================================================

GamePlayScreen::GamePlayScreen() = default;

GamePlayScreen::~GamePlayScreen() noexcept {
    releaseCursor();
    m_voxels.unloadAll();
}

void GamePlayScreen::onEnter() {
    ::biofuel::engine::world::voxel::VoxelWorld::Config config{};
    config.viewRadiusChunks = 6;
    config.maxBuildsPerFrame = 3;
    config.seed = 20260602U;
    config.seaLevel = config.baseHeight + 6;   // flood low valleys into lakes
    m_voxels.configure(config);

    // Spawn slightly inside the first chunk and prime the surrounding chunks so
    // the world is solid the moment the player appears. Ground collision uses the
    // noise function directly, so the player never falls even before meshes load.
    const Vector3 spawn{8.0f, 0.0f, 8.0f};
    for (i32 i = 0; i < 60; ++i) {
        m_voxels.update(spawn);
        if (m_voxels.lastBuiltThisFrame() == 0U) {
            break;
        }
    }
    m_player.reset(Vector3{spawn.x, m_voxels.groundHeight(spawn.x, spawn.z), spawn.z});

    ::biofuel::game::presentation::hands::ensureModelOnlyHandTracking();
    m_handOverlay.onEnter();
    captureCursor();
}

void GamePlayScreen::onExit() {
    releaseCursor();
    m_handOverlay.onExit();
    m_voxels.unloadAll();
}

void GamePlayScreen::onPause() {
    // An overlay (e.g. the pause popup) is taking the foreground — give the OS
    // cursor back so the player can click menu buttons.
    releaseCursor();
}

void GamePlayScreen::onResume() {
    // Back in control of the world: re-capture the cursor for mouse-look.
    captureCursor();
}

void GamePlayScreen::onUpdate(const f32 dt) {
    // Chunk streaming runs at the fixed step (60 Hz is plenty); the player itself
    // is driven per render frame in onInput() so mouse-look and jump react to
    // every input poll rather than only the ~1/N frames a fixed step samples.
    m_voxels.update(m_player.feetPosition());
    m_handOverlay.update(dt);
}

void GamePlayScreen::onInput() {
    // Runs once per rendered frame. Driving the kinematic controller here (with
    // the real frame delta) keeps mouse-look smooth and never drops a jump press
    // regardless of how far render rate runs ahead of the fixed update.
    const f32 dt = GetFrameTime();
    m_player.update(dt, [this](const f32 x, const f32 z) noexcept {
        return m_voxels.groundHeight(x, z);
    });
}

void GamePlayScreen::onRender() {
    renderSky();

    const Camera3D camera = m_player.camera();
    BeginMode3D(camera);
    m_voxels.render();
    m_voxels.renderWater(static_cast<f32>(GetTime()));
    EndMode3D();

    // Model-only hand-tracking overlay (renders the AR hand model when the
    // player has calibrated; no-op otherwise). Draws its own 3D pass.
    m_handOverlay.render();

    renderHud();
}

// =============================================================================
// Rendering helpers
// =============================================================================

void GamePlayScreen::renderSky() const {
    // ClearBackground also clears the depth buffer for the 3D pass; the gradient
    // is then painted as a flat sky behind the world.
    ClearBackground(kSkyHorizon);
    const i32 w = ::biofuel::engine::graphics::Renderer::screenWidth();
    const i32 h = ::biofuel::engine::graphics::Renderer::screenHeight();
    DrawRectangleGradientV(0, 0, w, h, kSkyTop, kSkyHorizon);
}

void GamePlayScreen::renderHud() const {
    using ::biofuel::engine::graphics::Renderer;
    const i32 w = Renderer::screenWidth();
    const i32 h = Renderer::screenHeight();

    const i32 cx = w / 2;
    const i32 cy = h / 2;
    DrawLine(cx - 8, cy, cx + 8, cy, Color{255, 255, 255, 180});
    DrawLine(cx, cy - 8, cx, cy + 8, Color{255, 255, 255, 180});

    Renderer::drawText("FUEL FARM — Voxel World", 20, 18, 24, Color{245, 232, 180, 235});
    Renderer::drawText("WASD move  •  Mouse look  •  SHIFT sprint  •  SPACE jump  •  ESC pause",
                       20, 48, 16, Color{210, 220, 224, 220});

    const Vector3 p = m_player.feetPosition();
    DrawText(TextFormat("pos %.0f, %.0f, %.0f   speed %.1f m/s   %s   chunks %zu",
                        static_cast<double>(p.x), static_cast<double>(p.y), static_cast<double>(p.z),
                        static_cast<double>(m_player.speed()),
                        m_player.grounded() ? "grounded" : "airborne",
                        m_voxels.loadedChunkCount()),
             20, h - 28, 16, Color{180, 196, 200, 220});
}

void GamePlayScreen::captureCursor() noexcept {
    if (!m_cursorCaptured) {
        DisableCursor();
        m_cursorCaptured = true;
    }
}

void GamePlayScreen::releaseCursor() noexcept {
    if (m_cursorCaptured) {
        EnableCursor();
        m_cursorCaptured = false;
    }
}

} // namespace biofuel::game::screens
