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
#include "ui/GameUi.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <deque>
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
    // ── Loop / render ──
    void updateCameraUbo(float aspect);
    void renderScene(float aspect);
    void renderPieces();
    void renderHighlights();
    void renderUi();

    // ── Input ──
    void onClickAt(double mouseX, double mouseY);

    // ── Game flow ──
    void startNewGame(const ui::GameSetup& setup);
    void backToMenu();
    void undoLastTurn();

    void refreshLegalMoves();
    void applyMove(const chess::Move& m);
    void maybeTriggerAi();
    void setDifficulty(ai::Difficulty d);
    void rebuildHudData();

    // ── Estado ──
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

    chess::Board board_;
    chess::MoveList legalMoves_;
    std::vector<std::string> positionHistory_;
    chess::GameResult result_ = chess::GameResult::Ongoing;

    chess::Square selectedSquare_ = chess::kNoSquare;
    std::optional<chess::Move> lastMove_;
    std::optional<ui::PromotionRequest> pendingPromotion_;

    anim::Animator animator_;
    float lastFrameTime_ = 0.0f;

    std::unique_ptr<ai::Agent> aiAgent_;
    chess::Color aiColor_ = chess::Color::Black;
    chess::Color humanColor_ = chess::Color::White;
    ai::Difficulty aiDifficulty_ = ai::Difficulty::Medium;
    bool aiThinking_ = false;

    // Histórico p/ SAN + capturas + undo
    struct Played {
        chess::Move move;
        std::string san;
        chess::Board boardBefore;  // snapshot completo pra restaurar (mais simples que cadeia de UndoInfo)
    };
    std::vector<Played> played_;
    std::vector<chess::Piece> capturedByWhite_;
    std::vector<chess::Piece> capturedByBlack_;

    ui::GameUi gameUi_;
    ui::AppState state_ = ui::AppState::MainMenu;
    ui::HudData hud_;

    GLuint cameraUbo_ = 0;
};

}  // namespace chess3d
