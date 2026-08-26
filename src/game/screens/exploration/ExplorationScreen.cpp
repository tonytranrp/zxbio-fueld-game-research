#include "ExplorationScreen.hpp"
#include "ExplorationScreenModule.hpp"
#include "engine/ui/ScreenManager.hpp"
#include "engine/ui/typed/RenderPipeline.hpp"
#include "engine/runtime/Runtime.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/graphics/Scene3D.hpp"
#include <raylib.h>

namespace biofuel::engine::ui::typed::exploration {

void WorldLayerTag::render(::biofuel::game::screens::ExplorationScreen& screen, RenderContext&) {
    const Vector3 eye{
        screen.character().position().x,
        screen.character().position().y + screen.character().eyeHeight(),
        screen.character().position().z,
    };
    const Camera3D camera = screen.firstPersonCamera().toCamera3D(eye, 74.0f);
    const ::biofuel::engine::graphics::ScopedMode3D mode3D(camera);
    screen.level().draw();
}

void ViewmodelLayerTag::render(::biofuel::game::screens::ExplorationScreen& screen, RenderContext& context) {
    const auto& hands = screen.handsInstance();
    if (!hands || !hands->ready()) {
        return;
    }

    // Fixed, non-rotating camera: this first pass deliberately does not track
    // player look (yaw/pitch) -- weapon-sway/pitch-tracking is a follow-up,
    // not required for the hands to be visibly present and correctly animated.
    constexpr Camera3D viewmodelCamera{
        .position = {0.0f, 0.0f, 0.0f},
        .target = {0.0f, 0.0f, 1.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 60.0f,
        .projection = CAMERA_PERSPECTIVE,
    };

    auto& pass = screen.viewmodelPass();
    pass.ensureSized(context.screenWidth, context.screenHeight);
    pass.begin(viewmodelCamera);
    {
        // The exported rig's rest pose reaches along local +Y (verified
        // numerically from the file's own node transforms -- see
        // assets/models/viewmodel_hands/README.md -- not assumed from the
        // generation prompt text). Rotate +90 degrees about local X so the
        // hands reach along +Z, this camera's fixed forward, instead of
        // straight up.
        const ::biofuel::engine::models::ModelRenderState renderState{
            .position = {0.0f, -0.28f, 0.45f},
            .rotationAxis = {1.0f, 0.0f, 0.0f},
            .scale = {1.0f, 1.0f, 1.0f},
            .rotationDegrees = 90.0f,
            .tint = WHITE,
            .visible = true,
        };
        hands->draw(renderState);
    }
    pass.endAndComposite();
}

void HudLayerTag::render(::biofuel::game::screens::ExplorationScreen&, RenderContext& context) {
    const i32 cx = context.screenWidth / 2;
    const i32 cy = context.screenHeight / 2;
    constexpr Color crosshair{255, 255, 255, 180};
    DrawLine(cx - 8, cy, cx + 8, cy, crosshair);
    DrawLine(cx, cy - 8, cx, cy + 8, crosshair);
}

} // namespace biofuel::engine::ui::typed::exploration

namespace biofuel::game::screens {

void ExplorationScreen::onEnter() {
    auto world = ::biofuel::engine::runtime::Runtime::physics().world3D();
    m_level.spawnColliders(world);
    m_character.spawn(world, m_level.playerSpawn());

    if (!m_handsInstance) {
        m_handsInstance = ::biofuel::engine::runtime::Runtime::model().createInstance(
            ::biofuel::engine::models::ModelAssetId::ViewmodelHands);
        // ModelAnimator::configure() already enters spec.defaultIdleState
        // ("idle") on construction -- no need to setAnimationState() here too.
    }

    if (!m_cursorCaptured) {
        DisableCursor();
        m_cursorCaptured = true;
    }
}

void ExplorationScreen::onExit() {
    auto world = ::biofuel::engine::runtime::Runtime::physics().world3D();
    m_character.despawn(world);
    m_level.despawn(world);

    if (m_cursorCaptured) {
        EnableCursor();
        m_cursorCaptured = false;
    }
}

void ExplorationScreen::onUpdate(const f32 dt) {
    // WASD/shift/space are level-state key queries, safe to re-poll every
    // fixed tick even if this runs 0..N times per render frame -- unlike
    // mouse delta (onInput below), there's no accumulation to double-count.
    Vector2 moveAxis{0.0f, 0.0f};
    if (IsKeyDown(KEY_D)) moveAxis.x += 1.0f;
    if (IsKeyDown(KEY_A)) moveAxis.x -= 1.0f;
    if (IsKeyDown(KEY_W)) moveAxis.y += 1.0f;
    if (IsKeyDown(KEY_S)) moveAxis.y -= 1.0f;

    const engine::character::CharacterMoveInput input{
        .moveAxis = moveAxis,
        .yawRadians = m_camera.yawRadians(),
        .sprint = IsKeyDown(KEY_LEFT_SHIFT),
        .jump = IsKeyDown(KEY_SPACE),
    };

    auto world = ::biofuel::engine::runtime::Runtime::physics().world3D();
    m_character.step(world, input, dt);
    m_camera.updateBob(m_character.horizontalSpeed(), dt);

    if (m_handsInstance && m_handsInstance->ready()) {
        // Actual per-frame animation advancement happens centrally via
        // ModelService::update() in App.cpp -- calling setAnimationState with
        // the name it's already in would reset it to frame 0 every tick (see
        // ModelAnimator::beginState), so only call it on an actual change.
        constexpr f32 kWalkAnimSpeedThreshold = 0.5f; // m/s
        const std::string_view desiredState =
            (m_character.grounded() && m_character.horizontalSpeed() > kWalkAnimSpeedThreshold)
                ? "walk"
                : "idle";
        if (m_handsInstance->animator().currentState() != desiredState) {
            m_handsInstance->setAnimationState(desiredState, 0.2f);
        }
    }
}

void ExplorationScreen::onRender() {
    ::biofuel::engine::ui::typed::RenderContext context{
        .manager = manager(),
        .services = &::biofuel::engine::runtime::Runtime::services(),
        .screenId = screenId(),
        .screenWidth = ::biofuel::engine::graphics::Renderer::screenWidth(),
        .screenHeight = ::biofuel::engine::graphics::Renderer::screenHeight(),
        .transitionAlpha = transitionAlpha(),
        .frameTime = GetFrameTime(),
    };
    ::biofuel::engine::ui::typed::RenderPipeline<ExplorationScreen>::render(*this, context);
}

void ExplorationScreen::onInput() {
    // Mouse delta must be read exactly once per render frame here, not in
    // onUpdate (which runs at the fixed tick rate and could read it 0 or
    // several times per frame) -- see engine/character/README.md.
    const Vector2 delta = GetMouseDelta();
    m_camera.addLookDelta(delta);
}

} // namespace biofuel::game::screens
