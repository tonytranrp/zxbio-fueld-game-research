#include "EventManager.hpp"

namespace biofuel {

EventManager& EventManager::instance() {
    static EventManager instance;
    return instance;
}

void EventManager::init() {
    if (!m_dispatcher) {
        m_dispatcher = std::make_unique<entt::dispatcher>();
    }
}

void EventManager::shutdown() {
    clear();
    m_dispatcher.reset();
}

entt::dispatcher& EventManager::dispatcher() {
    if (!m_dispatcher) {
        init();
    }
    return *m_dispatcher;
}

const entt::dispatcher& EventManager::dispatcher() const {
    if (!m_dispatcher) {
        m_dispatcher = std::make_unique<entt::dispatcher>();
    }
    return *m_dispatcher;
}

void EventManager::clear() {
    if (m_dispatcher) {
        m_dispatcher->clear();
    }
}

} // namespace biofuel
