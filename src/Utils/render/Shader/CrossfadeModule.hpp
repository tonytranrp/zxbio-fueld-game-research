#pragma once

#include "Utils/render/Shader/ShaderModule.hpp"
#include "ShaderSources.hpp"

namespace biofuel::utils::render::shader {

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

} // namespace biofuel::utils::render::shader
