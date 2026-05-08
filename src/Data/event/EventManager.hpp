#pragma once

#include <entt/entt.hpp>
#include <memory>

// ------------------------------------------------------------------------------
// EventManager - Central event bus manager
// Owns the entt::dispatcher, provides init/shutdown lifecycle.
// ------------------------------------------------------------------------------
namespace biofuel {

class EventManager {
public:
    static EventManager& instance();

    void init();
    void shutdown();

    [[nodiscard]] entt::dispatcher& dispatcher();
    [[nodiscard]] const entt::dispatcher& dispatcher() const;

    // Convenience: clear all listeners
    void clear();

    // Non-copyable, non-movable
    EventManager(const EventManager&) = delete;
    EventManager& operator=(const EventManager&) = delete;
    EventManager(EventManager&&) = delete;
    EventManager& operator=(EventManager&&) = delete;

private:
    EventManager() = default;
    ~EventManager() = default;

    std::unique_ptr<entt::dispatcher> m_dispatcher;
};

} // namespace biofuel
