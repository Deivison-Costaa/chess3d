#pragma once

#include "app/Window.h"
#include "input/InputHandler.h"
#include "render/Camera.h"
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

    GLuint cameraUbo_ = 0;
};

}  // namespace chess3d
