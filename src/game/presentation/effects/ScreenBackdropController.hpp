#pragma once

#include "engine/core/Types.hpp"
#include "engine/graphics/ShaderManager.hpp"
#include <raylib.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <functional>

namespace biofuel::game::presentation::effects {

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
    void restartReveal() noexcept;
    void update(f32 dt) noexcept;
    void render(f32 transitionAlpha) const;

    // Set an arbitrary float uniform on the backdrop shader.
    // The uniform is applied on the next render() call.
    void setFloat(std::string_view uniformName, f32 value) const;

    // Get the underlying Raylib Shader handle for component apply().
    // Returns a default Shader{} if not yet loaded.
    [[nodiscard]] Shader shader() const noexcept;

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

    // Cached arbitrary uniform locations (for setFloat calls).
    // Uses transparent hash so find(string_view) avoids heap allocation.
    mutable std::unordered_map<std::string, i32,
        TransparentHash, std::equal_to<>> m_uniformCache;

    f32 m_time = 0.0f;
    f32 m_revealElapsed = 0.0f;
};

} // namespace biofuel::game::presentation::effects
