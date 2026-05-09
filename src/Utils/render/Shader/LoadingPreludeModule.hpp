#pragma once

#include "Utils/render/Shader/ShaderModule.hpp"
#include "ShaderSources.hpp"

namespace biofuel::utils::render::shader {

class LoadingPreludeModule {
public:
    static constexpr std::string_view NAME = "loading_prelude";
    static constexpr std::string_view FRAGMENT_SOURCE = shader_source::loading_prelude_source;
    static constexpr const char* VERTEX_SOURCE = nullptr;
    static constexpr ShaderModuleConfig CONFIG{
        .name = NAME,
        .fragmentSource = FRAGMENT_SOURCE,
        .vertexSource = VERTEX_SOURCE,
    };

    static constexpr std::string_view UNIFORM_IRESOLUTION = "iResolution";
    static constexpr std::string_view UNIFORM_ITIME = "iTime";
    static constexpr std::string_view UNIFORM_UBRIGHTNESS = "uBrightness";
    static constexpr std::string_view UNIFORM_UREVEAL_PROGRESS = "uRevealProgress";
};

} // namespace biofuel::utils::render::shader
