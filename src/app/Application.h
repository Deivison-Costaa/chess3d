#pragma once

#include "app/Window.h"
#include "input/InputHandler.h"
#include "render/Camera.h"
#include "render/GltfLoader.h"
#include "render/Mesh.h"
#include "render/Shader.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace chess3d {

class Application {
public:
    Application();
    ~Application();

    int run();

private:
    void updateCameraUbo(float aspect);
    void renderScene(float aspect);

    Window window_;
    Camera camera_;
    InputHandler input_;

    Shader litShader_;
    Mesh cubeMesh_;
    Mesh floorMesh_;

    GltfLoader gltf_;
    float gltfWorldScale_ = 1.0f;
    glm::vec3 gltfWorldOffset_{0.0f};

    GLuint cameraUbo_ = 0;
};

}  // namespace chess3d
