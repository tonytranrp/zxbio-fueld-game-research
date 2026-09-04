#include <stdexcept>
#include <string>

#include "glfw_window.hpp"

#define GLFW_INCLUDE_NONE // no GL headers -- Diligent owns the graphics API
#include <GLFW/glfw3.h>

// glfw3native.h pulls in windows.h for the HWND accessor -- the only TU in the app that does.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace app {

GlfwWindow::GlfwWindow(std::uint32_t width, std::uint32_t height, const char* title) {
    if (glfwInit() != GLFW_TRUE) {
        const char* description = nullptr;
        glfwGetError(&description);
        throw std::runtime_error(std::string("glfwInit failed: ") + (description ? description : "unknown"));
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), title, nullptr, nullptr);
    if (window_ == nullptr) {
        const char* description = nullptr;
        glfwGetError(&description);
        glfwTerminate();
        throw std::runtime_error(std::string("glfwCreateWindow failed: ") + (description ? description : "unknown"));
    }
}

GlfwWindow::~GlfwWindow() {
    glfwDestroyWindow(window_);
    glfwTerminate();
}

void GlfwWindow::poll_events() {
    glfwPollEvents();
}

bool GlfwWindow::should_close() const {
    return glfwWindowShouldClose(window_) == GLFW_TRUE;
}

std::pair<std::uint32_t, std::uint32_t> GlfwWindow::framebuffer_size() const {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    return {static_cast<std::uint32_t>(width < 0 ? 0 : width), static_cast<std::uint32_t>(height < 0 ? 0 : height)};
}

void* GlfwWindow::native_handle() const {
    return glfwGetWin32Window(window_);
}

} // namespace app
