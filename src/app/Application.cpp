#include "Application.h"

#include "ai/MinimaxAgent.h"
#include "chess/Notation.h"
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
        if (state_ != ui::AppState::Playing) return;
        switch (key) {
            case GLFW_KEY_7: setDifficulty(ai::Difficulty::Easy);   break;
            case GLFW_KEY_8: setDifficulty(ai::Difficulty::Medium); break;
            case GLFW_KEY_9: setDifficulty(ai::Difficulty::Hard);   break;
            default: break;
        }
    });

    litShader_       = Shader(assetPath("shaders/lit.vert"),       assetPath("shaders/lit.frag"));
    highlightShader_ = Shader(assetPath("shaders/highlight.vert"), assetPath("shaders/highlight.frag"));
    if (litShader_.valid())       litShader_.bindUniformBlock("CameraBlock",       kCameraBlockBinding);
    if (highlightShader_.valid()) highlightShader_.bindUniformBlock("CameraBlock", kCameraBlockBinding);

    cubeMesh_      = Mesh::makeCube(0.6f);
    highlightQuad_ = Mesh::makeQuad(0.5f);

    if (gltf_.loadFromFile(assetPath("models/chessboard.glb"))) {
        if (const auto* board = gltf_.find(kBoardMeshName)) {
            const glm::vec3 size = board->bboxMax - board->bboxMin;
            const float boardWidth = std::max(size.x, size.z);
            if (boardWidth > 0.0f) gltfWorldScale_ = (8.0f * kSquareSize) / boardWidth;
            gltfWorldOffset_ = glm::vec3(-0.5f * (board->bboxMin.x + board->bboxMax.x),
                                         -board->bboxMax.y,
                                         -0.5f * (board->bboxMin.z + board->bboxMax.z));
        }
    }

    glCreateBuffers(1, &cameraUbo_);
    glNamedBufferStorage(cameraUbo_, sizeof(CameraBlockData), nullptr, GL_DYNAMIC_STORAGE_BIT);
    glBindBufferBase(GL_UNIFORM_BUFFER, kCameraBlockBinding, cameraUbo_);

    // Wire UI callbacks
    gameUi_.setStartGame([this](const ui::GameSetup& s) { startNewGame(s); });
    gameUi_.setExit([this]() { window_.setShouldClose(true); });
    gameUi_.setPromotionPick([this](chess::PieceType pt) {
        if (!pendingPromotion_) return;
        chess::Move m = pendingPromotion_->baseMove;
        m.promotion = pt;
        m.flag = pendingPromotion_->isCapture ? chess::MoveFlag::PromotionCapture
                                              : chess::MoveFlag::Promotion;
        pendingPromotion_.reset();
        applyMove(m);
    });
    gameUi_.setNewGame([this]() { startNewGame(gameUi_.setup()); });
    gameUi_.setBackToMenu([this]() { backToMenu(); });
    gameUi_.setUndo([this]() { undoLastTurn(); });

    // Inicializa cena vazia (Menu) — Animator com tabuleiro padrão pra ficar bonito de fundo
    board_.reset();
    animator_.initFromBoard(board_);
}

Application::~Application() {
    input_.detach();
    if (cameraUbo_) {
        glDeleteBuffers(1, &cameraUbo_);
        cameraUbo_ = 0;
    }
}

void Application::startNewGame(const ui::GameSetup& setup) {
    humanColor_ = setup.humanColor;
    aiColor_ = chess::other(setup.humanColor);
    aiDifficulty_ = setup.difficulty;
    aiAgent_ = ai::makeAgent(setup.difficulty);

    board_.reset();
    positionHistory_.clear();
    positionHistory_.push_back(chess::positionKey(board_));
    played_.clear();
    capturedByWhite_.clear();
    capturedByBlack_.clear();
    selectedSquare_ = chess::kNoSquare;
    lastMove_.reset();
    pendingPromotion_.reset();
    aiThinking_ = false;
    refreshLegalMoves();
    animator_.initFromBoard(board_);

    // Câmera atrás do lado humano
    const bool whiteSide = (humanColor_ == chess::Color::White);
    camera_.setHomeView(glm::vec3(0.0f), 14.0f,
                         whiteSide ? 0.0f : glm::pi<float>(),
                         glm::radians(40.0f));

    state_ = ui::AppState::Playing;
    spdlog::info("Nova partida: humano={}, IA={}, dificuldade={}",
                 whiteSide ? "white" : "black",
                 whiteSide ? "black" : "white",
                 static_cast<int>(setup.difficulty));
}

