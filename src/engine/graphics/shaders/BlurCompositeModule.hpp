#pragma once

#include "engine/runtime/typed/ShaderDeclare.hpp"
#include "engine/graphics/shaders/ShaderModule.hpp"
#include "ShaderSources.hpp"

namespace biofuel::engine::graphics::shader {

class BlurCompositeModule {
public:
    static constexpr std::string_view NAME = "blur_composite";
    static constexpr std::string_view FRAGMENT_SOURCE = shader_source::blur_composite_source;
    static constexpr const char* VERTEX_SOURCE = nullptr;
    static constexpr ShaderModuleConfig CONFIG{
        .name = NAME,
        .fragmentSource = FRAGMENT_SOURCE,
        .vertexSource = VERTEX_SOURCE,
    };

    static constexpr std::string_view UNIFORM_DESATURATION = "uDesaturation";
    static constexpr std::string_view UNIFORM_VIGNETTE_STRENGTH = "uVignetteStrength";
    static constexpr std::string_view UNIFORM_DIM_STRENGTH = "uDimStrength";
};

} // namespace biofuel::engine::graphics::shader

namespace biofuel::engine::runtime::typed::shader {
struct BlurComposite {};
namespace blur_composite {
BIOFUEL_SHADER_UNIFORM(Desaturation, ::biofuel::engine::graphics::shader::BlurCompositeModule::UNIFORM_DESATURATION, SHADER_UNIFORM_FLOAT);
BIOFUEL_SHADER_UNIFORM(VignetteStrength, ::biofuel::engine::graphics::shader::BlurCompositeModule::UNIFORM_VIGNETTE_STRENGTH, SHADER_UNIFORM_FLOAT);
BIOFUEL_SHADER_UNIFORM(DimStrength, ::biofuel::engine::graphics::shader::BlurCompositeModule::UNIFORM_DIM_STRENGTH, SHADER_UNIFORM_FLOAT);
} // namespace blur_composite
} // namespace biofuel::engine::runtime::typed::shader

namespace biofuel::engine::runtime::typed {
BIOFUEL_EMBEDDED_SHADER_ASSET(
    shader::BlurComposite,
    ::biofuel::engine::graphics::shader::BlurCompositeModule,
    shader::blur_composite::Desaturation,
    shader::blur_composite::VignetteStrength,
    shader::blur_composite::DimStrength);
BIOFUEL_SHADER_MODULE(BlurCompositeShaderModule, shader::BlurComposite)
} // namespace biofuel::engine::runtime::typed
