#pragma once

#include "engine/core/typed/TypedModule.hpp"
#include "engine/runtime/typed/AssetBase.hpp"
#include <string_view>

#define BIOFUEL_VIDEO_ASSET(TAG_TYPE, ASSET_NAME, ASSET_PATH, PRELOAD) \
    template<> struct VideoAsset<TAG_TYPE> { \
        using Tag = TAG_TYPE; \
        static constexpr std::string_view Name = ASSET_NAME; \
        static constexpr std::string_view Path = ASSET_PATH; \
        static constexpr bool PreloadOnStartup = PRELOAD; \
    }

#define BIOFUEL_SOUND_ASSET(TAG_TYPE, ASSET_NAME, ASSET_PATH, PRELOAD) \
    template<> struct SoundAsset<TAG_TYPE> { \
        using Tag = TAG_TYPE; \
        static constexpr std::string_view Name = ASSET_NAME; \
        static constexpr std::string_view Path = ASSET_PATH; \
        static constexpr bool PreloadOnStartup = PRELOAD; \
    }

#define BIOFUEL_MUSIC_ASSET(TAG_TYPE, ASSET_NAME, ASSET_PATH, PRELOAD) \
    template<> struct MusicAsset<TAG_TYPE> { \
        using Tag = TAG_TYPE; \
        static constexpr std::string_view Name = ASSET_NAME; \
        static constexpr std::string_view Path = ASSET_PATH; \
        static constexpr bool PreloadOnStartup = PRELOAD; \
    }

#define BIOFUEL_MODEL_ASSET(TAG_TYPE, ASSET_ID, ASSET_NAME, PRELOAD) \
    template<> struct ModelAsset<TAG_TYPE> { \
        using Tag = TAG_TYPE; \
        static constexpr auto Id = ASSET_ID; \
        static constexpr std::string_view Name = ASSET_NAME; \
        static constexpr bool PreloadOnStartup = PRELOAD; \
    }

#define BIOFUEL_ASSET_MODULE(MODULE_NAME, REGISTRY_ALIAS, ...) \
    namespace assets { \
    BIOFUEL_TYPED_REGISTRY_MODULE(MODULE_NAME, __VA_ARGS__); \
    } \
    BIOFUEL_TYPED_MODULE(asset, REGISTRY_ALIAS, ::biofuel::engine::runtime::typed::assets::MODULE_NAME)

