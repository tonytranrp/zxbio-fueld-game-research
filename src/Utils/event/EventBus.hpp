#pragma once

#include <entt/entt.hpp>
#include <utility>

namespace biofuel::utils::event {

// ------------------------------------------------------------------------------
// EventBus - entt event system wrapper
// Simplified API for emitting and listening to game events.
// ------------------------------------------------------------------------------
class EventBus {
public:
    template<typename Event, auto Handler>
    void connect();

    template<typename Event, typename Listener>
    void connect(Listener& listener);

    template<typename Event>
    void emit(const Event& event);

    template<typename Event, typename... Args>
    void emit(Args&&... args);

    void clear();

private:
    entt::dispatcher m_dispatcher;
};

// ------------------------------------------------------------------------------
// Inline implementations (header-only since entt is header-only)
// ------------------------------------------------------------------------------

template<typename Event, auto Handler>
void EventBus::connect() {
    m_dispatcher.sink<Event>().template connect<Handler>();
}

template<typename Event, typename Listener>
void EventBus::connect(Listener& listener) {
    m_dispatcher.sink<Event>().connect(listener);
}

template<typename Event>
void EventBus::emit(const Event& event) {
    m_dispatcher.trigger(event);
}

template<typename Event, typename... Args>
void EventBus::emit(Args&&... args) {
    m_dispatcher.trigger(Event{std::forward<Args>(args)...});
}

inline void EventBus::clear() {
    m_dispatcher.clear();
}

} // namespace biofuel::utils::event
