#pragma once

#include "Utils/render/Shader/ShaderModule.hpp"
#include "ShaderSources.hpp"

namespace biofuel::utils::render::shader {

class MenuOptionModule {
public:
    static constexpr std::string_view NAME = "menu_option";
    static constexpr std::string_view FRAGMENT_SOURCE = shader_source::menu_option_source;
    static constexpr const char* VERTEX_SOURCE = nullptr;
    static constexpr ShaderModuleConfig CONFIG{
        .name = NAME,
        .fragmentSource = FRAGMENT_SOURCE,
        .vertexSource = VERTEX_SOURCE,
    };

    static constexpr std::string_view UNIFORM_ITIME = "uTime";
    static constexpr std::string_view UNIFORM_ICENTER = "uCenter";
    static constexpr std::string_view UNIFORM_IHALF_SIZE = "uHalfSize";
    static constexpr std::string_view UNIFORM_SELECTION_STRENGTH = "uSelectionStrength";
    static constexpr std::string_view UNIFORM_HOVER_STRENGTH = "uHoverStrength";
};

} // namespace biofuel::utils::render::shader
