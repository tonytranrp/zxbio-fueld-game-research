#pragma once

#include "engine/ui/typed/ScreenTypes.hpp"
#include <tuple>

namespace biofuel::engine::ui::typed {

template<typename TScreen>
struct ScreenSpec {
    static constexpr ScreenId ID = ScreenId::Unknown;
    static constexpr std::string_view NAME = "UnknownScreen";
};

template<typename TScreen>
struct TransitionPolicy {
    static constexpr TransitionPolicyData VALUE{};
};

template<typename TScreen>
struct StackPolicy {
    static constexpr StackPolicyData VALUE{};
};

template<typename TScreen>
struct RenderLayers {
    using Type = std::tuple<>;
};

template<typename TScreen>
inline constexpr ScreenId screenIdOf = ScreenSpec<std::remove_cvref_t<TScreen>>::ID;

template<typename TScreen>
inline constexpr std::string_view screenNameOf = ScreenSpec<std::remove_cvref_t<TScreen>>::NAME;

template<typename TScreen>
inline constexpr TransitionPolicyData transitionPolicyOf =
    TransitionPolicy<std::remove_cvref_t<TScreen>>::VALUE;

template<typename TScreen>
inline constexpr StackPolicyData stackPolicyOf =
    StackPolicy<std::remove_cvref_t<TScreen>>::VALUE;

} // namespace biofuel::engine::ui::typed
