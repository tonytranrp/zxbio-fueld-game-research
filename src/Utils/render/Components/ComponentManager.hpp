#pragma once

#include "Utils/render/Components/ComponentModule.hpp"
#include <memory>
#include <string_view>
#include <vector>

namespace biofuel::utils::render::component {

// ==============================================================================
// ComponentManager — Owns and orchestrates shader components
// ==============================================================================
//
// Manages a collection of ComponentModule instances. Provides bulk operations
// to update, apply, and reset all components in a single call.
//
// Usage:
//   ComponentManager components;
//   components.add(std::make_unique<CameraComponent>());
//
//   // Per frame:
//   components.updateAll(dt);
//   components.applyAll(shader);
//
// Integration with ShaderManager:
//   Components apply their uniforms to a Raylib Shader obtained from
//   ShaderManager. The manager does NOT own or cache shaders — it only
//   writes uniform values into whatever Shader handle is passed to applyAll().
//
// ==============================================================================

class ComponentManager {
public:
    // Add a component. Ownership is transferred to the manager.
    void add(std::unique_ptr<ComponentModule> component);

    // Remove a component by name. Returns true if found and removed.
    bool remove(std::string_view name) noexcept;

    // Get a component by name. Returns nullptr if not found.
    [[nodiscard]] ComponentModule* get(std::string_view name) noexcept;
    [[nodiscard]] const ComponentModule* get(std::string_view name) const noexcept;

    // Get a component by name, cast to a specific type.
    template<typename T>
    [[nodiscard]] T* getAs(std::string_view name) noexcept {
        return dynamic_cast<T*>(get(name));
    }

    template<typename T>
    [[nodiscard]] const T* getAs(std::string_view name) const noexcept {
        return dynamic_cast<const T*>(get(name));
    }

    // Bulk operations — apply to all managed components.
    void resetAll() noexcept;
    void updateAll(f32 dt) noexcept;
    void applyAll(Shader shader) const noexcept;

    // True if any component has an active animation.
    [[nodiscard]] bool anyActive() const noexcept;

    // Number of managed components.
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::vector<std::unique_ptr<ComponentModule>> m_components;
};

} // namespace biofuel::utils::render::component