void Application::backToMenu() {
    state_ = ui::AppState::MainMenu;
    selectedSquare_ = chess::kNoSquare;
    pendingPromotion_.reset();
    aiThinking_ = false;
}

void Application::setDifficulty(ai::Difficulty d) {
    aiDifficulty_ = d;
    aiAgent_ = ai::makeAgent(d);
    gameUi_.setup().difficulty = d;
    spdlog::info("Dificuldade: {} ({})", static_cast<int>(d), aiAgent_->name());
}

void Application::refreshLegalMoves() {
    legalMoves_.clear();
    chess::generateLegalMoves(board_, legalMoves_);
    result_ = chess::evaluateGame(board_, positionHistory_);
    if (result_ != chess::GameResult::Ongoing) {
        spdlog::info("Fim de jogo: {}", chess::gameResultName(result_));
        state_ = ui::AppState::GameOver;
    }
}

void Application::applyMove(const chess::Move& m) {
    chess::Board boardBefore = board_;
    const std::string san = chess::moveToSan(m, boardBefore);

    // Identifica peça capturada (incluindo en passant)
    chess::Piece captured{};
    if (m.flag == chess::MoveFlag::Capture || m.flag == chess::MoveFlag::PromotionCapture) {
        captured = board_.pieceAt(m.to);
    } else if (m.flag == chess::MoveFlag::EnPassant) {
        const int dir = chess::pawnForward(boardBefore.pieceAt(m.from).color);
        captured = board_.pieceAt(chess::makeSquare(chess::fileOf(m.to), chess::rankOf(m.to) - dir));
    }

    chess::UndoInfo undo;
    board_.makeMove(m, undo);
    lastMove_ = m;
    positionHistory_.push_back(chess::positionKey(board_));
    played_.push_back({m, san, boardBefore});

    if (!captured.empty()) {
        if (captured.color == chess::Color::Black) capturedByWhite_.push_back(captured);
        else                                       capturedByBlack_.push_back(captured);
    }

    animator_.animateMove(m, boardBefore);
    refreshLegalMoves();
    spdlog::info("{}{} {}",
                 (boardBefore.sideToMove() == chess::Color::White) ? "" : "  ",
                 san, chess::moveToUci(m));
}

void Application::undoLastTurn() {
    if (state_ != ui::AppState::Playing) return;
    if (animator_.isAnimating()) return;
    // Desfaz 2 plies se houver — volta pro turno do jogador.
    const int undoCount = (played_.size() >= 2) ? 2 : (played_.size() >= 1 ? 1 : 0);
    for (int i = 0; i < undoCount; ++i) {
        if (played_.empty()) break;
        const auto& last = played_.back();
        // Restaura capturas no contador
        auto restoreCapture = [&](chess::MoveFlag f) {
            if (f == chess::MoveFlag::Capture || f == chess::MoveFlag::PromotionCapture
                || f == chess::MoveFlag::EnPassant) {
                if (board_.sideToMove() == chess::Color::White) {
                    if (!capturedByBlack_.empty()) capturedByBlack_.pop_back();
                } else {
                    if (!capturedByWhite_.empty()) capturedByWhite_.pop_back();
                }
            }
        };
        restoreCapture(last.move.flag);
        board_ = last.boardBefore;
        if (!positionHistory_.empty()) positionHistory_.pop_back();
        played_.pop_back();
    }
    selectedSquare_ = chess::kNoSquare;
    lastMove_ = played_.empty() ? std::optional<chess::Move>{} : std::optional<chess::Move>(played_.back().move);
    refreshLegalMoves();
    animator_.initFromBoard(board_);
    spdlog::info("Undo: {} plies", undoCount);
}

