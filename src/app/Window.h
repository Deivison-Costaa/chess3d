#pragma once

#include <string>

struct GLFWwindow;

namespace chess3d {

struct WindowSpec {
    int width = 1280;
    int height = 720;
    std::string title = "Chess3D";
    bool vsync = true;
    bool debugContext = true;
};

class Window {
public:
    explicit Window(const WindowSpec& spec);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ok() const { return handle_ != nullptr; }
    GLFWwindow* handle() const { return handle_; }

    bool shouldClose() const;
    void setShouldClose(bool v);
    void swap();
    void pollEvents();

    int width() const;
    int height() const;
    float aspect() const;

private:
    GLFWwindow* handle_ = nullptr;
    bool glfwOwned_ = false;
};

}  // namespace chess3d
