#pragma once

#include "engine/runtime/typed/ShaderDeclare.hpp"
#include "engine/graphics/shaders/ShaderModule.hpp"
#include "ShaderSources.hpp"

namespace biofuel::engine::graphics::shader {

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
};

} // namespace biofuel::engine::graphics::shader

namespace biofuel::engine::runtime::typed::shader {
struct LoadingPrelude {};
} // namespace biofuel::engine::runtime::typed::shader

namespace biofuel::engine::runtime::typed {
BIOFUEL_EMBEDDED_SHADER_ASSET(
    shader::LoadingPrelude,
    ::biofuel::engine::graphics::shader::LoadingPreludeModule);
BIOFUEL_SHADER_MODULE(LoadingPreludeShaderModule, shader::LoadingPrelude)
} // namespace biofuel::engine::runtime::typed
