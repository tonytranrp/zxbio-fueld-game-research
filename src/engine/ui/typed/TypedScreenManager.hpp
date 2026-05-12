#pragma once

#include "engine/ui/typed/ScreenRegistry.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"
#include <memory>
#include <type_traits>
#include <utility>

namespace biofuel::engine::ui {

class Screen;

template<typename TRegistry>
class TypedScreenManager {
public:
    template<typename TScreen, typename... TArgs>
    std::unique_ptr<Screen> make(TArgs&&... args) const {
        using CleanScreen = std::remove_cvref_t<TScreen>;
        static_assert(
            TRegistry::template contains<CleanScreen>,
            "TypedScreenManager::make<TScreen> used with a screen not registered in ScreenRegistry");
        return std::make_unique<CleanScreen>(std::forward<TArgs>(args)...);
    }
};

} // namespace biofuel::engine::ui
