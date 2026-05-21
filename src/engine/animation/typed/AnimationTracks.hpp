#pragma once

#include "engine/animation/Easing.hpp"
#include "engine/core/Types.hpp"
#include "engine/core/typed/Meta.hpp"
#include <string_view>

namespace biofuel::engine::animation::typed {

template<typename TTag>
struct EasingPolicy {
    static constexpr auto Function = Easing::linear;
};

template<typename TValue, typename TTag>
struct AnimationTrack {
    using Value = TValue;
    using Tag = TTag;
    static constexpr std::string_view Name = TTag::Name;
    static constexpr f32 Duration = TTag::Duration;
    static constexpr auto Easing = EasingPolicy<TTag>::Function;
};

namespace track {
struct PanelSlide {
    static constexpr std::string_view Name = "screen.panel_slide";
    static constexpr f32 Duration = 0.3f;
};

struct ScreenBackdropReveal {
    static constexpr std::string_view Name = "screen.backdrop_reveal";
    static constexpr f32 Duration = 1.35f;
};

struct ScreenDimensionShift {
    static constexpr std::string_view Name = "screen.dimension_shift";
    static constexpr f32 Duration = 3.0f;
};

struct ScreenCrossfade {
    static constexpr std::string_view Name = "screen.crossfade";
    static constexpr f32 Duration = 0.5f;
};
} // namespace track

template<>
struct EasingPolicy<track::PanelSlide> {
    static constexpr auto Function = Easing::easeOutCubic;
};

template<>
struct EasingPolicy<track::ScreenBackdropReveal> {
    static constexpr auto Function = Easing::easeOutCubic;
};

template<>
struct EasingPolicy<track::ScreenDimensionShift> {
    static constexpr auto Function = Easing::easeInOutCubic;
};

template<>
struct EasingPolicy<track::ScreenCrossfade> {
    static constexpr auto Function = Easing::easeOutCubic;
};

using ScreenAnimationRegistry = biofuel::typed::Registry<
    track::PanelSlide,
    track::ScreenBackdropReveal,
    track::ScreenDimensionShift,
    track::ScreenCrossfade>;

static_assert(ScreenAnimationRegistry::valid());

} // namespace biofuel::engine::animation::typed
