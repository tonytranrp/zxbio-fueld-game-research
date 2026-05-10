#pragma once

#include "Core/Types.hpp"
#include <raylib.h>
#include <string_view>

namespace biofuel::utils::render::component {

// ==============================================================================
// ComponentModule — Base interface for shader-attached components
// ==============================================================================
//
// A component is a reusable piece of shader state (e.g. camera, fog, lighting)
// that:
//   1. Owns animation/interpolation state (C++ side)
//   2. Knows which GLSL uniforms it manages
//   3. Can apply its current state to any Raylib Shader
//
// ------------------------------------------------------------------------------
// DESIGN: VIRTUAL INTERFACE (UNLIKE SHADER MODULES)
// ------------------------------------------------------------------------------
//
// Shader modules (Utils/render/Shader/) are data-only, constexpr descriptors
// on the hot path — virtual dispatch is unacceptable there.
//
// Components, by contrast, are fewer in number and called once per frame to
// set a handful of uniforms. Virtual dispatch cost is negligible here, and
// the polymorphism it enables (ComponentManager can own any mix of components)
// justifies the tradeoff.
//
// ==============================================================================

class ComponentModule {
public:
    virtual ~ComponentModule() = default;

    // Human-readable name for debugging / identification.
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    // Reset component to its default/identity state.
    virtual void reset() noexcept = 0;

    // Advance any internal animation by dt seconds.
    virtual void update(f32 dt) noexcept = 0;

    // Write current uniform values into the given shader.
    // Called once per frame during the render phase.
    virtual void apply(Shader shader) const noexcept = 0;

    // True if the component has any active animation in progress.
    [[nodiscard]] virtual bool isActive() const noexcept = 0;
};

} // namespace biofuel::utils::render::component
