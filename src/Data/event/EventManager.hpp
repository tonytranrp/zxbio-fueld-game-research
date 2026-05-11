#pragma once

#include <entt/entt.hpp>
#include <memory>
#include <mutex>

// ------------------------------------------------------------------------------
// EventManager - Central event bus manager
// Owns the entt::dispatcher, provides init/shutdown lifecycle.
// Thread-safe lazy initialization via mutex guard (B004).
// ------------------------------------------------------------------------------
namespace biofuel {

class EventManager {
public:
    [[nodiscard]] static EventManager& instance() noexcept;

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
    mutable std::mutex m_initMutex;
};

} // namespace biofuel
