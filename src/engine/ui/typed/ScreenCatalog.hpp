#pragma once

#include "engine/ui/typed/ScreenRegistry.hpp"
#include "engine/ui/typed/ScreenSpec.hpp"
#include "engine/ui/typed/ScreenValidation.hpp"

namespace biofuel::engine::ui::typed {

using AppScreenRegistry = ScreenRegistry<>;

[[nodiscard]] constexpr TransitionPolicyData transitionPolicyForId(const ScreenId) noexcept {
    return TransitionPolicyData{};
}

[[nodiscard]] constexpr StackPolicyData stackPolicyForId(const ScreenId) noexcept {
    return StackPolicyData{};
}

} // namespace biofuel::engine::ui::typed
