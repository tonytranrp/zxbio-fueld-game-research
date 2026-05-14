#pragma once

#include <tuple>
#include <type_traits>

namespace biofuel::engine::ui::typed {

template<typename... TScreens>
struct ScreenRegistry {
    using Screens = std::tuple<TScreens...>;
    static constexpr auto size = sizeof...(TScreens);

    template<typename TScreen>
    static constexpr bool contains =
        (std::is_same_v<std::remove_cvref_t<TScreen>, TScreens> || ...);
};

struct UnvalidatedScreenRegistry {
    template<typename TScreen>
    static constexpr bool contains = true;
};

} // namespace biofuel::engine::ui::typed
