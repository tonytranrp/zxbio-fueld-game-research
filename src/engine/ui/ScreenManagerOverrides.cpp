#include "ScreenManager.hpp"
#include "engine/runtime/Runtime.hpp"

// ------------------------------------------------------------------------------
// ScreenManager — per-screen override state
//
// Split out of ScreenManager.cpp. These members own the transient/persistent
// transition, layer, and debug-render overrides published through the event bus.
// ------------------------------------------------------------------------------

namespace biofuel::engine::ui {

bool ScreenManager::isLayerEnabled(
    const typed::ScreenId screenId,
    const std::string_view layerName) const noexcept
{
    const auto& overrides = overridesFor(screenId);
    const auto layer = overrides.layers.find(layerName);
    if (layer == overrides.layers.end()) {
        return true;
    }
    return layer->second.enabled;
}

ScreenManager::ScreenOverrideState& ScreenManager::overridesFor(const typed::ScreenId screenId) noexcept {
    const auto index = typed::screenIdIndex(screenId);
    if (index >= m_overrides.size()) {
        return m_overrides[typed::screenIdIndex(typed::ScreenId::Unknown)];
    }
    return m_overrides[index];
}

const ScreenManager::ScreenOverrideState& ScreenManager::overridesFor(const typed::ScreenId screenId) const noexcept {
    const auto index = typed::screenIdIndex(screenId);
    if (index >= m_overrides.size()) {
        return m_overrides[typed::screenIdIndex(typed::ScreenId::Unknown)];
    }
    return m_overrides[index];
}

void ScreenManager::clearTransientOverrides(const typed::ScreenId screenId) {
    auto& overrides = overridesFor(screenId);
    if (overrides.transition.active && !overrides.transition.persistent) {
        overrides.transition = {};
    }
    if (overrides.debug.active && !overrides.debug.persistent) {
        overrides.debug = {};
    }

    for (auto it = overrides.layers.begin(); it != overrides.layers.end(); ) {
        if (!it->second.persistent) {
            it = overrides.layers.erase(it);
        } else {
            ++it;
        }
    }
}

void ScreenManager::connectOverrideSinks() {
    if (m_overrideSinksConnected) {
        return;
    }

    ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::screen::TransitionOverride>().connect<&ScreenManager::onTransitionOverride>(*this);
    ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::screen::LayerOverride>().connect<&ScreenManager::onLayerOverride>(*this);
    ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::screen::DebugRenderOverride>().connect<&ScreenManager::onDebugOverride>(*this);
    m_overrideSinksConnected = true;
}

void ScreenManager::disconnectOverrideSinks() {
    if (!m_overrideSinksConnected) {
        return;
    }

    ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::screen::TransitionOverride>().disconnect<&ScreenManager::onTransitionOverride>(*this);
    ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::screen::LayerOverride>().disconnect<&ScreenManager::onLayerOverride>(*this);
    ::biofuel::engine::runtime::typed::Events::sink<::biofuel::engine::runtime::typed::screen::DebugRenderOverride>().disconnect<&ScreenManager::onDebugOverride>(*this);
    m_overrideSinksConnected = false;
}

void ScreenManager::onTransitionOverride(const ::biofuel::engine::events::screen::ScreenTransitionOverrideEvent& event) {
    auto& overrides = overridesFor(event.screenId);
    if (!event.enabled) {
        overrides.transition = {};
        return;
    }

    overrides.transition = TransitionOverrideState{
        .active = true,
        .persistent = event.persistent,
        .policy = event.policy,
    };
}

void ScreenManager::onLayerOverride(const ::biofuel::engine::events::screen::ScreenLayerOverrideEvent& event) {
    if (event.layerName.empty()) {
        return;
    }

    auto& overrides = overridesFor(event.screenId);
    overrides.layers[event.layerName] = LayerOverrideState{
        .enabled = event.enabled,
        .persistent = event.persistent,
    };
}

void ScreenManager::onDebugOverride(const ::biofuel::engine::events::screen::ScreenDebugRenderOverrideEvent& event) {
    auto& overrides = overridesFor(event.screenId);
    overrides.debug = DebugOverrideState{
        .active = true,
        .enabled = event.enabled,
        .persistent = event.persistent,
    };
}

} // namespace biofuel::engine::ui
