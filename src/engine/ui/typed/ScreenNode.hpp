#pragma once

#include "engine/ui/Screen.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"

namespace biofuel::engine::ui::typed {

template<typename TDerived>
class ScreenNode : public Screen {
public:
    [[nodiscard]] ScreenId screenId() const noexcept override {
        return ScreenSpec<TDerived>::ID;
    }

    [[nodiscard]] std::string_view getName() const noexcept override {
        return ScreenSpec<TDerived>::NAME;
    }

protected:
    [[nodiscard]] TDerived& self() noexcept {
        return static_cast<TDerived&>(*this);
    }

    [[nodiscard]] const TDerived& self() const noexcept {
        return static_cast<const TDerived&>(*this);
    }
};

} // namespace biofuel::engine::ui::typed
