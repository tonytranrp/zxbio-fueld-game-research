#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace render::diligent {

// Which Diligent backend to initialize. Vulkan is the project's primary target
// (PROJECT_BRIEF.md §2.1); D3D12 exists to keep the code honestly cross-backend from day one
// (PHASE_1_COMPLETION_BRIEF.md task 16) -- a backend-specific bug caught now is far cheaper than
// one caught after months of Vulkan-only development.
enum class Backend {
    Vulkan,
    D3D12,
};

[[nodiscard]] const char* to_string(Backend backend) noexcept;

struct RenderContextCreateInfo {
    Backend backend = Backend::Vulkan;
    void* native_window_handle = nullptr; // Win32 HWND from the windowing layer
    bool enable_validation = false;       // Diligent validation layer (debug naming flows through it too)
};

// Owns the Diligent device, immediate context, and swap chain (PHASE_1_BRIEF.md §2.1–§2.3).
// PIMPL is the module's compile firewall: no DiligentCore header leaks past this boundary, so
// app/ and world/ stay GPU-header-free (PROJECT_BRIEF.md §3) -- the same pattern
// world/generation uses to contain FastNoise2.
class RenderContext {
public:
    // Throws std::runtime_error when the backend cannot be initialized (no compatible GPU/driver,
    // D3D12 runtime missing, swap-chain creation failure) -- the caller decides whether another
    // backend is worth attempting; nothing here is recoverable in-place.
    explicit RenderContext(const RenderContextCreateInfo& info);
    ~RenderContext();

    RenderContext(RenderContext&&) noexcept;
    RenderContext& operator=(RenderContext&&) noexcept;
    RenderContext(const RenderContext&) = delete;
    RenderContext& operator=(const RenderContext&) = delete;

    void resize(std::uint32_t width, std::uint32_t height);
    void present();

    [[nodiscard]] Backend backend() const noexcept;
    [[nodiscard]] std::uint32_t width() const noexcept;
    [[nodiscard]] std::uint32_t height() const noexcept;

    // The enumerated adapter's human-readable name -- the M1.4 "device enumeration actually works
    // at runtime, not just builds" proof (PHASE_1_BRIEF.md §0), logged at startup and available
    // for the Group E overlay.
    [[nodiscard]] std::string adapter_description() const;

    // Module-internal accessor: Impl is complete only inside render/diligent's own translation
    // units (detail/render_context_impl.hpp); other modules see an opaque forward declaration and
    // cannot reach Diligent types through it.
    struct Impl;
    [[nodiscard]] Impl& impl() noexcept { return *impl_; }

private:
    std::unique_ptr<Impl> impl_;
};

} // namespace render::diligent
