#pragma once

#include <glm/glm.hpp>

#include <functional>

struct GLFWwindow;

namespace chess3d {

class Camera;

class InputHandler {
public:
    using ClickCallback = std::function<void(double mouseX, double mouseY)>;
    using KeyCallback   = std::function<void(int key)>;

    InputHandler() = default;

    void attach(GLFWwindow* window, Camera* camera);
    void detach();

    bool isMouseCaptured() const { return draggingRotate_ || draggingPan_; }

    void setOnLeftClick(ClickCallback cb) { onLeftClick_ = std::move(cb); }
    void setOnGameKey(KeyCallback cb)     { onGameKey_   = std::move(cb); }

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
    bool leftPressed_ = false;
    bool leftMovedWhilePressed_ = false;
    glm::dvec2 leftPressPos_{0.0};
    glm::dvec2 lastCursor_{0.0};

    ClickCallback onLeftClick_;
    KeyCallback   onGameKey_;

    float rotateSensitivity_ = 0.006f;
    float panSensitivity_ = 0.0015f;
    float zoomStep_ = 1.10f;
    float clickPixelTolerance_ = 4.0f;
};

}  // namespace chess3d