void Application::onClickAt(double mouseX, double mouseY) {
    if (state_ != ui::AppState::Playing) return;
    if (animator_.isAnimating()) return;
    if (pendingPromotion_) return;
    if (window_.imguiWantsMouse()) return;
    if (aiAgent_ && board_.sideToMove() == aiColor_) return;

    const SquarePick pick = pickSquare(mouseX, mouseY, window_.width(), window_.height(),
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
        if (!p.empty() && p.color == board_.sideToMove()) selectedSquare_ = sq;
        return;
    }

    // Procura jogada legal selected -> sq
    bool foundPromotion = false;
    chess::Move basePromotion{};
    bool promoIsCapture = false;
    chess::Move chosen{};
    bool found = false;
    for (const auto& mv : legalMoves_) {
        if (mv.from != selectedSquare_ || mv.to != sq) continue;
        if (mv.isPromotion()) {
            foundPromotion = true;
            basePromotion = mv;
            promoIsCapture = (mv.flag == chess::MoveFlag::PromotionCapture);
            // continua iterando — só precisamos saber que existe
        } else {
            chosen = mv;
            found = true;
            break;
        }
    }

    if (foundPromotion && !found) {
        // Mostra diálogo
        pendingPromotion_ = ui::PromotionRequest{basePromotion, promoIsCapture};
        return;
    }
    if (found) {
        applyMove(chosen);
        selectedSquare_ = chess::kNoSquare;
    } else if (!p.empty() && p.color == board_.sideToMove()) {
        selectedSquare_ = sq;
    } else {
        selectedSquare_ = chess::kNoSquare;
    }
}

void Application::maybeTriggerAi() {
    if (state_ != ui::AppState::Playing) return;
    if (!aiAgent_) return;
    if (result_ != chess::GameResult::Ongoing) return;
    if (animator_.isAnimating()) return;
    if (pendingPromotion_) return;
    if (board_.sideToMove() != aiColor_) {
        aiThinking_ = false;
        return;
    }

    // Defer 1 frame: marca "Pensando..." e retorna; no próximo frame de fato pensa.
    if (!aiThinking_) {
        aiThinking_ = true;
        return;
    }

    const chess::Move m = aiAgent_->chooseMove(board_);
    aiThinking_ = false;
    if (m.isNull()) {
        spdlog::warn("AI: chooseMove returned null move");
        return;
    }
    const auto info = aiAgent_->lastInfo();
    spdlog::info("AI: {} eval={} cp nodes={} time={}ms",
                 chess::moveToUci(m), info.evaluation, info.nodesVisited,
                 info.elapsed.count() / 1000.0);
    applyMove(m);
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

    if (const auto* board = gltf_.find(kBoardMeshName)) {
        litShader_.setMat4("uModel", normalize);
        litShader_.setMat3("uNormalMatrix", glm::mat3(glm::transpose(glm::inverse(normalize))));
        litShader_.setVec3("uAlbedo", glm::vec3(0.30f, 0.22f, 0.16f));
        litShader_.setFloat("uAlpha", 1.0f);
        board->mesh.draw();
    }

    bool blendEnabled = false;
    for (const auto& v : animator_.pieces()) {
        if (!v.alive) continue;
        const char* meshName = meshNameFor(v.type, v.color);
        if (!meshName) continue;
        const auto* item = gltf_.find(meshName);
        if (!item) continue;
        const glm::mat4 model =
            glm::translate(glm::mat4(1.0f), v.worldPos)
            * glm::rotate(glm::mat4(1.0f), glm::radians(v.yawDeg), glm::vec3(0,1,0))
            * glm::scale(glm::mat4(1.0f), glm::vec3(gltfWorldScale_ * v.scale));
        litShader_.setMat4("uModel", model);
        litShader_.setMat3("uNormalMatrix", glm::mat3(glm::transpose(glm::inverse(model))));
        litShader_.setVec3("uAlbedo", v.color == chess::Color::White ? kWhiteColor : kBlackColor);
        litShader_.setFloat("uAlpha", v.alpha);
        const bool needsBlend = v.alpha < 0.999f;
        if (needsBlend && !blendEnabled) {
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE); blendEnabled = true;
        } else if (!needsBlend && blendEnabled) {
            glDisable(GL_BLEND); glDepthMask(GL_TRUE); blendEnabled = false;
        }
        item->mesh.draw();
    }
    if (blendEnabled) { glDisable(GL_BLEND); glDepthMask(GL_TRUE); }
}

