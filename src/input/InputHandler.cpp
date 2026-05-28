#include "InputHandler.h"

#include "render/Camera.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>

#include <cmath>

namespace chess3d {

void InputHandler::attach(GLFWwindow* window, Camera* camera) {
    window_ = window;
    camera_ = camera;
    glfwSetWindowUserPointer(window_, this);
    glfwSetMouseButtonCallback(window_, &InputHandler::mouseButtonThunk);
    glfwSetCursorPosCallback(window_, &InputHandler::cursorPosThunk);
    glfwSetScrollCallback(window_, &InputHandler::scrollThunk);
    glfwSetKeyCallback(window_, &InputHandler::keyThunk);
    // ImGui também precisa do char callback (para campos de texto) e do focus.
    glfwSetCharCallback(window_, &ImGui_ImplGlfw_CharCallback);
    glfwSetWindowFocusCallback(window_, &ImGui_ImplGlfw_WindowFocusCallback);
    glfwSetCursorEnterCallback(window_, &ImGui_ImplGlfw_CursorEnterCallback);
}

void InputHandler::detach() {
    if (!window_) return;
    glfwSetMouseButtonCallback(window_, nullptr);
    glfwSetCursorPosCallback(window_, nullptr);
    glfwSetScrollCallback(window_, nullptr);
    glfwSetKeyCallback(window_, nullptr);
    glfwSetWindowUserPointer(window_, nullptr);
    window_ = nullptr;
    camera_ = nullptr;
}

void InputHandler::onMouseButton(int button, int action, int /*mods*/) {
    if (!window_) return;
    double x = 0.0, y = 0.0;
    glfwGetCursorPos(window_, &x, &y);
    lastCursor_ = {x, y};

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            leftPressed_ = true;
            leftMovedWhilePressed_ = false;
            leftPressPos_ = {x, y};
            draggingRotate_ = false;  // só vira drag quando mover além da tolerância
        } else {  // GLFW_RELEASE
            const bool wasClick = leftPressed_ && !leftMovedWhilePressed_;
            leftPressed_ = false;
            draggingRotate_ = false;
            if (wasClick && onLeftClick_) onLeftClick_(x, y);
        }
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        draggingPan_ = (action == GLFW_PRESS);
    }
}

void InputHandler::onCursorPos(double xpos, double ypos) {
    const glm::dvec2 cur(xpos, ypos);
    const glm::dvec2 delta = cur - lastCursor_;
    lastCursor_ = cur;

    // Esquerda: vira drag quando excede a tolerância de pixels.
    if (leftPressed_ && !leftMovedWhilePressed_) {
        const glm::dvec2 pd = cur - leftPressPos_;
        if (std::abs(pd.x) > clickPixelTolerance_ || std::abs(pd.y) > clickPixelTolerance_) {
            leftMovedWhilePressed_ = true;
            draggingRotate_ = true;
        }
    }

    if (!camera_) return;

    if (draggingRotate_) {
        camera_->rotate(-static_cast<float>(delta.x) * rotateSensitivity_,
                        -static_cast<float>(delta.y) * rotateSensitivity_);
    }
    if (draggingPan_ && window_) {
        int w = 0, h = 0;
        glfwGetWindowSize(window_, &w, &h);
        const float aspect = (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
        camera_->pan(glm::vec2(static_cast<float>(delta.x), static_cast<float>(delta.y))
                         * panSensitivity_,
                     aspect);
    }
}

void InputHandler::onScroll(double /*xoffset*/, double yoffset) {
    if (!camera_) return;
    const float factor = (yoffset > 0.0) ? (1.0f / zoomStep_) : zoomStep_;
    camera_->zoom(factor);
}

void InputHandler::onKey(int key, int /*scancode*/, int action, int /*mods*/) {
    if (action != GLFW_PRESS) return;

    if (camera_) {
        switch (key) {
            case GLFW_KEY_R: camera_->reset(); return;
            case GLFW_KEY_F: camera_->setTopDown(); return;
            case GLFW_KEY_1: camera_->setSideView(true);  return;
            case GLFW_KEY_2: camera_->setSideView(false); return;
            default: break;
        }
    }
    if (onGameKey_) onGameKey_(key);
}

void InputHandler::mouseButtonThunk(GLFWwindow* w, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(w, button, action, mods);
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (auto* self = static_cast<InputHandler*>(glfwGetWindowUserPointer(w))) {
        self->onMouseButton(button, action, mods);
    }
}

void InputHandler::cursorPosThunk(GLFWwindow* w, double x, double y) {
    ImGui_ImplGlfw_CursorPosCallback(w, x, y);
    if (auto* self = static_cast<InputHandler*>(glfwGetWindowUserPointer(w))) {
        self->onCursorPos(x, y);
    }
}

void InputHandler::scrollThunk(GLFWwindow* w, double x, double y) {
    ImGui_ImplGlfw_ScrollCallback(w, x, y);
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (auto* self = static_cast<InputHandler*>(glfwGetWindowUserPointer(w))) {
        self->onScroll(x, y);
    }
}

void InputHandler::keyThunk(GLFWwindow* w, int key, int sc, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(w, key, sc, action, mods);
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    if (auto* self = static_cast<InputHandler*>(glfwGetWindowUserPointer(w))) {
        self->onKey(key, sc, action, mods);
    }
}

}  // namespace chess3d
