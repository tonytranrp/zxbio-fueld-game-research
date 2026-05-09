#pragma once

#include "Utils/render/Shader/ShaderModule.hpp"
#include "ShaderSources.hpp"

namespace biofuel::utils::render::shader {

// ==============================================================================
// BlurHModule — Horizontal Gaussian blur pass
// ==============================================================================
//
// GLSL source lives in assets/shaders/blur_h.glsl and is embedded at build time
// via CMake file(READ) + configure_file() into shader_source::blur_h_source.
// ==============================================================================

class BlurHModule {
public:
    static constexpr std::string_view NAME = "blur_h";
    static constexpr std::string_view FRAGMENT_SOURCE = shader_source::blur_h_source;
    static constexpr const char* VERTEX_SOURCE = nullptr;
    static constexpr ShaderModuleConfig CONFIG{
        .name = NAME,
        .fragmentSource = FRAGMENT_SOURCE,
        .vertexSource = VERTEX_SOURCE,
    };
    static constexpr std::string_view UNIFORM_TEXEL_SIZE = "texelSize";
    static constexpr std::string_view UNIFORM_BLUR_RADIUS = "blurRadius";
};

} // namespace biofuel::utils::render::shader
