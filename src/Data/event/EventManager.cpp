#include "EventManager.hpp"
#include <mutex>

namespace biofuel {

EventManager& EventManager::instance() noexcept {
    static EventManager instance;
    return instance;
}

void EventManager::init() {
    std::lock_guard lock(m_initMutex);
    if (!m_dispatcher) {
        m_dispatcher = std::make_unique<entt::dispatcher>();
    }
}

void EventManager::shutdown() {
    std::lock_guard lock(m_initMutex);
    if (m_dispatcher) {
        m_dispatcher->clear();
    }
    m_dispatcher.reset();
}

entt::dispatcher& EventManager::dispatcher() {
    init();  // thread-safe: guarded by mutex
    return *m_dispatcher;
}

const entt::dispatcher& EventManager::dispatcher() const {
    // const_cast is acceptable: init() is idempotent and thread-safe
    // via mutex. The alternative (mutable unique_ptr without sync)
    // was a data race (B004).
    const_cast<EventManager*>(this)->init();
    return *m_dispatcher;
}

void EventManager::clear() {
    std::lock_guard lock(m_initMutex);
    if (m_dispatcher) {
        m_dispatcher->clear();
    }
}

} // namespace biofuel
