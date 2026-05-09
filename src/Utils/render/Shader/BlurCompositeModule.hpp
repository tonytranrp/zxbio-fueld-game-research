#pragma once

#include "Utils/render/Shader/ShaderModule.hpp"
#include "ShaderSources.hpp"

namespace biofuel::utils::render::shader {

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

} // namespace biofuel::utils::render::shader
