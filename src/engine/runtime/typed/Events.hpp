#pragma once

#include "engine/events/EventManager.hpp"
#include "engine/runtime/typed/EventTags.hpp"
#include <concepts>
#include <string_view>
#include <type_traits>
#include <utility>

namespace biofuel::engine::runtime::typed {

template<typename TEvent>
concept RegisteredEvent = requires {
    typename EventSpec<TEvent>::Payload;
    { EventSpec<TEvent>::Name } -> std::convertible_to<std::string_view>;
};

template<typename TEvent>
struct EventChannel {
    static_assert(RegisteredEvent<TEvent>, "Every typed event needs EventSpec<TEvent>.");
    using Event = TEvent;
    using Payload = typename EventSpec<TEvent>::Payload;
    static constexpr std::string_view Name = EventSpec<TEvent>::Name;

    static void publish(const Payload& payload) {
        ::biofuel::engine::events::EventManager::instance().dispatcher().trigger<Payload>(payload);
    }

    static void publish(Payload&& payload = {}) {
        ::biofuel::engine::events::EventManager::instance().dispatcher().trigger<Payload>(std::move(payload));
    }

    [[nodiscard]] static auto sink() {
        return ::biofuel::engine::events::EventManager::instance().dispatcher().sink<Payload>();
    }
};

class Events {
public:
    template<typename TEvent>
    using Payload = typename EventChannel<TEvent>::Payload;

    static void init() {
        ::biofuel::engine::events::EventManager::instance().init();
    }

    static void shutdown() {
        ::biofuel::engine::events::EventManager::instance().shutdown();
    }

    static void clear() {
        ::biofuel::engine::events::EventManager::instance().clear();
    }

    template<typename TEvent>
    static void publish(const Payload<TEvent>& payload) {
        EventChannel<TEvent>::publish(payload);
    }

    template<typename TEvent>
    static void publish(Payload<TEvent>&& payload = {}) {
        EventChannel<TEvent>::publish(std::move(payload));
    }

    template<typename TEvent>
    [[nodiscard]] static auto sink() {
        return EventChannel<TEvent>::sink();
    }

    [[nodiscard]] static entt::dispatcher& bridgeDispatcher() {
        return ::biofuel::engine::events::EventManager::instance().dispatcher();
    }
};

template<typename TRegistry>
struct EventRegistryValidator;

template<typename... TEvents>
struct EventRegistryValidator<biofuel::typed::Registry<TEvents...>> {
    static consteval bool valid() {
        return (RegisteredEvent<TEvents> && ...);
    }
};

static_assert(EventRegistryValidator<AppEventRegistry>::valid());

} // namespace biofuel::engine::runtime::typed