void Application::renderHighlights() {
    if (!highlightShader_.valid()) return;
    if (state_ != ui::AppState::Playing) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    highlightShader_.use();

    constexpr float kHoverY = 0.02f;
    auto drawSquare = [&](int file, int rank, const glm::vec4& color) {
        const glm::mat4 model =
            glm::translate(glm::mat4(1.0f), squareToWorld(file, rank) + glm::vec3(0.0f, kHoverY, 0.0f))
            * glm::scale(glm::mat4(1.0f), glm::vec3(kSquareSize, 1.0f, kSquareSize));
        highlightShader_.setMat4("uModel", model);
        highlightShader_.setVec4("uColor", color);
        highlightQuad_.draw();
    };

    if (lastMove_) {
        const glm::vec4 yellow(0.95f, 0.85f, 0.30f, 0.35f);
        drawSquare(chess::fileOf(lastMove_->from), chess::rankOf(lastMove_->from), yellow);
        drawSquare(chess::fileOf(lastMove_->to),   chess::rankOf(lastMove_->to),   yellow);
    }
    if (selectedSquare_ != chess::kNoSquare) {
        drawSquare(chess::fileOf(selectedSquare_), chess::rankOf(selectedSquare_),
                   glm::vec4(0.30f, 0.65f, 0.95f, 0.45f));
        for (const auto& mv : legalMoves_) {
            if (mv.from != selectedSquare_) continue;
            const bool capture = mv.isCapture();
            const glm::vec4 col = capture
                ? glm::vec4(0.95f, 0.30f, 0.30f, 0.50f)
                : glm::vec4(0.30f, 0.85f, 0.40f, 0.45f);
            drawSquare(chess::fileOf(mv.to), chess::rankOf(mv.to), col);
        }
    }
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

void Application::rebuildHudData() {
    hud_.sideToMove = board_.sideToMove();
    hud_.inCheck = board_.inCheck(board_.sideToMove());
    hud_.aiThinking = aiThinking_;
    hud_.fullmove = (static_cast<int>(played_.size()) / 2) + 1;
    hud_.sanHistory.clear();
    hud_.sanHistory.reserve(played_.size());
    for (const auto& p : played_) hud_.sanHistory.push_back(p.san);
    hud_.capturedByWhite = capturedByWhite_;
    hud_.capturedByBlack = capturedByBlack_;
}

void Application::renderUi() {
    window_.beginImGuiFrame();
    switch (state_) {
        case ui::AppState::MainMenu:
            gameUi_.renderMainMenu();
            break;
        case ui::AppState::Playing:
            rebuildHudData();
            gameUi_.renderHud(hud_);
            if (pendingPromotion_) {
                gameUi_.renderPromotionDialog(*pendingPromotion_);
            }
            break;
        case ui::AppState::GameOver:
            rebuildHudData();
            gameUi_.renderHud(hud_);
            gameUi_.renderEndGame(result_, static_cast<int>(played_.size()));
            break;
    }
    window_.endImGuiFrame();
}

int Application::run() {
    if (!window_.ok()) return 1;
    spdlog::info("Chess3D — main loop iniciado");
    lastFrameTime_ = static_cast<float>(glfwGetTime());

    while (!window_.shouldClose()) {
        window_.pollEvents();
        if (glfwGetKey(window_.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            // ESC só fecha a partir do menu; em jogo, volta pro menu
            if (state_ == ui::AppState::MainMenu) {
                window_.setShouldClose(true);
            } else {
                backToMenu();
            }
        }

        const float now = static_cast<float>(glfwGetTime());
        const float dt = std::min(now - lastFrameTime_, 0.1f);
        lastFrameTime_ = now;
        animator_.update(dt);

        if (state_ == ui::AppState::Playing) {
            maybeTriggerAi();
        }

        glClearColor(0.07f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderScene(window_.aspect());
        renderUi();
        window_.swap();
    }
    return 0;
}

}  // namespace chess3d
