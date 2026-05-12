#include "EventManager.hpp"
#include <mutex>

namespace biofuel::engine::events {

EventManager& EventManager::instance() noexcept {
    static EventManager instance;
    return instance;
}

void EventManager::init() {
    ensureInitialized();
}

void EventManager::ensureInitialized() const {
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
    ensureInitialized();
    return *m_dispatcher;
}

const entt::dispatcher& EventManager::dispatcher() const {
    ensureInitialized();
    return *m_dispatcher;
}

void EventManager::clear() {
    std::lock_guard lock(m_initMutex);
    if (m_dispatcher) {
        m_dispatcher->clear();
    }
}

} // namespace biofuel::engine::events
