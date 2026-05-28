#include "Application.h"

#include "ai/MinimaxAgent.h"
#include "core/BoardCoords.h"
#include "render/Picker.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
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

constexpr const char* kBoardMeshName = "Cube.006";

// Nome canônico do mesh por (tipo, cor) — primeira ocorrência no .glb da Jaximus.
const char* meshNameFor(chess::PieceType type, chess::Color color) {
    using namespace chess;
    const bool w = (color == Color::White);
    switch (type) {
        case PieceType::Pawn:   return w ? "Pawn"   : "Pawn.B";
        case PieceType::Rook:   return w ? "Rook"   : "Rook.B";
        case PieceType::Knight: return w ? "Knight" : "Knight.B";
        case PieceType::Bishop: return w ? "Bishop" : "Bishop.B";
        case PieceType::Queen:  return w ? "Queen"  : "Queen.B";
        case PieceType::King:   return w ? "King"   : "King.B";
        default: return nullptr;
    }
}

const glm::vec3 kWhiteColor(0.92f, 0.88f, 0.78f);
const glm::vec3 kBlackColor(0.10f, 0.10f, 0.12f);

}  // namespace

Application::Application()
    : window_({1280, 720, "Chess3D", true, true}) {
    if (!window_.ok()) {
        spdlog::critical("Application: window not initialised");
        return;
    }

    camera_.setFovYDeg(50.0f);
    camera_.setNearFar(0.1f, 100.0f);
    camera_.setHomeView(glm::vec3(0.0f), 14.0f, glm::radians(35.0f), glm::radians(40.0f));

    input_.attach(window_.handle(), &camera_);
    input_.setOnLeftClick([this](double x, double y) { onClickAt(x, y); });
    input_.setOnGameKey([this](int key) {
        switch (key) {
            case GLFW_KEY_7: setDifficulty(ai::Difficulty::Easy);   break;
            case GLFW_KEY_8: setDifficulty(ai::Difficulty::Medium); break;
            case GLFW_KEY_9: setDifficulty(ai::Difficulty::Hard);   break;
            default: break;
        }
    });

    litShader_ = Shader(assetPath("shaders/lit.vert"), assetPath("shaders/lit.frag"));
    highlightShader_ = Shader(assetPath("shaders/highlight.vert"),
                              assetPath("shaders/highlight.frag"));
    if (litShader_.valid())      litShader_.bindUniformBlock("CameraBlock", kCameraBlockBinding);
    if (highlightShader_.valid()) highlightShader_.bindUniformBlock("CameraBlock", kCameraBlockBinding);

    cubeMesh_ = Mesh::makeCube(0.6f);
    highlightQuad_ = Mesh::makeQuad(0.5f);  // 1x1 com half=0.5 — escala via uniform

    if (gltf_.loadFromFile(assetPath("models/chessboard.glb"))) {
        if (const auto* board = gltf_.find(kBoardMeshName)) {
            const glm::vec3 size = board->bboxMax - board->bboxMin;
            const float boardWidth = std::max(size.x, size.z);
            if (boardWidth > 0.0f) {
                gltfWorldScale_ = (8.0f * kSquareSize) / boardWidth;
            }
            gltfWorldOffset_ = glm::vec3(-0.5f * (board->bboxMin.x + board->bboxMax.x),
                                         -board->bboxMax.y,
                                         -0.5f * (board->bboxMin.z + board->bboxMax.z));
        }
        spdlog::info("GltfLoader: world scale={:.4f}, offset=({:.2f},{:.2f},{:.2f})",
                     gltfWorldScale_, gltfWorldOffset_.x, gltfWorldOffset_.y, gltfWorldOffset_.z);
    }

    glCreateBuffers(1, &cameraUbo_);
    glNamedBufferStorage(cameraUbo_, sizeof(CameraBlockData), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, kCameraBlockBinding, cameraUbo_);

    board_.reset();
    positionHistory_.clear();
    positionHistory_.push_back(chess::positionKey(board_));
    refreshLegalMoves();
    animator_.initFromBoard(board_);

    aiColor_ = chess::Color::Black;
    setDifficulty(ai::Difficulty::Medium);
}

void Application::setDifficulty(ai::Difficulty d) {
    aiDifficulty_ = d;
    aiAgent_ = ai::makeAgent(d);
    const char* label = "?";
    switch (d) {
        case ai::Difficulty::Easy:   label = "Easy";   break;
        case ai::Difficulty::Medium: label = "Medium"; break;
        case ai::Difficulty::Hard:   label = "Hard";   break;
        case ai::Difficulty::Master: label = "Master"; break;
    }
    spdlog::info("AI: {} (level={}) jogando com {}",
                 aiAgent_->name(), label,
                 aiColor_ == chess::Color::White ? "white" : "black");
}

