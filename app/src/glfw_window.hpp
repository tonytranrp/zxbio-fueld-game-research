#pragma once

#include <cstdint>
#include <utility>

struct GLFWwindow; // avoid pulling GLFW (and windows.h) into every app TU

namespace app {

// App-local GLFW glue (task 10): owns the one window and the GLFW library lifetime. Created with
// GLFW_NO_API -- Diligent drives Vulkan/D3D12 itself; GLFW is windowing + (in M1.5) input only.
// Not an engine module on purpose: Phase 1 brief §7's tree adds engine/input for callbacks
// later, but window ownership is application glue, and render/diligent deliberately consumes only
// the opaque native handle so it never links GLFW.
class GlfwWindow {
public:
    // Throws std::runtime_error if GLFW init or window creation fails.
    // `visible` false creates the window hidden (GLFW_VISIBLE). That is what voxel_harness
    // --headless means here: the same swap chain and the same back-buffer readback --verify-frame
    // already uses, with nothing on screen. It is NOT a surfaceless presentation path, so a driver
    // that behaves differently without a visible surface would not be caught by it -- stated
    // rather than implied, because "headless" usually promises more than this.
    GlfwWindow(std::uint32_t width, std::uint32_t height, const char* title, bool visible = true);
    ~GlfwWindow();

    GlfwWindow(const GlfwWindow&) = delete;
    GlfwWindow& operator=(const GlfwWindow&) = delete;
    GlfwWindow(GlfwWindow&&) = delete;
    GlfwWindow& operator=(GlfwWindow&&) = delete;

    void poll_events();
    [[nodiscard]] bool should_close() const;
    [[nodiscard]] std::pair<std::uint32_t, std::uint32_t> framebuffer_size() const;
    [[nodiscard]] void* native_handle() const;                      // Win32 HWND, for RenderContextCreateInfo
    [[nodiscard]] GLFWwindow* handle() noexcept { return window_; } // for engine/input's callbacks (M1.5)

private:
    GLFWwindow* window_ = nullptr;
};

} // namespace app
