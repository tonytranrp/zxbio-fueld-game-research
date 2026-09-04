#include "engine/input/glfw_input.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace engine::input {

namespace {

GlfwInput* self(GLFWwindow* window) {
    return static_cast<GlfwInput*>(glfwGetWindowUserPointer(window));
}

} // namespace

GlfwInput::GlfwInput(GLFWwindow* window) : window_(window) {
    glfwSetWindowUserPointer(window_, this);
    glfwSetKeyCallback(window_, &GlfwInput::key_callback);
    glfwSetMouseButtonCallback(window_, &GlfwInput::mouse_button_callback);
    glfwSetCursorPosCallback(window_, &GlfwInput::cursor_pos_callback);
    // Per-pixel deltas independent of OS cursor acceleration where the platform supports it --
    // exactly what mouse-look wants.
    if (glfwRawMouseMotionSupported() == GLFW_TRUE) {
        glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }
}

GlfwInput::~GlfwInput() {
    glfwSetKeyCallback(window_, nullptr);
    glfwSetMouseButtonCallback(window_, nullptr);
    glfwSetCursorPosCallback(window_, nullptr);
    glfwSetWindowUserPointer(window_, nullptr);
}

void GlfwInput::key_callback(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    GlfwInput* input = self(window);
    if (input == nullptr || action == GLFW_REPEAT) {
        return; // REPEAT would just rewrite the same held state
    }
    const bool down = action == GLFW_PRESS;
    InputState& state = input->state_;
    switch (key) {
    case GLFW_KEY_W: state.move_forward = down; break;
    case GLFW_KEY_S: state.move_back = down; break;
    case GLFW_KEY_A: state.move_left = down; break;
    case GLFW_KEY_D: state.move_right = down; break;
    case GLFW_KEY_SPACE: state.move_up = down; break;
    case GLFW_KEY_LEFT_CONTROL: state.move_down = down; break;
    case GLFW_KEY_LEFT_SHIFT: state.speed_boost = down; break;
    case GLFW_KEY_G:
        if (down) {
            state.pending_walk_toggle = true; // edge: consumed by take_walk_toggle()
        }
        break;
    case GLFW_KEY_F2:
        if (down) {
            state.pending_screenshot = true; // edge: consumed by take_screenshot()
        }
        break;
    case GLFW_KEY_ESCAPE:
        if (down) {
            state.quit_requested = true;
        }
        break;
    default: break;
    }
}

void GlfwInput::mouse_button_callback(GLFWwindow* window, int button, int action, int /*mods*/) {
    GlfwInput* input = self(window);
    if (input == nullptr || button != GLFW_MOUSE_BUTTON_RIGHT) {
        return;
    }
    const bool down = action == GLFW_PRESS;
    input->state_.look_active = down;
    glfwSetInputMode(window, GLFW_CURSOR, down ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    // Force a fresh baseline on the next cursor event -- capturing warps the cursor, and folding
    // that warp into a look delta would snap the view.
    input->have_last_cursor_ = false;
}

void GlfwInput::cursor_pos_callback(GLFWwindow* window, double x, double y) {
    GlfwInput* input = self(window);
    if (input == nullptr) {
        return;
    }
    if (input->have_last_cursor_ && input->state_.look_active) {
        input->state_.pending_look_delta += glm::vec2(static_cast<float>(x - input->last_cursor_x_),
                                                     static_cast<float>(y - input->last_cursor_y_));
    }
    input->last_cursor_x_ = x;
    input->last_cursor_y_ = y;
    input->have_last_cursor_ = true;
}

} // namespace engine::input
