#pragma once

#include "engine/runtime/typed/ShaderDeclare.hpp"
#include "engine/graphics/shaders/ShaderModule.hpp"
#include "ShaderSources.hpp"

namespace biofuel::engine::graphics::shader {

// ==============================================================================
// CrossfadeModule — Blends two screen captures by transition progress
// ==============================================================================
//
// GLSL source lives in assets/shaders/crossfade.glsl and is embedded at build
// time via CMake file(READ) + configure_file() into shader_source::crossfade_source.
//
// Uniforms:
//   texture0   — outgoing screen (bound to draw call)
//   textureIn  — incoming screen (set via SetShaderValueTexture)
//   progress   — blend factor 0.0→1.0 (outgoing→incoming)
// ==============================================================================

class CrossfadeModule {
public:
    static constexpr std::string_view NAME = "crossfade";
    static constexpr std::string_view FRAGMENT_SOURCE = shader_source::crossfade_source;
    static constexpr const char* VERTEX_SOURCE = nullptr;
    static constexpr ShaderModuleConfig CONFIG{
        .name = NAME,
        .fragmentSource = FRAGMENT_SOURCE,
        .vertexSource = VERTEX_SOURCE,
    };
    static constexpr std::string_view UNIFORM_TEXTURE_IN = "textureIn";
    static constexpr std::string_view UNIFORM_PROGRESS = "progress";
};

} // namespace biofuel::engine::graphics::shader

namespace biofuel::engine::runtime::typed::shader {
struct Crossfade {};
namespace crossfade {
BIOFUEL_SHADER_UNIFORM(TextureIn, ::biofuel::engine::graphics::shader::CrossfadeModule::UNIFORM_TEXTURE_IN, SHADER_UNIFORM_SAMPLER2D);
BIOFUEL_SHADER_UNIFORM(Progress, ::biofuel::engine::graphics::shader::CrossfadeModule::UNIFORM_PROGRESS, SHADER_UNIFORM_FLOAT);
} // namespace crossfade
} // namespace biofuel::engine::runtime::typed::shader

namespace biofuel::engine::runtime::typed {
BIOFUEL_EMBEDDED_SHADER_ASSET(
    shader::Crossfade,
    ::biofuel::engine::graphics::shader::CrossfadeModule,
    shader::crossfade::TextureIn,
    shader::crossfade::Progress);
BIOFUEL_SHADER_MODULE(CrossfadeShaderModule, shader::Crossfade)
} // namespace biofuel::engine::runtime::typed
