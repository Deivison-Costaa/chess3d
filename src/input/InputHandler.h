#pragma once

#include <glm/glm.hpp>

struct GLFWwindow;

namespace chess3d {

class Camera;

class InputHandler {
public:
    InputHandler() = default;

    void attach(GLFWwindow* window, Camera* camera);
    void detach();

    bool isMouseCaptured() const { return draggingRotate_ || draggingPan_; }

private:
    void onMouseButton(int button, int action, int mods);
    void onCursorPos(double xpos, double ypos);
    void onScroll(double xoffset, double yoffset);
    void onKey(int key, int scancode, int action, int mods);

    static void mouseButtonThunk(GLFWwindow* w, int button, int action, int mods);
    static void cursorPosThunk(GLFWwindow* w, double x, double y);
    static void scrollThunk(GLFWwindow* w, double x, double y);
    static void keyThunk(GLFWwindow* w, int key, int sc, int action, int mods);

    GLFWwindow* window_ = nullptr;
    Camera* camera_ = nullptr;

    bool draggingRotate_ = false;
    bool draggingPan_ = false;
    glm::dvec2 lastCursor_{0.0};

    float rotateSensitivity_ = 0.006f;
    float panSensitivity_ = 0.0015f;
    float zoomStep_ = 1.10f;
};

}  // namespace chess3d
