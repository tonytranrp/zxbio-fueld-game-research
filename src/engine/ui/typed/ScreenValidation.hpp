#pragma once

#include "engine/ui/typed/RenderLayers.hpp"
#include "engine/ui/typed/ScreenModule.hpp"
#include "engine/ui/typed/ScreenRegistry.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"
#include <concepts>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace biofuel::engine::ui::typed {

namespace detail {

template<typename TScreen, typename TLayerList>
struct RenderLayerListMatchesScreen : std::false_type {};

template<typename TScreen, typename... TLayers>
struct RenderLayerListMatchesScreen<TScreen, RenderLayerList<TScreen, TLayers...>> : std::true_type {};

template<typename T>
concept ValidationNamedRenderNode = requires {
    { T::NAME } -> std::convertible_to<std::string_view>;
};

template<typename TNode>
consteval bool validateNamedRenderNode() {
    if constexpr (ValidationNamedRenderNode<TNode>) {
        static_assert(TNode::NAME.size() > 0U,
            "Named render layers/elements must expose a non-empty NAME.");
    }
    return true;
}

template<typename TLayer>
struct RenderLayerNodeValidator {
    static consteval bool valid() {
        return validateNamedRenderNode<TLayer>();
    }
};

template<typename... TElements>
struct RenderLayerNodeValidator<RenderElementList<TElements...>> {
    static consteval bool valid() {
        static_assert((validateNamedRenderNode<TElements>() && ...),
            "Every named render element in RenderElementList must have a non-empty NAME.");
        return true;
    }
};

template<typename TLayerList>
struct RenderLayerNodesValidator;

template<typename TScreen, typename... TLayers>
struct RenderLayerNodesValidator<RenderLayerList<TScreen, TLayers...>> {
    static consteval bool valid() {
        static_assert((RenderLayerNodeValidator<TLayers>::valid() && ...),
            "Every named render layer in RenderLayerList must have a non-empty NAME.");
        return true;
    }
};

template<typename TScreen>
consteval bool validateScreenModule() {
    using CleanScreen = std::remove_cvref_t<TScreen>;
    using Module = ScreenModule<CleanScreen>;
    using State = typename Module::State;
    using Layers = typename RenderLayers<CleanScreen>::Type;

    static_assert(ScreenSpec<CleanScreen>::ID != ScreenId::Unknown,
        "Registered screen must specialize ScreenSpec<T>::ID");
    static_assert(ScreenSpec<CleanScreen>::NAME.size() > 0,
        "Registered screen must specialize ScreenSpec<T>::NAME");
    static_assert(std::is_same_v<State, ScreenState<CleanScreen>> || !std::is_void_v<State>,
        "Registered screen module must expose a usable State type");
    static_assert(RenderLayerListMatchesScreen<CleanScreen, Layers>::value,
        "Registered screen RenderLayers<T>::Type must be RenderLayerList<T, ...>");
    static_assert(RenderLayerNodesValidator<Layers>::valid(),
        "Registered screen render layers and elements must be valid.");
    if constexpr (TransitionPolicy<CleanScreen>::VALUE.composer == TransitionComposer::Crossfade) {
        static_assert(RenderLayerListMatchesScreen<CleanScreen, Layers>::value,
            "Crossfade screens must use a typed RenderLayerList<TScreen, ...> pipeline.");
    }
    static_assert(requires(CleanScreen& screen, LifecycleContext& lifecycle) {
        Module::onEnter(screen, lifecycle);
        Module::onExit(screen, lifecycle);
        Module::onPause(screen, lifecycle);
    }, "Registered screen module must expose lifecycle hooks");
    static_assert(requires(CleanScreen& screen, ResumeContext& resume) {
        Module::onResume(screen, resume);
    }, "Registered screen module must expose typed resume hook");
    static_assert(requires(CleanScreen& screen, UpdateContext& update) {
        Module::onUpdate(screen, update);
    }, "Registered screen module must expose update hook");
    static_assert(requires(CleanScreen& screen, InputContext& input) {
        Module::onInput(screen, input);
    }, "Registered screen module must expose input hook");
    static_assert(requires(CleanScreen& screen, RenderContext& render) {
        Module::onRender(screen, render);
    }, "Registered screen module must expose render hook");

    return true;
}

template<typename... TScreens>
consteval bool screenIdsAreUnique() {
    constexpr ScreenId ids[] = {ScreenSpec<TScreens>::ID...};
    for (size_t i = 0; i < sizeof...(TScreens); ++i) {
        for (size_t j = i + 1; j < sizeof...(TScreens); ++j) {
            if (ids[i] == ids[j]) {
                return false;
            }
        }
    }
    return true;
}

template<typename... TScreens>
consteval bool screenNamesAreUnique() {
    constexpr std::string_view names[] = {ScreenSpec<TScreens>::NAME...};
    for (size_t i = 0; i < sizeof...(TScreens); ++i) {
        for (size_t j = i + 1; j < sizeof...(TScreens); ++j) {
            if (names[i] == names[j]) {
                return false;
            }
        }
    }
    return true;
}

template<typename TTuple>
struct RegistryValidator;

template<typename... TScreens>
struct RegistryValidator<std::tuple<TScreens...>> {
    static consteval bool validate() {
        static_assert((validateScreenModule<TScreens>() && ...),
            "Every registered screen must have a valid typed screen module");
        static_assert(screenIdsAreUnique<TScreens...>(),
            "Every registered screen must have a unique ScreenId");
        static_assert(screenNamesAreUnique<TScreens...>(),
            "Every registered screen must have a unique ScreenSpec<T>::NAME");
        return true;
    }
};

} // namespace detail

template<typename TRegistry>
consteval bool validateScreenRegistry() {
    return detail::RegistryValidator<typename TRegistry::Screens>::validate();
}

} // namespace biofuel::engine::ui::typed