Application::~Application() {
    input_.detach();
    if (cameraUbo_) {
        glDeleteBuffers(1, &cameraUbo_);
        cameraUbo_ = 0;
    }
}

void Application::refreshLegalMoves() {
    legalMoves_.clear();
    chess::generateLegalMoves(board_, legalMoves_);
    result_ = chess::evaluateGame(board_, positionHistory_);
    if (result_ != chess::GameResult::Ongoing) {
        spdlog::info("Game over: {}", chess::gameResultName(result_));
    }
}

void Application::maybeTriggerAi() {
    if (!aiAgent_) return;
    if (result_ != chess::GameResult::Ongoing) return;
    if (animator_.isAnimating()) return;
    if (board_.sideToMove() != aiColor_) return;

    const chess::Move m = aiAgent_->chooseMove(board_);
    if (m.isNull()) {
        spdlog::warn("AI: chooseMove returned null move");
        return;
    }
    const auto info = aiAgent_->lastInfo();
    spdlog::info("AI move: {} (eval={} cp, nodes={}, time={} us)",
                 chess::moveToUci(m), info.evaluation, info.nodesVisited,
                 info.elapsed.count());
    applyMove(m);
}

void Application::applyMove(const chess::Move& m) {
    chess::Board boardBefore = board_;  // snapshot para o Animator
    chess::UndoInfo undo;
    board_.makeMove(m, undo);
    lastMove_ = m;
    positionHistory_.push_back(chess::positionKey(board_));
    refreshLegalMoves();
    animator_.animateMove(m, boardBefore);
    spdlog::info("Move: {} | side to move: {}",
                 chess::moveToUci(m),
                 board_.sideToMove() == chess::Color::White ? "white" : "black");
}

void Application::onClickAt(double mouseX, double mouseY) {
    if (result_ != chess::GameResult::Ongoing) return;
    if (animator_.isAnimating()) return;  // regra de ouro: bloqueia input enquanto anima
    if (aiAgent_ && board_.sideToMove() == aiColor_) return;  // vez da IA

    const SquarePick pick = pickSquare(mouseX, mouseY,
                                       window_.width(), window_.height(),
                                       camera_.viewMatrix(),
                                       camera_.projectionMatrix(window_.aspect()),
                                       0.0f);
    if (!pick.valid()) {
        selectedSquare_ = chess::kNoSquare;
        return;
    }

    const chess::Square sq = chess::makeSquare(pick.file, pick.rank);
    const chess::Piece p = board_.pieceAt(sq);

    if (selectedSquare_ == chess::kNoSquare) {
        if (!p.empty() && p.color == board_.sideToMove()) {
            selectedSquare_ = sq;
        }
        return;
    }

    // Há uma peça selecionada — buscar movimento legal de selected → sq.
    chess::Move chosen{};
    bool found = false;
    for (const auto& mv : legalMoves_) {
        if (mv.from == selectedSquare_ && mv.to == sq) {
            // Promoção: por enquanto auto-Queen (Fase 9 trará o diálogo modal).
            if (mv.isPromotion() && mv.promotion != chess::PieceType::Queen) continue;
            chosen = mv;
            found = true;
            break;
        }
    }

    if (found) {
        applyMove(chosen);
        selectedSquare_ = chess::kNoSquare;
    } else if (!p.empty() && p.color == board_.sideToMove()) {
        // troca seleção para outra peça do mesmo lado
        selectedSquare_ = sq;
    } else {
        selectedSquare_ = chess::kNoSquare;
    }
}

void Application::updateCameraUbo(float aspect) {
    CameraBlockData data;
    data.view = camera_.viewMatrix();
    data.projection = camera_.projectionMatrix(aspect);
    data.cameraPos = glm::vec4(camera_.position(), 1.0f);
    glNamedBufferSubData(cameraUbo_, 0, sizeof(CameraBlockData), &data);
}

