#pragma once

#include "engine/runtime/typed/ShaderDeclare.hpp"
#include "engine/graphics/shaders/ShaderModule.hpp"
#include "ShaderSources.hpp"

namespace biofuel::engine::graphics::shader {

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

} // namespace biofuel::engine::graphics::shader

namespace biofuel::engine::runtime::typed::shader {
struct BlurH {};
namespace blur_h {
BIOFUEL_SHADER_UNIFORM(TexelSize, ::biofuel::engine::graphics::shader::BlurHModule::UNIFORM_TEXEL_SIZE, SHADER_UNIFORM_VEC2);
BIOFUEL_SHADER_UNIFORM(Radius, ::biofuel::engine::graphics::shader::BlurHModule::UNIFORM_BLUR_RADIUS, SHADER_UNIFORM_FLOAT);
} // namespace blur_h
} // namespace biofuel::engine::runtime::typed::shader

namespace biofuel::engine::runtime::typed {
BIOFUEL_EMBEDDED_SHADER_ASSET(
    shader::BlurH,
    ::biofuel::engine::graphics::shader::BlurHModule,
    shader::blur_h::TexelSize,
    shader::blur_h::Radius);
BIOFUEL_SHADER_MODULE(BlurHShaderModule, shader::BlurH)
} // namespace biofuel::engine::runtime::typed
