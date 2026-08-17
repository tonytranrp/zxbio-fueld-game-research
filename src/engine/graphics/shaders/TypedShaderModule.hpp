#pragma once

#include "engine/runtime/typed/AssetBase.hpp"
#include "engine/runtime/typed/Services.hpp"
#include "engine/graphics/ShaderManager.hpp"
#include <raylib.h>

namespace biofuel::engine::runtime::typed {

template<typename TShader>
struct ShaderModule {
    using Shader = TShader;
    using Asset = ShaderAsset<TShader>;

    static void load(::biofuel::engine::graphics::ShaderManager& shaders) {
        if constexpr (requires { Asset::FragmentSource; }) {
            shaders.loadFromMemory(Asset::Name.data(), Asset::VertexSource, Asset::FragmentSource.data());
        } else {
            shaders.load(Asset::Name, Asset::VertexPath, Asset::FragmentPath);
        }
    }

    static void load(AppServices& services) {
        load(services.get<ShaderService>());
    }

    [[nodiscard]] static ::Shader shader(const ::biofuel::engine::graphics::ShaderManager& shaders) noexcept {
        return shaders.get(Asset::Name);
    }

    [[nodiscard]] static ::Shader shader(AppServices& services) noexcept {
        return shader(services.get<ShaderService>());
    }

    [[nodiscard]] static bool loaded(const ::biofuel::engine::graphics::ShaderManager& shaders) noexcept {
        return shaders.has(Asset::Name);
    }
};

template<typename TShader, typename TUniform>
struct ShaderUniform {
    [[nodiscard]] static i32 location(::Shader shader) noexcept {
        return ::biofuel::engine::graphics::ShaderManager::getLocation(shader, TUniform::Name);
    }

    static void set(::Shader shader, i32 location, const void* value) noexcept {
        ::biofuel::engine::graphics::ShaderManager::setValue(shader, location, value, TUniform::UniformType);
    }

    static void setTexture(::Shader shader, i32 location, Texture2D texture) noexcept {
        ::biofuel::engine::graphics::ShaderManager::setValueTexture(shader, location, texture);
    }
};

class Shaders {
public:
    template<typename TShader>
    static void load(::biofuel::engine::graphics::ShaderManager& shaders) {
        ShaderModule<TShader>::load(shaders);
    }

    template<typename TShader>
    [[nodiscard]] static ::Shader get(const ::biofuel::engine::graphics::ShaderManager& shaders) noexcept {
        return ShaderModule<TShader>::shader(shaders);
    }

    template<typename TShader>
    [[nodiscard]] static ::Shader get() noexcept {
        return get<TShader>(ServiceModule<ShaderService>::get());
    }

    template<typename TShader>
    [[nodiscard]] static bool loaded(const ::biofuel::engine::graphics::ShaderManager& shaders) noexcept {
        return ShaderModule<TShader>::loaded(shaders);
    }

    template<typename TShader, typename TUniform>
    [[nodiscard]] static i32 loc(::Shader shader) noexcept {
        return ShaderUniform<TShader, TUniform>::location(shader);
    }

    template<typename TShader, typename TUniform>
    static void set(::Shader shader, i32 location, const void* value) noexcept {
        ShaderUniform<TShader, TUniform>::set(shader, location, value);
    }

    template<typename TShader, typename TUniform>
    static void setTexture(::Shader shader, i32 location, Texture2D texture) noexcept {
        ShaderUniform<TShader, TUniform>::setTexture(shader, location, texture);
    }
};

} // namespace biofuel::engine::runtime::typed
