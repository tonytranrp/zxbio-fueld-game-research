#pragma once

#include "Animation.hpp"
#include <memory>
#include <raylib.h>

namespace biofuel::animation::PremadeAnimations {

// ------------------------------------------------------------------------------
// makeFadeIn / makeFadeOut
// Animates alpha from 0→255 or 255→0 using Color lerp.
// ------------------------------------------------------------------------------
[[nodiscard]] inline std::unique_ptr<Animation<Color>> makeFadeIn(
    f32 duration,
    Easing::Fn easing = Easing::easeOutQuad,
    entt::dispatcher* dispatcher = nullptr
) {
    return std::make_unique<Animation<Color>>(
        "fade_in",
        Color{0, 0, 0, 0},
        Color{0, 0, 0, 255},
        duration,
        easing,
        dispatcher
    );
}

[[nodiscard]] inline std::unique_ptr<Animation<Color>> makeFadeOut(
    f32 duration,
    Easing::Fn easing = Easing::easeInQuad,
    entt::dispatcher* dispatcher = nullptr
) {
    return std::make_unique<Animation<Color>>(
        "fade_out",
        Color{0, 0, 0, 255},
        Color{0, 0, 0, 0},
        duration,
        easing,
        dispatcher
    );
}

// ------------------------------------------------------------------------------
// makeScaleIn / makeScaleOut
// Animates scale from startScale→1.0 or 1.0→endScale.
// pivot is the center point for scaling (e.g., screen center).
// ------------------------------------------------------------------------------
[[nodiscard]] inline std::unique_ptr<Animation<f32>> makeScaleIn(
    f32 duration,
    f32 startScale = 0.0f,
    Easing::Fn easing = Easing::easeOutBack,
    entt::dispatcher* dispatcher = nullptr
) {
    return std::make_unique<Animation<f32>>(
        "scale_in",
        startScale,
        1.0f,
        duration,
        easing,
        dispatcher
    );
}

[[nodiscard]] inline std::unique_ptr<Animation<f32>> makeScaleOut(
    f32 duration,
    f32 endScale = 0.0f,
    Easing::Fn easing = Easing::easeInBack,
    entt::dispatcher* dispatcher = nullptr
) {
    return std::make_unique<Animation<f32>>(
        "scale_out",
        1.0f,
        endScale,
        duration,
        easing,
        dispatcher
    );
}

// ------------------------------------------------------------------------------
// makeSlideUp / makeSlideDown
// Animates Y offset from offsetY→0 (slide up) or 0→offsetY (slide down).
// Positive offsetY = slide down from above. Negative = slide up from below.
// ------------------------------------------------------------------------------
[[nodiscard]] inline std::unique_ptr<Animation<f32>> makeSlideUp(
    f32 offsetY,
    f32 duration,
    Easing::Fn easing = Easing::easeOutCubic,
    entt::dispatcher* dispatcher = nullptr
) {
    return std::make_unique<Animation<f32>>(
        "slide_up",
        offsetY,
        0.0f,
        duration,
        easing,
        dispatcher
    );
}

[[nodiscard]] inline std::unique_ptr<Animation<f32>> makeSlideDown(
    f32 offsetY,
    f32 duration,
    Easing::Fn easing = Easing::easeInCubic,
    entt::dispatcher* dispatcher = nullptr
) {
    return std::make_unique<Animation<f32>>(
        "slide_down",
        0.0f,
        offsetY,
        duration,
        easing,
        dispatcher
    );
}

// ------------------------------------------------------------------------------
// makePulse
// Oscillates scale between 1.0 and maxScale for a given duration.
// Uses easeInOutSine for smooth in/out oscillation.
// ------------------------------------------------------------------------------
[[nodiscard]] inline std::unique_ptr<Animation<f32>> makePulse(
    f32 maxScale,
    f32 duration,
    Easing::Fn easing = Easing::easeInOutSine,
    entt::dispatcher* dispatcher = nullptr
) {
    return std::make_unique<Animation<f32>>(
        "pulse",
        1.0f,
        maxScale,
        duration,
        easing,
        dispatcher
    );
}

// ------------------------------------------------------------------------------
// makeShake
// Oscillates X position ±intensity for a given duration.
// Good for error feedback or impact effects.
// ------------------------------------------------------------------------------
[[nodiscard]] inline std::unique_ptr<Animation<f32>> makeShake(
    f32 intensity,
    f32 duration,
    Easing::Fn easing = Easing::easeOutQuad,
    entt::dispatcher* dispatcher = nullptr
) {
    return std::make_unique<Animation<f32>>(
        "shake",
        -intensity,
        intensity,
        duration,
        easing,
        dispatcher
    );
}

// ------------------------------------------------------------------------------
// makeColorShift
// Interpolates a Color from start to end over duration.
// Useful for button hover effects, title pulses, etc.
// ------------------------------------------------------------------------------
[[nodiscard]] inline std::unique_ptr<Animation<Color>> makeColorShift(
    Color start,
    Color end,
    f32 duration,
    Easing::Fn easing = Easing::easeInOutQuad,
    entt::dispatcher* dispatcher = nullptr
) {
    return std::make_unique<Animation<Color>>(
        "color_shift",
        start,
        end,
        duration,
        easing,
        dispatcher
    );
}

// ------------------------------------------------------------------------------
// makePositionLerp
// Interpolates a Vector2 from start to end.
// Useful for smooth camera movement or UI element sliding.
// ------------------------------------------------------------------------------
[[nodiscard]] inline std::unique_ptr<Animation<Vector2>> makePositionLerp(
    Vector2 start,
    Vector2 end,
    f32 duration,
    Easing::Fn easing = Easing::easeInOutQuad,
    entt::dispatcher* dispatcher = nullptr
) {
    return std::make_unique<Animation<Vector2>>(
        "position_lerp",
        start,
        end,
        duration,
        easing,
        dispatcher
    );
}

// ------------------------------------------------------------------------------
// makeRectLerp
// Interpolates a Rectangle (position + size) from start to end.
// Useful for panel resize animations.
// ------------------------------------------------------------------------------
[[nodiscard]] inline std::unique_ptr<Animation<Rectangle>> makeRectLerp(
    Rectangle start,
    Rectangle end,
    f32 duration,
    Easing::Fn easing = Easing::easeInOutQuad,
    entt::dispatcher* dispatcher = nullptr
) {
    return std::make_unique<Animation<Rectangle>>(
        "rect_lerp",
        start,
        end,
        duration,
        easing,
        dispatcher
    );
}

// ------------------------------------------------------------------------------
// makeFloatLerp
// Generic float interpolation for any numeric value.
// ------------------------------------------------------------------------------
[[nodiscard]] inline std::unique_ptr<Animation<f32>> makeFloatLerp(
    std::string name,
    f32 start,
    f32 end,
    f32 duration,
    Easing::Fn easing = Easing::easeInOutQuad,
    entt::dispatcher* dispatcher = nullptr
) {
    return std::make_unique<Animation<f32>>(
        std::move(name),
        start,
        end,
        duration,
        easing,
        dispatcher
    );
}

} // namespace biofuel::animation::PremadeAnimations