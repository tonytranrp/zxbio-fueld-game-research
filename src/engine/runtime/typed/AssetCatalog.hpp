#pragma once

#include "engine/core/typed/Meta.hpp"
#include "engine/runtime/typed/Assets.hpp"
#include <concepts>
#include <string_view>
#include <type_traits>
#include <utility>

namespace biofuel::engine::runtime::typed {

template<typename TCatalogTag>
struct AssetCatalog;

template<typename TAssetTag>
struct AssetEntry {
    using Tag = TAssetTag;
};

template<typename TShaderTag>
struct ShaderEntry {
    using Tag = TShaderTag;
};

template<typename TModelTag>
struct ModelEntry {
    using Tag = TModelTag;
};

template<typename TSoundTag>
struct SoundEntry {
    using Tag = TSoundTag;
};

template<typename TMusicTag>
struct MusicEntry {
    using Tag = TMusicTag;
};

template<typename TVideoTag>
struct VideoEntry {
    using Tag = TVideoTag;
};

template<typename TEntry>
struct CatalogEntryTraits {
    using Tag = TEntry;
};

template<typename TAssetTag>
struct CatalogEntryTraits<AssetEntry<TAssetTag>> {
    using Tag = TAssetTag;
};

template<typename TShaderTag>
struct CatalogEntryTraits<ShaderEntry<TShaderTag>> {
    using Tag = TShaderTag;
};

template<typename TModelTag>
struct CatalogEntryTraits<ModelEntry<TModelTag>> {
    using Tag = TModelTag;
};

template<typename TSoundTag>
struct CatalogEntryTraits<SoundEntry<TSoundTag>> {
    using Tag = TSoundTag;
};

template<typename TMusicTag>
struct CatalogEntryTraits<MusicEntry<TMusicTag>> {
    using Tag = TMusicTag;
};

template<typename TVideoTag>
struct CatalogEntryTraits<VideoEntry<TVideoTag>> {
    using Tag = TVideoTag;
};

template<typename TEntry>
using CatalogEntryTag = typename CatalogEntryTraits<TEntry>::Tag;

template<typename T>
concept ShaderCatalogEntry =
    std::is_same_v<T, ShaderEntry<CatalogEntryTag<T>>>
    || requires { typename ShaderAsset<CatalogEntryTag<T>>::Tag; };

template<typename T>
concept ModelCatalogEntry =
    std::is_same_v<T, ModelEntry<CatalogEntryTag<T>>>
    || requires { typename ModelAsset<CatalogEntryTag<T>>::Tag; };

template<typename T>
concept SoundCatalogEntry =
    std::is_same_v<T, SoundEntry<CatalogEntryTag<T>>>
    || requires { typename SoundAsset<CatalogEntryTag<T>>::Tag; };

template<typename T>
concept MusicCatalogEntry =
    std::is_same_v<T, MusicEntry<CatalogEntryTag<T>>>
    || requires { typename MusicAsset<CatalogEntryTag<T>>::Tag; };

template<typename T>
concept VideoCatalogEntry =
    std::is_same_v<T, VideoEntry<CatalogEntryTag<T>>>
    || requires { typename VideoAsset<CatalogEntryTag<T>>::Tag; };

template<typename TEntry>
struct CatalogEntryValidator {
    using Tag = CatalogEntryTag<TEntry>;

    static consteval bool valid() {
        if constexpr (ShaderCatalogEntry<TEntry>) {
            static_assert(ShaderAssetRegistry::template contains<Tag>,
                "Shader catalog entry must be registered in ShaderAssetRegistry.");
            return true;
        } else if constexpr (ModelCatalogEntry<TEntry>
            || SoundCatalogEntry<TEntry>
            || MusicCatalogEntry<TEntry>
            || VideoCatalogEntry<TEntry>) {
            static_assert(AppAssetRegistry::template contains<Tag>,
                "Asset catalog entry must be registered in AppAssetRegistry.");
            return true;
        } else {
            static_assert(AppAssetRegistry::template contains<Tag> || ShaderAssetRegistry::template contains<Tag>,
                "Asset catalog entry must be a registered asset or shader tag.");
            return true;
        }
    }
};

template<typename TCatalogTag>
concept RegisteredAssetCatalog = requires {
    typename AssetCatalog<TCatalogTag>::Assets;
};

template<typename TCatalogTag>
struct AssetCatalogView {
    static_assert(RegisteredAssetCatalog<TCatalogTag>, "Catalog must specialize AssetCatalog<TCatalogTag>.");

    using Assets = typename AssetCatalog<TCatalogTag>::Assets;

    template<typename TFn>
    static constexpr void forEach(TFn&& fn) {
        forEachImpl(Assets{}, std::forward<TFn>(fn));
    }

private:
    template<typename... TEntries, typename TFn>
    static constexpr void forEachImpl(::biofuel::typed::Registry<TEntries...>, TFn&& fn) {
        (fn.template operator()<TEntries>(), ...);
    }
};

template<typename TCatalogTag>
struct AssetCatalogValidator {
    using Assets = typename AssetCatalog<TCatalogTag>::Assets;

    static consteval bool valid() {
        return validImpl(Assets{});
    }

private:
    template<typename... TEntries>
    static consteval bool validImpl(::biofuel::typed::Registry<TEntries...>) {
        static_assert(Assets::valid(), "Asset catalog entries must be unique.");
        static_assert((CatalogEntryValidator<TEntries>::valid() && ...),
            "Every asset catalog entry must already be registered.");
        return true;
    }
};

template<typename TCatalogTag>
consteval bool validateAssetCatalog() {
    return AssetCatalogValidator<TCatalogTag>::valid();
}

struct EngineStartupCatalog;

template<>
struct AssetCatalog<EngineStartupCatalog> {
    using Assets = ::biofuel::typed::Registry<
        ShaderEntry<shader::BlurH>,
        ShaderEntry<shader::BlurV>,
        ShaderEntry<shader::BlurComposite>,
        ShaderEntry<shader::Crossfade>,
        ShaderEntry<shader::LoadingPrelude>,
        ShaderEntry<shader::MenuOption>,
        ShaderEntry<shader::MainMenuBg>,
        VideoEntry<video::IdleAmbient>,
        SoundEntry<sound::MenuMove>,
        SoundEntry<sound::MenuAccept>,
        MusicEntry<music::IdleAmbient>>;
};

static_assert(validateAssetCatalog<EngineStartupCatalog>());

} // namespace biofuel::engine::runtime::typed
