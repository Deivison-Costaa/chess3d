#include "Application.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>

namespace chess3d {

namespace {

constexpr GLuint kCameraBlockBinding = 0;

struct CameraBlockData {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::vec4 cameraPos{0.0f};
};

std::filesystem::path assetPath(const char* relative) {
#ifdef CHESS3D_ASSETS_DIR
    return std::filesystem::path(CHESS3D_ASSETS_DIR) / relative;
#else
    return std::filesystem::path("assets") / relative;
#endif
}

}  // namespace

Application::Application()
    : window_({1280, 720, "Chess3D", true, true}) {
    if (!window_.ok()) {
        spdlog::critical("Application: window not initialised");
        return;
    }

    camera_.setFovYDeg(50.0f);
    camera_.setNearFar(0.1f, 100.0f);
    camera_.setHomeView(glm::vec3(0.0f), 12.0f, glm::radians(35.0f), glm::radians(35.0f));

    input_.attach(window_.handle(), &camera_);

    litShader_ = Shader(assetPath("shaders/lit.vert"), assetPath("shaders/lit.frag"));
    if (litShader_.valid()) {
        litShader_.bindUniformBlock("CameraBlock", kCameraBlockBinding);
    }

    cubeMesh_ = Mesh::makeCube(0.6f);
    floorMesh_ = Mesh::makeQuad(8.0f);

    glCreateBuffers(1, &cameraUbo_);
    glNamedBufferStorage(cameraUbo_, sizeof(CameraBlockData), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, kCameraBlockBinding, cameraUbo_);
}

Application::~Application() {
    input_.detach();
    if (cameraUbo_) {
        glDeleteBuffers(1, &cameraUbo_);
        cameraUbo_ = 0;
    }
}

void Application::updateCameraUbo(float aspect) {
    CameraBlockData data;
    data.view = camera_.viewMatrix();
    data.projection = camera_.projectionMatrix(aspect);
    data.cameraPos = glm::vec4(camera_.position(), 1.0f);
    glNamedBufferSubData(cameraUbo_, 0, sizeof(CameraBlockData), &data);
}

void Application::renderScene(float aspect) {
    updateCameraUbo(aspect);

    if (!litShader_.valid()) return;
    litShader_.use();

    litShader_.setVec3("uLightDir", glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)));
    litShader_.setVec3("uLightColor", glm::vec3(1.0f, 0.97f, 0.9f));

    // chão
    {
        const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.8f, 0.0f));
        litShader_.setMat4("uModel", model);
        litShader_.setMat3("uNormalMatrix", glm::mat3(glm::transpose(glm::inverse(model))));
        litShader_.setVec3("uAlbedo", glm::vec3(0.18f, 0.22f, 0.28f));
        floorMesh_.draw();
    }

    // cubo central girando
    {
        const float t = static_cast<float>(glfwGetTime());
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::rotate(model, t * 0.6f, glm::vec3(0.2f, 1.0f, 0.1f));
        litShader_.setMat4("uModel", model);
        litShader_.setMat3("uNormalMatrix", glm::mat3(glm::transpose(glm::inverse(model))));
        litShader_.setVec3("uAlbedo", glm::vec3(0.85f, 0.55f, 0.25f));
        cubeMesh_.draw();
    }
}

int Application::run() {
    if (!window_.ok()) return 1;

    spdlog::info("Chess3D — entering main loop (ESC quits, R reset, F top-down, 1/2 white/black side, scroll zooms, LMB rotates, RMB pans)");

    while (!window_.shouldClose()) {
        window_.pollEvents();

        if (glfwGetKey(window_.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            window_.setShouldClose(true);
        }

        glClearColor(0.07f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderScene(window_.aspect());

        window_.swap();
    }
    return 0;
}

}  // namespace chess3d
