#pragma once

#include "Core/Types.hpp"
#include <cmath>

namespace biofuel::animation::Easing {

// ------------------------------------------------------------------------------
// Easing Functions
// All functions take a normalized progress t ∈ [0, 1] and return the eased value.
// ------------------------------------------------------------------------------

// ---- Linear ----
[[nodiscard]] inline f32 linear(f32 t) noexcept {
    return t;
}

// ---- Quadratic ----
[[nodiscard]] inline f32 easeInQuad(f32 t) noexcept {
    return t * t;
}
[[nodiscard]] inline f32 easeOutQuad(f32 t) noexcept {
    return t * (2.0f - t);
}
[[nodiscard]] inline f32 easeInOutQuad(f32 t) noexcept {
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

// ---- Cubic ----
[[nodiscard]] inline f32 easeInCubic(f32 t) noexcept {
    return t * t * t;
}
[[nodiscard]] inline f32 easeOutCubic(f32 t) noexcept {
    const f32 u = t - 1.0f;
    return u * u * u + 1.0f;
}
[[nodiscard]] inline f32 easeInOutCubic(f32 t) noexcept {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * 0.5f;
}

// ---- Quartic ----
[[nodiscard]] inline f32 easeInQuart(f32 t) noexcept {
    return t * t * t * t;
}
[[nodiscard]] inline f32 easeOutQuart(f32 t) noexcept {
    const f32 u = t - 1.0f;
    return 1.0f - u * u * u * u;
}
[[nodiscard]] inline f32 easeInOutQuart(f32 t) noexcept {
    return t < 0.5f ? 8.0f * t * t * t * t : 1.0f - 8.0f * (t - 1.0f) * (t - 1.0f) * (t - 1.0f) * (t - 1.0f);
}

// ---- Quintic ----
[[nodiscard]] inline f32 easeInQuint(f32 t) noexcept {
    return t * t * t * t * t;
}
[[nodiscard]] inline f32 easeOutQuint(f32 t) noexcept {
    const f32 u = t - 1.0f;
    return u * u * u * u * u + 1.0f;
}
[[nodiscard]] inline f32 easeInOutQuint(f32 t) noexcept {
    return t < 0.5f ? 16.0f * t * t * t * t * t : 1.0f + 16.0f * (t - 1.0f) * (t - 1.0f) * (t - 1.0f) * (t - 1.0f) * (t - 1.0f);
}

// ---- Sine ----
[[nodiscard]] inline f32 easeInSine(f32 t) noexcept {
    return 1.0f - std::cos(t * (3.14159265f / 2.0f));
}
[[nodiscard]] inline f32 easeOutSine(f32 t) noexcept {
    return std::sin(t * (3.14159265f / 2.0f));
}
[[nodiscard]] inline f32 easeInOutSine(f32 t) noexcept {
    return -0.5f * (std::cos(3.14159265f * t) - 1.0f);
}

// ---- Exponential ----
[[nodiscard]] inline f32 easeInExpo(f32 t) noexcept {
    return t == 0.0f ? 0.0f : std::pow(2.0f, 10.0f * (t - 1.0f));
}
[[nodiscard]] inline f32 easeOutExpo(f32 t) noexcept {
    return t == 1.0f ? 1.0f : 1.0f - std::pow(-2.0f, -10.0f * t);
}
[[nodiscard]] inline f32 easeInOutExpo(f32 t) noexcept {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    return t < 0.5f
        ? 0.5f * std::pow(2.0f, 20.0f * t - 10.0f)
        : 1.0f - 0.5f * std::pow(2.0f, -20.0f * t + 10.0f);
}

// ---- Circular ----
[[nodiscard]] inline f32 easeInCirc(f32 t) noexcept {
    return 1.0f - std::sqrt(1.0f - t * t);
}
[[nodiscard]] inline f32 easeOutCirc(f32 t) noexcept {
    const f32 u = t - 1.0f;
    return std::sqrt(1.0f - u * u);
}
[[nodiscard]] inline f32 easeInOutCirc(f32 t) noexcept {
    return t < 0.5f
        ? 0.5f * (1.0f - std::sqrt(1.0f - 4.0f * t * t))
        : 0.5f * (std::sqrt(-(2.0f * t - 1.0f) * (2.0f * t - 3.0f)) + 1.0f);
}

// ---- Elastic ----
[[nodiscard]] inline f32 easeInElastic(f32 t) noexcept {
    if (t == 0.0f || t == 1.0f) return t;
    const f32 p = 0.3f;
    const f32 s = p / 4.0f;
    const f32 u = t - 1.0f;
    return -std::pow(2.0f, 10.0f * u) * std::sin((u - s) * (3.14159265f * 2.0f) / p);
}
[[nodiscard]] inline f32 easeOutElastic(f32 t) noexcept {
    if (t == 0.0f || t == 1.0f) return t;
    const f32 p = 0.3f;
    const f32 s = p / 4.0f;
    return std::pow(2.0f, -10.0f * t) * std::sin((t - s) * (3.14159265f * 2.0f) / p) + 1.0f;
}
[[nodiscard]] inline f32 easeInOutElastic(f32 t) noexcept {
    if (t == 0.0f || t == 1.0f) return t;
    const f32 p = 0.45f;
    const f32 s = p / 4.0f;
    if (t < 0.5f) {
        const f32 u = 2.0f * t - 1.0f;
        return -0.5f * std::pow(2.0f, 10.0f * u) * std::sin((u - s) * (3.14159265f * 2.0f) / p);
    }
    const f32 u = 2.0f * t - 1.0f;
    return 0.5f * std::pow(2.0f, -10.0f * u) * std::sin((u - s) * (3.14159265f * 2.0f) / p) + 1.0f;
}

// ---- Back (overshoot) ----
[[nodiscard]] inline f32 easeInBack(f32 t) noexcept {
    constexpr f32 c1 = 1.70158f;
    constexpr f32 c3 = c1 + 1.0f;
    return c3 * t * t * t - c1 * t * t;
}
[[nodiscard]] inline f32 easeOutBack(f32 t) noexcept {
    constexpr f32 c1 = 1.70158f;
    constexpr f32 c3 = c1 + 1.0f;
    const f32 u = t - 1.0f;
    return 1.0f + c3 * u * u * u + c1 * u * u;
}
[[nodiscard]] inline f32 easeInOutBack(f32 t) noexcept {
    constexpr f32 c1 = 1.70158f;
    constexpr f32 c2 = c1 * 1.525f;
    if (t < 0.5f) {
        const f32 u = 2.0f * t;
        return (u * u * ((c2 + 1.0f) * u - c2)) * 0.5f;
    }
    const f32 u = 2.0f * t - 2.0f;
    return (u * u * ((c2 + 1.0f) * u + c2) + 2.0f) * 0.5f;
}

// ---- Bounce ----
[[nodiscard]] inline f32 easeOutBounce(f32 t) noexcept {
    constexpr f32 n1 = 7.5625f;
    constexpr f32 d1 = 2.75f;
    if (t < 1.0f / d1) {
        return n1 * t * t;
    }
    if (t < 2.0f / d1) {
        const f32 u = t - 1.5f / d1;
        return n1 * u * u + 0.75f;
    }
    if (t < 2.5f / d1) {
        const f32 u = t - 2.25f / d1;
        return n1 * u * u + 0.9375f;
    }
    const f32 u = t - 2.625f / d1;
    return n1 * u * u + 0.984375f;
}
[[nodiscard]] inline f32 easeInBounce(f32 t) noexcept {
    return 1.0f - easeOutBounce(1.0f - t);
}
[[nodiscard]] inline f32 easeInOutBounce(f32 t) noexcept {
    return t < 0.5f
        ? 0.5f * (1.0f - easeOutBounce(1.0f - 2.0f * t))
        : 0.5f * easeOutBounce(2.0f * t - 1.0f) + 0.5f;
}

// ---- Type alias for easing function signature ----
using Fn = f32(*)(f32);

} // namespace biofuel::animation::Easing