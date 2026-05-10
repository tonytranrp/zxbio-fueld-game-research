#include "ComponentManager.hpp"
#include <algorithm>

namespace biofuel::utils::render::component {

void ComponentManager::add(std::unique_ptr<ComponentModule> component) {
    if (component) {
        m_components.push_back(std::move(component));
    }
}

bool ComponentManager::remove(const std::string_view name) noexcept {
    const auto it = std::remove_if(m_components.begin(), m_components.end(),
        [name](const auto& c) { return c && c->name() == name; });
    if (it == m_components.end()) {
        return false;
    }
    m_components.erase(it, m_components.end());
    return true;
}

ComponentModule* ComponentManager::get(const std::string_view name) noexcept {
    for (auto& c : m_components) {
        if (c && c->name() == name) {
            return c.get();
        }
    }
    return nullptr;
}

const ComponentModule* ComponentManager::get(const std::string_view name) const noexcept {
    for (const auto& c : m_components) {
        if (c && c->name() == name) {
            return c.get();
        }
    }
    return nullptr;
}

void ComponentManager::resetAll() noexcept {
    for (auto& c : m_components) {
        if (c) {
            c->reset();
        }
    }
}

void ComponentManager::updateAll(const f32 dt) noexcept {
    for (auto& c : m_components) {
        if (c) {
            c->update(dt);
        }
    }
}

void ComponentManager::applyAll(const Shader shader) const noexcept {
    for (const auto& c : m_components) {
        if (c) {
            c->apply(shader);
        }
    }
}

bool ComponentManager::anyActive() const noexcept {
    for (const auto& c : m_components) {
        if (c && c->isActive()) {
            return true;
        }
    }
    return false;
}

std::size_t ComponentManager::size() const noexcept {
    return m_components.size();
}

} // namespace biofuel::utils::render::component
