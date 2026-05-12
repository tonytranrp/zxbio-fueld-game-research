#pragma once

#include "engine/ui/typed/ScreenModule.hpp"
#include <memory>
#include <type_traits>
#include <utility>

namespace biofuel::engine::ui {
class Screen;
}

namespace biofuel::engine::ui::typed {

template<typename TScreen>
struct ScreenRuntime {
    using Screen = std::remove_cvref_t<TScreen>;
    using Module = ScreenModule<Screen>;
    using State = typename Module::State;

    template<typename... TArgs>
    [[nodiscard]] static std::unique_ptr<::biofuel::engine::ui::Screen> makeBridge(TArgs&&... args) {
        return std::make_unique<Screen>(std::forward<TArgs>(args)...);
    }
};

} // namespace biofuel::engine::ui::typed
