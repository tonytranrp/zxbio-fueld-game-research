#pragma once

#include "engine/input/input_state.hpp"

struct GLFWwindow; // GLFW stays out of this header; src/glfw_input.cpp is the only GLFW-touching TU

namespace engine::input {

// Registers GLFW key/mouse-button/cursor callbacks on an existing window (owned elsewhere) and
// folds them into an InputState (Phase 1 brief §7's engine/input). Holding the right mouse
// button captures the cursor (GLFW_CURSOR_DISABLED) for mouse-look and releases it on button-up
// -- editor-style, so the window never traps the cursor permanently.
//
// Uses the window's GLFW user pointer to reach the instance from C callbacks -- so at most one
// GlfwInput may be attached to a window, and nothing else may claim that user pointer.
class GlfwInput {
public:
    explicit GlfwInput(GLFWwindow* window);
    ~GlfwInput(); // detaches the callbacks and clears the user pointer

    GlfwInput(const GlfwInput&) = delete;
    GlfwInput& operator=(const GlfwInput&) = delete;
    GlfwInput(GlfwInput&&) = delete; // the user pointer holds `this` -- moving would dangle it
    GlfwInput& operator=(GlfwInput&&) = delete;

    [[nodiscard]] const InputState& state() const noexcept { return state_; }
    [[nodiscard]] glm::vec2 take_look_delta() noexcept { return state_.take_look_delta(); }
    [[nodiscard]] bool take_walk_toggle() noexcept { return state_.take_walk_toggle(); }
    [[nodiscard]] bool take_screenshot() noexcept { return state_.take_screenshot(); }

private:
    static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void cursor_pos_callback(GLFWwindow* window, double x, double y);

    GLFWwindow* window_ = nullptr;
    InputState state_;
    double last_cursor_x_ = 0.0;
    double last_cursor_y_ = 0.0;
    bool have_last_cursor_ = false;
};

} // namespace engine::input
