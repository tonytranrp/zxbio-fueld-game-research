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

    static constexpr std::string_view UNIFORM_IRESOLUTION = "iResolution";
    static constexpr std::string_view UNIFORM_ITIME = "iTime";
    static constexpr std::string_view UNIFORM_UBRIGHTNESS = "uBrightness";
    static constexpr std::string_view UNIFORM_UREVEAL_PROGRESS = "uRevealProgress";
};

} // namespace biofuel::engine::graphics::shader

namespace biofuel::engine::runtime::typed::shader {
struct LoadingPrelude {};
namespace loading_prelude {
BIOFUEL_SHADER_UNIFORM(IResolution, ::biofuel::engine::graphics::shader::LoadingPreludeModule::UNIFORM_IRESOLUTION, SHADER_UNIFORM_VEC3);
BIOFUEL_SHADER_UNIFORM(ITime, ::biofuel::engine::graphics::shader::LoadingPreludeModule::UNIFORM_ITIME, SHADER_UNIFORM_FLOAT);
BIOFUEL_SHADER_UNIFORM(Brightness, ::biofuel::engine::graphics::shader::LoadingPreludeModule::UNIFORM_UBRIGHTNESS, SHADER_UNIFORM_FLOAT);
BIOFUEL_SHADER_UNIFORM(RevealProgress, ::biofuel::engine::graphics::shader::LoadingPreludeModule::UNIFORM_UREVEAL_PROGRESS, SHADER_UNIFORM_FLOAT);
} // namespace loading_prelude
} // namespace biofuel::engine::runtime::typed::shader

namespace biofuel::engine::runtime::typed {
BIOFUEL_EMBEDDED_SHADER_ASSET(
    shader::LoadingPrelude,
    ::biofuel::engine::graphics::shader::LoadingPreludeModule,
    true,
    shader::loading_prelude::IResolution,
    shader::loading_prelude::ITime,
    shader::loading_prelude::Brightness,
    shader::loading_prelude::RevealProgress);
BIOFUEL_SHADER_MODULE(LoadingPreludeShaderModule, shader::LoadingPrelude)
} // namespace biofuel::engine::runtime::typed
