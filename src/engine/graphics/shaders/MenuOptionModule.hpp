#pragma once

#include "engine/runtime/typed/ShaderDeclare.hpp"
#include "engine/graphics/shaders/ShaderModule.hpp"
#include "ShaderSources.hpp"

namespace biofuel::engine::graphics::shader {

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

} // namespace biofuel::engine::graphics::shader

namespace biofuel::engine::runtime::typed::shader {
struct MenuOption {};
namespace menu_option {
BIOFUEL_SHADER_UNIFORM(Time, ::biofuel::engine::graphics::shader::MenuOptionModule::UNIFORM_ITIME, SHADER_UNIFORM_FLOAT);
BIOFUEL_SHADER_UNIFORM(Center, ::biofuel::engine::graphics::shader::MenuOptionModule::UNIFORM_ICENTER, SHADER_UNIFORM_VEC2);
BIOFUEL_SHADER_UNIFORM(HalfSize, ::biofuel::engine::graphics::shader::MenuOptionModule::UNIFORM_IHALF_SIZE, SHADER_UNIFORM_VEC2);
BIOFUEL_SHADER_UNIFORM(SelectionStrength, ::biofuel::engine::graphics::shader::MenuOptionModule::UNIFORM_SELECTION_STRENGTH, SHADER_UNIFORM_FLOAT);
BIOFUEL_SHADER_UNIFORM(HoverStrength, ::biofuel::engine::graphics::shader::MenuOptionModule::UNIFORM_HOVER_STRENGTH, SHADER_UNIFORM_FLOAT);
} // namespace menu_option
} // namespace biofuel::engine::runtime::typed::shader

namespace biofuel::engine::runtime::typed {
BIOFUEL_EMBEDDED_SHADER_ASSET(
    shader::MenuOption,
    ::biofuel::engine::graphics::shader::MenuOptionModule,
    true,
    shader::menu_option::Time,
    shader::menu_option::Center,
    shader::menu_option::HalfSize,
    shader::menu_option::SelectionStrength,
    shader::menu_option::HoverStrength);
BIOFUEL_SHADER_MODULE(MenuOptionShaderModule, shader::MenuOption)
} // namespace biofuel::engine::runtime::typed
