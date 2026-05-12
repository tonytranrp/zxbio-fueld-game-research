#pragma once

#include "engine/core/Types.hpp"
#include "engine/runtime/typed/ServiceTags.hpp"
#include "engine/ui/typed/ScreenTypes.hpp"

namespace biofuel::engine::ui {
class ScreenManager;
}

namespace biofuel::engine::ui::typed {

enum class ResumeReason : u8 {
    Unknown,
    Popped,
    Replaced,
    Cleared,
};

struct LifecycleContext {
    ScreenManager& manager;
    ::biofuel::engine::runtime::typed::AppServices* services = nullptr;
    ScreenId screenId = ScreenId::Unknown;
};

struct UpdateContext {
    ScreenManager& manager;
    ::biofuel::engine::runtime::typed::AppServices* services = nullptr;
    ScreenId screenId = ScreenId::Unknown;
    f32 deltaTime = 0.0f;
};

struct InputContext {
    ScreenManager& manager;
    ::biofuel::engine::runtime::typed::AppServices* services = nullptr;
    ScreenId screenId = ScreenId::Unknown;
};

struct ResumeContext {
    ScreenManager& manager;
    ::biofuel::engine::runtime::typed::AppServices* services = nullptr;
    ScreenId screenId = ScreenId::Unknown;
    ScreenId poppedScreenId = ScreenId::Unknown;
    ResumeReason reason = ResumeReason::Unknown;
};

} // namespace biofuel::engine::ui::typed