void Application::renderPieces() {
    if (!litShader_.valid() || !gltf_.ok()) return;
    litShader_.use();
    litShader_.setVec3("uLightDir", glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)));
    litShader_.setVec3("uLightColor", glm::vec3(1.0f, 0.97f, 0.9f));

    const glm::mat4 normalize =
        glm::scale(glm::mat4(1.0f), glm::vec3(gltfWorldScale_))
        * glm::translate(glm::mat4(1.0f), gltfWorldOffset_);

    // Tabuleiro
    if (const auto* board = gltf_.find(kBoardMeshName)) {
        litShader_.setMat4("uModel", normalize);
        litShader_.setMat3("uNormalMatrix", glm::mat3(glm::transpose(glm::inverse(normalize))));
        litShader_.setVec3("uAlbedo", glm::vec3(0.30f, 0.22f, 0.16f));
        litShader_.setFloat("uAlpha", 1.0f);
        board->mesh.draw();
    }

    // Peças do Animator (estado visual, possivelmente em transição)
    bool blendEnabled = false;
    for (const auto& v : animator_.pieces()) {
        if (!v.alive) continue;
        const char* meshName = meshNameFor(v.type, v.color);
        if (!meshName) continue;
        const auto* item = gltf_.find(meshName);
        if (!item) continue;

        const glm::mat4 model =
            glm::translate(glm::mat4(1.0f), v.worldPos)
            * glm::rotate(glm::mat4(1.0f), glm::radians(v.yawDeg), glm::vec3(0.0f, 1.0f, 0.0f))
            * glm::scale(glm::mat4(1.0f), glm::vec3(gltfWorldScale_ * v.scale));
        litShader_.setMat4("uModel", model);
        litShader_.setMat3("uNormalMatrix", glm::mat3(glm::transpose(glm::inverse(model))));
        glm::vec3 albedo = v.color == chess::Color::White ? kWhiteColor : kBlackColor;
        litShader_.setVec3("uAlbedo", albedo);
        litShader_.setFloat("uAlpha", v.alpha);
        // Habilita blend quando peça está fadeando (captura).
        const bool needsBlend = v.alpha < 0.999f;
        if (needsBlend && !blendEnabled) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            blendEnabled = true;
        } else if (!needsBlend && blendEnabled) {
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            blendEnabled = false;
        }
        item->mesh.draw();
    }
    if (blendEnabled) {
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}

void Application::renderHighlights() {
    if (!highlightShader_.valid()) return;

    // Renderiza acima do tabuleiro, sem escrever em depth — quads translúcidos.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    highlightShader_.use();

    constexpr float kHoverY = 0.02f;  // pouco acima do tabuleiro para evitar z-fighting

    auto drawSquare = [&](int file, int rank, const glm::vec4& color) {
        const glm::mat4 model =
            glm::translate(glm::mat4(1.0f),
                           squareToWorld(file, rank) + glm::vec3(0.0f, kHoverY, 0.0f))
            * glm::scale(glm::mat4(1.0f), glm::vec3(kSquareSize, 1.0f, kSquareSize));
        highlightShader_.setMat4("uModel", model);
        highlightShader_.setVec4("uColor", color);
        highlightQuad_.draw();
    };

    // Última jogada (origem + destino)
    if (lastMove_) {
        const glm::vec4 yellow(0.95f, 0.85f, 0.30f, 0.35f);
        drawSquare(chess::fileOf(lastMove_->from), chess::rankOf(lastMove_->from), yellow);
        drawSquare(chess::fileOf(lastMove_->to),   chess::rankOf(lastMove_->to),   yellow);
    }

    // Peça selecionada
    if (selectedSquare_ != chess::kNoSquare) {
        drawSquare(chess::fileOf(selectedSquare_), chess::rankOf(selectedSquare_),
                   glm::vec4(0.30f, 0.65f, 0.95f, 0.45f));

        // Destinos legais a partir da peça selecionada
        for (const auto& mv : legalMoves_) {
            if (mv.from != selectedSquare_) continue;
            const bool capture = mv.isCapture();
            const glm::vec4 col = capture
                ? glm::vec4(0.95f, 0.30f, 0.30f, 0.50f)
                : glm::vec4(0.30f, 0.85f, 0.40f, 0.45f);
            drawSquare(chess::fileOf(mv.to), chess::rankOf(mv.to), col);
        }
    }

    // Rei em xeque (pulsante leve via tempo)
    if (board_.inCheck(board_.sideToMove())) {
        const chess::Square kSq = board_.kingSquare(board_.sideToMove());
        const float t = static_cast<float>(glfwGetTime());
        const float pulse = 0.35f + 0.20f * std::sin(t * 6.0f);
        drawSquare(chess::fileOf(kSq), chess::rankOf(kSq),
                   glm::vec4(0.95f, 0.45f, 0.10f, pulse));
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void Application::renderScene(float aspect) {
    updateCameraUbo(aspect);
    renderPieces();
    renderHighlights();
}

int Application::run() {
    if (!window_.ok()) return 1;
    spdlog::info("Chess3D — main loop (LMB seleciona/move, drag rotaciona, RMB pan, scroll zoom, R/F/1/2 câmera, ESC sai)");

    lastFrameTime_ = static_cast<float>(glfwGetTime());

    while (!window_.shouldClose()) {
        window_.pollEvents();
        if (glfwGetKey(window_.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            window_.setShouldClose(true);
        }

        const float now = static_cast<float>(glfwGetTime());
        const float dt = std::min(now - lastFrameTime_, 0.1f);  // cap em 100ms (evita salto após pausa)
        lastFrameTime_ = now;
        animator_.update(dt);

        maybeTriggerAi();

        glClearColor(0.07f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderScene(window_.aspect());
        window_.swap();
    }
    return 0;
}

}  // namespace chess3d
