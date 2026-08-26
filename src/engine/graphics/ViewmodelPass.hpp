#pragma once

#include "engine/core/Types.hpp"
#include "engine/graphics/Render.hpp"
#include "engine/graphics/RenderSurface.hpp"
#include "engine/graphics/Scene3D.hpp"
#include <optional>
#include <raylib.h>

namespace biofuel::engine::graphics {

// -----------------------------------------------------------------------------
// ViewmodelPass - renders a viewmodel (first-person hands, later a held item)
// into its own offscreen RenderSurface with an independent depth buffer, then
// composites the result over whatever is already on screen.
//
// This is what actually makes the depth isolation work: the viewmodel pass
// never shares a depth buffer with the world pass, so the viewmodel can never
// be clipped by, or clip into, world geometry no matter how close the
// player's collision capsule gets to a wall -- drawing it last in the same
// pass would still depth-test against the world and wouldn't have this
// property. ScopedClipPlanes compresses the near/far range around the
// viewmodel's own small authored scale for reasonable depth precision.
//
// Owns no raw Load*/Unload* calls itself (delegates to RenderSurface, already
// on RuntimeSafetyGuard's allow-list), so this header needs no allow-list
// entry of its own.
// -----------------------------------------------------------------------------
class ViewmodelPass final {
public:
    void ensureSized(const i32 screenWidth, const i32 screenHeight) {
        m_surface.ensureSize(screenWidth, screenHeight);
    }

    // Begins capturing into the offscreen surface. Must be paired with a
    // matching endAndComposite() call later the same frame.
    void begin(const Camera3D viewmodelCamera) noexcept {
        if (!m_surface.valid()) {
            return;
        }
        BeginTextureMode(m_surface.target());
        ClearBackground(BLANK);
        m_clipPlanes.emplace(kNearPlane, kFarPlane);
        m_mode3D.emplace(viewmodelCamera);
    }

    // Ends the 3D scope, ends the offscreen capture, and blits the result
    // over the current framebuffer with alpha blending (BLANK-cleared pixels
    // the viewmodel never drew to stay fully transparent).
    void endAndComposite() noexcept {
        if (!m_surface.valid()) {
            return;
        }
        m_mode3D.reset();
        m_clipPlanes.reset();
        EndTextureMode();
        Renderer::drawRenderTexture(m_surface.texture(), 0, 0);
    }

private:
    // Compressed relative to the world's own near/far so a ~0.5m-scale hand
    // model gets reasonable depth precision regardless of world scale.
    static constexpr double kNearPlane = 0.01;
    static constexpr double kFarPlane = 10.0;

    RenderSurface m_surface;
    std::optional<ScopedClipPlanes> m_clipPlanes;
    std::optional<ScopedMode3D> m_mode3D;
};

} // namespace biofuel::engine::graphics
