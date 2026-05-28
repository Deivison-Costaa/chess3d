#include "Window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <spdlog/spdlog.h>

namespace chess3d {

namespace {

bool gGlfwInitialised = false;

void glfwErrorCallback(int code, const char* description) {
    spdlog::error("GLFW error {}: {}", code, description);
}

void GLAPIENTRY glDebugCallback(GLenum /*source*/, GLenum type, GLuint id,
                              GLenum severity, GLsizei /*length*/,
                              const GLchar* message, const void* /*user*/) {
    if (id == 131185 || id == 131204) return;
    auto level = spdlog::level::debug;
    if (severity == GL_DEBUG_SEVERITY_HIGH) level = spdlog::level::err;
    else if (severity == GL_DEBUG_SEVERITY_MEDIUM) level = spdlog::level::warn;
    else if (severity == GL_DEBUG_SEVERITY_LOW) level = spdlog::level::info;
    spdlog::log(level, "[GL type=0x{:x} id={}] {}", type, id, message);
}

void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
}

}  // namespace

Window::Window(const WindowSpec& spec) {
    if (!gGlfwInitialised) {
        glfwSetErrorCallback(&glfwErrorCallback);
        if (!glfwInit()) {
            spdlog::critical("Window: glfwInit failed");
            return;
        }
        gGlfwInitialised = true;
        glfwOwned_ = true;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, spec.debugContext ? GLFW_TRUE : GLFW_FALSE);
#else
    (void)spec.debugContext;
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);  // 4x MSAA — barato e melhora muito a aparência

    handle_ = glfwCreateWindow(spec.width, spec.height, spec.title.c_str(), nullptr, nullptr);
    if (!handle_) {
        spdlog::critical("Window: failed to create GLFW window (OpenGL 4.6 Core required)");
        return;
    }

    glfwMakeContextCurrent(handle_);
    glfwSwapInterval(spec.vsync ? 1 : 0);
    glfwSetFramebufferSizeCallback(handle_, &framebufferSizeCallback);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        spdlog::critical("Window: failed to load OpenGL via glad");
        glfwDestroyWindow(handle_);
        handle_ = nullptr;
        return;
    }

    spdlog::info("OpenGL {} | GLSL {} | renderer={}",
                 reinterpret_cast<const char*>(glGetString(GL_VERSION)),
                 reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)),
                 reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

    GLint flags = 0;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(&glDebugCallback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        spdlog::debug("KHR_debug callback installed");
    }

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(handle_, &fbW, &fbH);
    glViewport(0, 0, fbW, fbH);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glEnable(GL_MULTISAMPLE);

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    // install_callbacks=false: vamos chamar os callbacks do ImGui manualmente
    // a partir dos thunks do InputHandler para não sobrescrevê-los depois.
    ImGui_ImplGlfw_InitForOpenGL(handle_, false);
    ImGui_ImplOpenGL3_Init("#version 460 core");
    imguiInitialised_ = true;
    spdlog::debug("ImGui initialised");
}

Window::~Window() {
    if (imguiInitialised_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
    if (handle_) glfwDestroyWindow(handle_);
    if (glfwOwned_) {
        glfwTerminate();
        gGlfwInitialised = false;
    }
}

void Window::beginImGuiFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Window::endImGuiFrame() {
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool Window::imguiWantsMouse() const {
    return imguiInitialised_ && ImGui::GetIO().WantCaptureMouse;
}

bool Window::imguiWantsKeyboard() const {
    return imguiInitialised_ && ImGui::GetIO().WantCaptureKeyboard;
}

bool Window::shouldClose() const {
    return handle_ ? glfwWindowShouldClose(handle_) : true;
}

void Window::setShouldClose(bool v) {
    if (handle_) glfwSetWindowShouldClose(handle_, v ? GLFW_TRUE : GLFW_FALSE);
}

void Window::swap() {
    if (handle_) glfwSwapBuffers(handle_);
}

void Window::pollEvents() {
    glfwPollEvents();
}

int Window::width() const {
    int w = 0, h = 0;
    if (handle_) glfwGetFramebufferSize(handle_, &w, &h);
    return w;
}

int Window::height() const {
    int w = 0, h = 0;
    if (handle_) glfwGetFramebufferSize(handle_, &w, &h);
    return h;
}

float Window::aspect() const {
    const int h = height();
    return (h > 0) ? (static_cast<float>(width()) / static_cast<float>(h)) : 1.0f;
}

int Window::windowWidth() const {
    int w = 0, h = 0;
    if (handle_) glfwGetWindowSize(handle_, &w, &h);
    return w;
}

int Window::windowHeight() const {
    int w = 0, h = 0;
    if (handle_) glfwGetWindowSize(handle_, &w, &h);
    return h;
}

}  // namespace chess3d
