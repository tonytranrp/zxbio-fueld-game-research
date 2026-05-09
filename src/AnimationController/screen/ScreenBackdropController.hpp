#pragma once

#include "Core/Types.hpp"
#include <raylib.h>
#include <string_view>

namespace biofuel::animation::screen {

struct ScreenBackdropConfig {
    std::string_view shaderName;
    Color fallbackColor = BLACK;
    f32 revealDelay = 0.0f;
    f32 revealDuration = 1.0f;
    f32 brightnessFloor = 0.0f;
    f32 brightnessCeiling = 1.0f;
    f32 transitionWeight = 0.4f;
    f32 revealWeight = 0.6f;
};

class ScreenBackdropController {
public:
    void configure(const ScreenBackdropConfig& config) noexcept;
    void reset() noexcept;
    void update(f32 dt) noexcept;
    void render(f32 transitionAlpha) const;

    [[nodiscard]] f32 revealProgress() const noexcept;
    [[nodiscard]] bool ready() const noexcept;

private:
    void ensureShader() const;

    ScreenBackdropConfig m_config{};
    mutable Shader m_shader{};
    mutable i32 m_resolutionLoc = -1;
    mutable i32 m_timeLoc = -1;
    mutable i32 m_brightnessLoc = -1;
    mutable i32 m_revealLoc = -1;
    mutable bool m_shaderReady = false;

    f32 m_time = 0.0f;
    f32 m_revealElapsed = 0.0f;
};

} // namespace biofuel::animation::screen
