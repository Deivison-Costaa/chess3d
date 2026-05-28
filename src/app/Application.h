#pragma once

#include "ai/Agent.h"
#include "ai/DifficultyLevels.h"
#include "anim/Animator.h"
#include "app/Window.h"
#include "chess/Board.h"
#include "chess/MoveGenerator.h"
#include "chess/Rules.h"
#include "input/InputHandler.h"
#include "render/Camera.h"
#include "render/GltfLoader.h"
#include "render/Mesh.h"
#include "render/Shader.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <memory>
#include <optional>
#include <vector>

namespace chess3d {

class Application {
public:
    Application();
    ~Application();

    int run();

private:
    void updateCameraUbo(float aspect);
    void renderScene(float aspect);
    void renderPieces();
    void renderHighlights();

    void onClickAt(double mouseX, double mouseY);
    void refreshLegalMoves();
    void applyMove(const chess::Move& m);
    void maybeTriggerAi();
    void setDifficulty(ai::Difficulty d);

    Window window_;
    Camera camera_;
    InputHandler input_;

    Shader litShader_;
    Shader highlightShader_;
    Mesh cubeMesh_;
    Mesh highlightQuad_;

    GltfLoader gltf_;
    float gltfWorldScale_ = 1.0f;
    glm::vec3 gltfWorldOffset_{0.0f};

    // Estado de jogo
    chess::Board board_;
    chess::MoveList legalMoves_;
    std::vector<std::string> positionHistory_;
    chess::GameResult result_ = chess::GameResult::Ongoing;

    chess::Square selectedSquare_ = chess::kNoSquare;
    std::optional<chess::Move> lastMove_;

    anim::Animator animator_;
    float lastFrameTime_ = 0.0f;

    std::unique_ptr<ai::Agent> aiAgent_;
    chess::Color aiColor_ = chess::Color::Black;
    ai::Difficulty aiDifficulty_ = ai::Difficulty::Medium;

    GLuint cameraUbo_ = 0;
};

}  // namespace chess3d
