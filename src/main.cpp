#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <cstdlib>

namespace {

constexpr int kInitialWidth = 1280;
constexpr int kInitialHeight = 720;
constexpr const char* kWindowTitle = "Chess3D";

void glfwErrorCallback(int code, const char* description) {
    spdlog::error("GLFW error {}: {}", code, description);
}

void framebufferSizeCallback(GLFWwindow*, int width, int height) {
    glViewport(0, 0, width, height);
}

void APIENTRY glDebugCallback(GLenum /*source*/, GLenum type, GLuint id,
                              GLenum severity, GLsizei /*length*/,
                              const GLchar* message, const void* /*user*/) {
    if (id == 131185 || id == 131204) {
        return;  // NVIDIA buffer-usage / texture-base-level noise
    }
    auto level = spdlog::level::debug;
    if (severity == GL_DEBUG_SEVERITY_HIGH) level = spdlog::level::err;
    else if (severity == GL_DEBUG_SEVERITY_MEDIUM) level = spdlog::level::warn;
    else if (severity == GL_DEBUG_SEVERITY_LOW) level = spdlog::level::info;
    spdlog::log(level, "[GL type=0x{:x} id={}] {}", type, id, message);
}

}  // namespace

int main() {
    spdlog::set_level(spdlog::level::debug);
    spdlog::info("Chess3D — starting");

    glfwSetErrorCallback(&glfwErrorCallback);
    if (!glfwInit()) {
        spdlog::critical("Failed to initialise GLFW");
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(kInitialWidth, kInitialHeight, kWindowTitle, nullptr, nullptr);
    if (!window) {
        spdlog::critical("Failed to create GLFW window (OpenGL 4.6 Core required)");
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetFramebufferSizeCallback(window, &framebufferSizeCallback);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        spdlog::critical("Failed to load OpenGL via glad");
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
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

    int fbWidth = 0;
    int fbHeight = 0;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glClearColor(0.10f, 0.12f, 0.16f, 1.0f);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    spdlog::info("Chess3D — bye");
    return EXIT_SUCCESS;
}
