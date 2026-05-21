#pragma once

#include "engine/runtime/typed/ShaderDeclare.hpp"
#include "engine/graphics/shaders/ShaderModule.hpp"
#include "ShaderSources.hpp"

namespace biofuel::engine::graphics::shader {

// ==============================================================================
// ProceduralBackdropModule — Raymarched fractal background for the main menu
// ==============================================================================
//
// Adapted from ShaderToy s3s3WN. Uses gl_FragCoord so no texture binding
// is required — draw any full-screen geometry (e.g. DrawRectangle) with
// this shader active and it will fill the viewport.
//
// GLSL source lives in assets/shaders/procedural_backdrop.glsl and is embedded at
// build time via CMake into shader_source::procedural_backdrop_source.
//
// Uniforms (shader-owned):
//   iResolution      (vec3)  — viewport width, height, 1.0
//   iTime            (float) — elapsed seconds since screen entered
//   uBrightness      (float) — overall brightness envelope during screen intro
//   uRevealProgress  (float) — background landing progress (0.0 → 1.0)
//   uDimensionShift  (float) — warp-through effect after UI dismiss (0.0 → 1.0)
//   uIdleDim         (float) — idle screen fade (0.0 = normal, 1.0 = dim)
//
// Component-managed uniforms (set by CameraComponent):
//   uCameraOffsetX   (float) — via Components/Camera/CameraComponent
//   uCameraOffsetY   (float) — via Components/Camera/CameraComponent
//   uCameraYaw       (float) — via Components/Camera/CameraComponent
// ==============================================================================

class ProceduralBackdropModule {
public:
    static constexpr std::string_view NAME = "procedural_backdrop";
    static constexpr std::string_view FRAGMENT_SOURCE = shader_source::procedural_backdrop_source;
    static constexpr const char* VERTEX_SOURCE = nullptr;
    static constexpr ShaderModuleConfig CONFIG{
        .name = NAME,
        .fragmentSource = FRAGMENT_SOURCE,
        .vertexSource = VERTEX_SOURCE,
    };

    // Shader-owned uniforms (managed by ScreenBackdropController)
    static constexpr std::string_view UNIFORM_IRESOLUTION = "iResolution";
    static constexpr std::string_view UNIFORM_ITIME = "iTime";
    static constexpr std::string_view UNIFORM_UBRIGHTNESS = "uBrightness";
    static constexpr std::string_view UNIFORM_UREVEAL_PROGRESS = "uRevealProgress";
    static constexpr std::string_view UNIFORM_UDIMENSION_SHIFT = "uDimensionShift";

    // NOTE: Camera uniforms (uCameraYaw, uCameraOffsetX/Y) are now managed
    // by CameraComponent — see Components/Camera/CameraComponent.hpp
};

} // namespace biofuel::engine::graphics::shader

namespace biofuel::engine::runtime::typed::shader {
struct ProceduralBackdrop {};
namespace procedural_backdrop {
BIOFUEL_SHADER_UNIFORM(IResolution, ::biofuel::engine::graphics::shader::ProceduralBackdropModule::UNIFORM_IRESOLUTION, SHADER_UNIFORM_VEC3);
BIOFUEL_SHADER_UNIFORM(ITime, ::biofuel::engine::graphics::shader::ProceduralBackdropModule::UNIFORM_ITIME, SHADER_UNIFORM_FLOAT);
BIOFUEL_SHADER_UNIFORM(Brightness, ::biofuel::engine::graphics::shader::ProceduralBackdropModule::UNIFORM_UBRIGHTNESS, SHADER_UNIFORM_FLOAT);
BIOFUEL_SHADER_UNIFORM(RevealProgress, ::biofuel::engine::graphics::shader::ProceduralBackdropModule::UNIFORM_UREVEAL_PROGRESS, SHADER_UNIFORM_FLOAT);
BIOFUEL_SHADER_UNIFORM(DimensionShift, ::biofuel::engine::graphics::shader::ProceduralBackdropModule::UNIFORM_UDIMENSION_SHIFT, SHADER_UNIFORM_FLOAT);
} // namespace procedural_backdrop
} // namespace biofuel::engine::runtime::typed::shader

namespace biofuel::engine::runtime::typed {
BIOFUEL_EMBEDDED_SHADER_ASSET(
    shader::ProceduralBackdrop,
    ::biofuel::engine::graphics::shader::ProceduralBackdropModule,
    false,
    shader::procedural_backdrop::IResolution,
    shader::procedural_backdrop::ITime,
    shader::procedural_backdrop::Brightness,
    shader::procedural_backdrop::RevealProgress,
    shader::procedural_backdrop::DimensionShift);
BIOFUEL_SHADER_MODULE(ProceduralBackdropShaderModule, shader::ProceduralBackdrop)
} // namespace biofuel::engine::runtime::typed
