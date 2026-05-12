#pragma once

#include "engine/core/Types.hpp"
#include "engine/runtime/typed/ShaderDeclare.hpp"
#include "game/models/ModelSystem.hpp"
#include "engine/graphics/components/Camera/ShaderCamera.hpp"
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
#include "game/presentation/effects/ModelControllerOverlay.hpp"
#include <array>
#endif
#include <raylib.h>
#include <memory>

namespace biofuel::game::presentation::effects {

// ------------------------------------------------------------------------------
// MenuTransitionHands — 3D hand model overlay for New Game / Continue transitions.
//
// Phase machine:
//   Idle → (start) → Playing → (anim done, 2.52s) → Complete (holds final frame)
//
// Renders during Playing and Complete. Stays in Complete until reset() or
// unload() is called by the owning screen's lifecycle.
// ------------------------------------------------------------------------------
class MenuTransitionHands final {
public:
    enum class Phase : u8 {
        Idle,       // loaded, not active — no rendering
        Playing,    // "action" animation running — full render
        Complete,   // animation finished, holding final frame — full render
    };
public:
    static constexpr game::models::ModelAssetId ASSET_ID = game::models::ModelAssetId::MenuTransitionHands;

    // Animation timing constants
    static constexpr f32 ACTION_DURATION = 2.52f;   // "action" anim clip length

    void load();
    void unload() noexcept;
    void reset() noexcept;
    void start() noexcept;
    void update(f32 dt, f32 dimensionShift, const ::biofuel::engine::graphics::component::ShaderCameraState& shaderCamera) noexcept;
    void render() noexcept;
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
    void renderControllerOverlay() noexcept;
#endif

    [[nodiscard]] bool ready() const noexcept { return m_loaded; }
    [[nodiscard]] Phase phase() const noexcept { return m_phase; }
    [[nodiscard]] bool isRendering() const noexcept { return m_phase == Phase::Playing || m_phase == Phase::Complete; }

private:
    struct TransitionRenderPose {
        Vector3 position{0.0f, 0.0f, 0.0f};
        Quaternion orientation{0.0f, 0.0f, 0.0f, 1.0f};
        Vector3 scale{1.0f, 1.0f, 1.0f};
        Color auraTint{88, 136, 220, 180};
        Color baseTint{232, 238, 255, 220};
    };

    [[nodiscard]] TransitionRenderPose buildPose(const game::models::ModelInstance& instance) const noexcept;
    void drawHands(const game::models::ModelInstance& instance, const TransitionRenderPose& pose) const noexcept;

    void updateCamera(const ::biofuel::engine::graphics::component::ShaderCameraState& shaderCamera) noexcept;
    void cacheUniformLocations(Shader shader) const noexcept;
    void applyShaderUniforms(const game::models::ModelInstance& instance) const noexcept;
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
    using ControllerTargets = std::array<ModelControlTarget, 5>;
    [[nodiscard]] ControllerTargets buildControllerTargets(const TransitionRenderPose& pose) noexcept;
    void updateController(const TransitionRenderPose& pose) noexcept;
    void renderController(const TransitionRenderPose& pose) noexcept;
    void applyControllerOffsets() noexcept;
#endif
    Camera3D m_camera{
        .position = Vector3{0.0f, 0.15f, 2.55f},
        .target = Vector3{0.0f, 0.0f, 0.0f},
        .up = Vector3{0.0f, 1.0f, 0.0f},
        .fovy = 30.0f,
        .projection = CAMERA_PERSPECTIVE,
    };

    std::shared_ptr<game::models::ModelInstance> m_instance;
    bool m_loaded = false;
    Phase m_phase = Phase::Idle;
    f32 m_elapsed = 0.0f;
    f32 m_dimensionShift = 0.0f;
    f32 m_cameraYaw = 0.0f;
    f32 m_baseScale = 1.0f;
    f32 m_fingerStartY = 0.0f;
    f32 m_fingerEndY = 1.0f;
#ifdef BIOFUEL_DEV_MODEL_CONTROLLER
    ModelControllerOverlay m_controller;
    Vector3 m_rootOffset{0.0f, 0.0f, 0.0f};
    Vector3 m_cameraPositionOffset{0.0f, 0.0f, 0.0f};
    Vector3 m_cameraTargetOffset{0.0f, 0.0f, 0.0f};
    Vector3 m_leftHandOffset{0.0f, 0.0f, 0.0f};
    Vector3 m_rightHandOffset{0.0f, 0.0f, 0.0f};
#endif

    mutable i32 m_timeLoc = -1;
    mutable i32 m_portalStrengthLoc = -1;
    mutable i32 m_fingerStartLoc = -1;
    mutable i32 m_fingerEndLoc = -1;
    mutable i32 m_colorALoc = -1;
    mutable i32 m_colorBLoc = -1;
    mutable i32 m_rimColorLoc = -1;
    mutable u32 m_cachedShaderId = 0;
};

} // namespace biofuel::game::presentation::effects

namespace biofuel::engine::runtime::typed::shader {
struct MenuHands {};
namespace menu_hands {
BIOFUEL_SHADER_UNIFORM(Time, "uTime", SHADER_UNIFORM_FLOAT);
BIOFUEL_SHADER_UNIFORM(PortalStrength, "uPortalStrength", SHADER_UNIFORM_FLOAT);
BIOFUEL_SHADER_UNIFORM(FingerStartY, "uFingerStartY", SHADER_UNIFORM_FLOAT);
BIOFUEL_SHADER_UNIFORM(FingerEndY, "uFingerEndY", SHADER_UNIFORM_FLOAT);
BIOFUEL_SHADER_UNIFORM(ColorA, "uColorA", SHADER_UNIFORM_VEC3);
BIOFUEL_SHADER_UNIFORM(ColorB, "uColorB", SHADER_UNIFORM_VEC3);
BIOFUEL_SHADER_UNIFORM(RimColor, "uRimColor", SHADER_UNIFORM_VEC3);
} // namespace menu_hands
} // namespace biofuel::engine::runtime::typed::shader

namespace biofuel::engine::runtime::typed {
BIOFUEL_FILE_SHADER_ASSET(
    shader::MenuHands,
    "menu_hands",
    "assets/shaders/menu_hands.vs",
    "assets/shaders/menu_hands.fs",
    true,
    shader::menu_hands::Time,
    shader::menu_hands::PortalStrength,
    shader::menu_hands::FingerStartY,
    shader::menu_hands::FingerEndY,
    shader::menu_hands::ColorA,
    shader::menu_hands::ColorB,
    shader::menu_hands::RimColor);
BIOFUEL_SHADER_MODULE(MenuHandsShaderModule, shader::MenuHands)
} // namespace biofuel::engine::runtime::typed
