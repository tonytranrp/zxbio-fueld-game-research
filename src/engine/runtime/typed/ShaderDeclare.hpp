#pragma once

#include "engine/core/typed/TypedModule.hpp"
#include "engine/core/Types.hpp"
#include "engine/runtime/typed/AssetBase.hpp"
#include <raylib.h>
#include <string_view>

#define BIOFUEL_SHADER_UNIFORM(TAG_NAME, UNIFORM_NAME, UNIFORM_KIND) \
    struct TAG_NAME { \
        static constexpr std::string_view Name = UNIFORM_NAME; \
        static constexpr i32 UniformType = UNIFORM_KIND; \
    }

#define BIOFUEL_EMBEDDED_SHADER_ASSET(TAG_TYPE, RUNTIME_MODULE, ...) \
    template<> struct ShaderAsset<TAG_TYPE> { \
        using Tag = TAG_TYPE; \
        using RuntimeModule = RUNTIME_MODULE; \
        using Uniforms = ::biofuel::typed::Registry<__VA_ARGS__>; \
        static constexpr std::string_view Name = RuntimeModule::NAME; \
        static constexpr const char* VertexSource = RuntimeModule::VERTEX_SOURCE; \
        static constexpr std::string_view FragmentSource = RuntimeModule::FRAGMENT_SOURCE; \
    }

#define BIOFUEL_SHADER_MODULE(MODULE_NAME, ...) \
    namespace shaders { \
    BIOFUEL_TYPED_REGISTRY_MODULE(MODULE_NAME, __VA_ARGS__); \
    } \
    BIOFUEL_TYPED_MODULE(shader, ShaderAssetRegistry, ::biofuel::engine::runtime::typed::shaders::MODULE_NAME)
