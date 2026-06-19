#include "GamePlayScreen.hpp"

#include "engine/graphics/Render.hpp"
#include "engine/graphics/ShaderManager.hpp"
#include "engine/runtime/Runtime.hpp"
#include <raylib.h>
#include <raymath.h>
#include <cmath>

namespace biofuel::game::screens {

namespace {

constexpr Color kSkyTop{116, 170, 226, 255};
constexpr Color kSkyHorizon{206, 224, 236, 255};

// Flat albedo per Block id (0 = air, unused). Indices match enum Block.
constexpr f32 kPalette[24] = {
    0.00f, 0.00f, 0.00f,   // 0 air
    0.32f, 0.55f, 0.24f,   // 1 grass
    0.45f, 0.33f, 0.21f,   // 2 dirt
    0.48f, 0.48f, 0.52f,   // 3 stone
    0.82f, 0.76f, 0.55f,   // 4 sand
    0.93f, 0.95f, 0.98f,   // 5 snow
    0.42f, 0.30f, 0.18f,   // 6 wood
    0.24f, 0.50f, 0.22f,   // 7 leaves
};

// Indices into GamePlayScreen::m_rayLoc.
enum RayUniform : i32 {
    URes = 0, UCamPos, UCamFwd, UCamRight, UCamUp, UTanHalfFov,
    UVolume, UVolDim, USunDir, UPalette, UFlipY
};

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

    // Collision uses the noise function directly (groundHeight), so the player
    // never falls even before any geometry is built.
    const Vector3 spawn{8.0f, 0.0f, 8.0f};
    m_player.reset(Vector3{spawn.x, m_voxels.groundHeight(spawn.x, spawn.z), spawn.z});

    // Bake the voxel volume the raymarcher reads, and compile its shader.
    m_volume.configure({});
    m_volume.update(m_voxels, m_player.feetPosition());
    loadRaymarchShader();

    captureCursor();
}

void GamePlayScreen::loadRaymarchShader() {
    auto& shaders = ::biofuel::engine::runtime::Runtime::shader();
    shaders.load("raymarched_voxels", "", "assets/shaders/raymarched_voxels.glsl");
    m_rayShader = shaders.get("raymarched_voxels");
    m_rayShaderReady = IsShaderValid(m_rayShader);
    if (!m_rayShaderReady) {
        m_raymarchMode = false;   // fall back to rasterized chunks
        return;
    }
    m_rayLoc[URes]        = GetShaderLocation(m_rayShader, "uResolution");
    m_rayLoc[UCamPos]     = GetShaderLocation(m_rayShader, "uCamPos");
    m_rayLoc[UCamFwd]     = GetShaderLocation(m_rayShader, "uCamFwd");
    m_rayLoc[UCamRight]   = GetShaderLocation(m_rayShader, "uCamRight");
    m_rayLoc[UCamUp]      = GetShaderLocation(m_rayShader, "uCamUp");
    m_rayLoc[UTanHalfFov] = GetShaderLocation(m_rayShader, "uTanHalfFov");
    m_rayLoc[UVolume]     = GetShaderLocation(m_rayShader, "uVolume");
    m_rayLoc[UVolDim]     = GetShaderLocation(m_rayShader, "uVolDim");
    m_rayLoc[USunDir]     = GetShaderLocation(m_rayShader, "uSunDir");
    m_rayLoc[UPalette]    = GetShaderLocation(m_rayShader, "uPalette");
    m_rayLoc[UFlipY]      = GetShaderLocation(m_rayShader, "uFlipY");
}

void GamePlayScreen::onExit() {
    releaseCursor();
    m_voxels.unloadAll();
    m_volume.unload();
    m_rayTarget.release();
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

void GamePlayScreen::onUpdate(const f32) {
    // 60 Hz fixed step. In raymarch mode we bake the GPU volume; in raster mode
    // we stream chunk meshes. (The player itself is driven per render frame in
    // onInput so mouse-look/jump never get dropped at high render rates.)
    if (m_raymarchMode) {
        m_volume.update(m_voxels, m_player.feetPosition());
    } else {
        m_voxels.update(m_player.feetPosition());
    }
}

void GamePlayScreen::onInput() {
    // Runs once per rendered frame. Driving the kinematic controller here (with
    // the real frame delta) keeps mouse-look smooth and never drops a jump press
    // regardless of how far render rate runs ahead of the fixed update.
    const f32 dt = GetFrameTime();
    m_player.update(dt, [this](const f32 x, const f32 z) noexcept {
        return m_voxels.groundHeight(x, z);
    });
    if (m_rayShaderReady && IsKeyPressed(KEY_F6)) {
        m_raymarchMode = !m_raymarchMode;
    }
}

void GamePlayScreen::onRender() {
    if (m_raymarchMode && m_rayShaderReady) {
        renderRaymarch();
    } else {
        renderRaster();
    }

    renderHud();
}

void GamePlayScreen::renderRaster() {
    renderSky();
    const Camera3D camera = m_player.camera();
    BeginMode3D(camera);
    m_voxels.render();
    m_voxels.renderWater(static_cast<f32>(GetTime()));
    EndMode3D();
}

void GamePlayScreen::renderRaymarch() {
    using ::biofuel::engine::graphics::Renderer;
    const i32 sw = Renderer::screenWidth();
    const i32 sh = Renderer::screenHeight();
    const i32 rw = sw / 2;        // half-res: recovers perf, suits the pixel look
    const i32 rh = sh / 2;
    m_rayTarget.ensureSize(rw, rh);
    if (!m_rayTarget.valid()) {
        renderRaster();
        return;
    }
    SetTextureFilter(m_rayTarget.texture(), TEXTURE_FILTER_POINT);

    // Camera basis from the first-person camera.
    const Camera3D cam = m_player.camera();
    const Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    const Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, Vector3{0.0f, 1.0f, 0.0f}));
    const Vector3 up = Vector3CrossProduct(right, fwd);
    const f32 tanHalf = std::tan(cam.fovy * (PI / 180.0f) * 0.5f);

