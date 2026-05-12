#pragma once

// ------------------------------------------------------------------------------
// Runtime Hook - typed access point for engine services.
// ------------------------------------------------------------------------------

#include "engine/runtime/typed/Assets.hpp"
#include "engine/runtime/typed/Services.hpp"
#include "game/gameplay/FutureSystems.hpp"

namespace biofuel::engine::runtime {

class Runtime {
public:
    [[nodiscard]] static ::biofuel::engine::runtime::typed::AppServices& services() {
        static ::biofuel::engine::runtime::typed::AppServices services{};
        return services;
    }

    template<typename TService>
    [[nodiscard]] static typename ::biofuel::engine::runtime::typed::ServiceModule<TService>::Backend& service() {
        return services().get<TService>();
    }

    [[nodiscard]] static auto& screen() { return service<::biofuel::engine::runtime::typed::ScreenService>(); }
    [[nodiscard]] static auto& shader() { return service<::biofuel::engine::runtime::typed::ShaderService>(); }
    [[nodiscard]] static auto& video() { return service<::biofuel::engine::runtime::typed::VideoService>(); }
    [[nodiscard]] static auto& audio() { return service<::biofuel::engine::runtime::typed::AudioService>(); }
    [[nodiscard]] static auto& model() { return service<::biofuel::engine::runtime::typed::ModelService>(); }
    [[nodiscard]] static auto& animation() { return service<::biofuel::engine::runtime::typed::AnimationService>(); }
    [[nodiscard]] static auto& events() { return service<::biofuel::engine::runtime::typed::EventService>(); }
    [[nodiscard]] static auto& render() { return service<::biofuel::engine::runtime::typed::RenderService>(); }
#ifdef BIOFUEL_ENABLE_HAND_TRACKING
    [[nodiscard]] static auto& handTracking() { return service<::biofuel::engine::runtime::typed::HandTrackingService>(); }
#endif
};

} // namespace biofuel::engine::runtime
