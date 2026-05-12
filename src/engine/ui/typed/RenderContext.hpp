#pragma once

#include "engine/core/Types.hpp"
#include "engine/runtime/typed/ServiceTags.hpp"
#include "engine/ui/typed/ScreenTypes.hpp"
#include <string_view>

namespace biofuel::engine::ui {
class ScreenManager;
}

namespace biofuel::engine::ui::typed {

struct RenderContext {
    ScreenManager* manager = nullptr;
    ::biofuel::engine::runtime::typed::AppServices* services = nullptr;
    ScreenId screenId = ScreenId::Unknown;
    i32 screenWidth = 0;
    i32 screenHeight = 0;
    f32 transitionAlpha = 1.0f;
    f32 frameTime = 0.0f;

    [[nodiscard]] bool layerEnabled(std::string_view layerName) const noexcept;
};

} // namespace biofuel::engine::ui::typed
