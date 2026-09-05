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
    GlfwWindow(std::uint32_t width, std::uint32_t height, const char* title);
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