    // Camera eye in volume-local coordinates.
    const Vector3 origin = m_volume.originWorld();
    const Vector3 eye = m_player.eyePosition();
    const Vector3 camLocal{eye.x - origin.x, eye.y - origin.y, eye.z - origin.z};

    const f32 res[2] = {static_cast<f32>(rw), static_cast<f32>(rh)};
    const f32 volDim[3] = {static_cast<f32>(m_volume.width()), static_cast<f32>(m_volume.height()), static_cast<f32>(m_volume.depth())};
    const Vector3 sun = Vector3Normalize(Vector3{0.45f, 0.82f, 0.35f});
    const f32 sunv[3] = {sun.x, sun.y, sun.z};
    const f32 flip = -1.0f;

    SetShaderValue(m_rayShader, m_rayLoc[URes], res, SHADER_UNIFORM_VEC2);
    SetShaderValue(m_rayShader, m_rayLoc[UCamPos], &camLocal, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_rayShader, m_rayLoc[UCamFwd], &fwd, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_rayShader, m_rayLoc[UCamRight], &right, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_rayShader, m_rayLoc[UCamUp], &up, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_rayShader, m_rayLoc[UTanHalfFov], &tanHalf, SHADER_UNIFORM_FLOAT);
    SetShaderValue(m_rayShader, m_rayLoc[UVolDim], volDim, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_rayShader, m_rayLoc[USunDir], sunv, SHADER_UNIFORM_VEC3);
    SetShaderValue(m_rayShader, m_rayLoc[UFlipY], &flip, SHADER_UNIFORM_FLOAT);
    SetShaderValueV(m_rayShader, m_rayLoc[UPalette], kPalette, SHADER_UNIFORM_VEC3, 8);

    {
        ::biofuel::engine::graphics::ScopedTextureMode tex(m_rayTarget.target());
        ClearBackground(BLACK);
        ::biofuel::engine::graphics::ScopedShaderMode shaderMode(m_rayShader);
        SetShaderValueTexture(m_rayShader, m_rayLoc[UVolume], m_volume.texture());
        DrawRectangle(0, 0, rw, rh, WHITE);
    }

    // Upscale the half-res result to the screen, point-filtered (crisp pixels).
    const Texture2D t = m_rayTarget.texture();
    DrawTexturePro(t,
                   Rectangle{0.0f, 0.0f, static_cast<f32>(rw), -static_cast<f32>(rh)},
                   Rectangle{0.0f, 0.0f, static_cast<f32>(sw), static_cast<f32>(sh)},
                   Vector2{0.0f, 0.0f}, 0.0f, WHITE);
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
