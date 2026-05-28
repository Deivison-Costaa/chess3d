#include "Application.h"

#include "ai/EngineCatalog.h"
#include "ai/MinimaxAgent.h"
#include "chess/Move.h"
#include "chess/Notation.h"
#include "core/BoardCoords.h"
#include "platform/AssetPaths.h"
#include "render/Picker.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

// stb_image: implementação fica em GltfLoader.cpp; aqui só precisamos das declarações.
#include <stb_image.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <thread>

namespace chess3d {

namespace {

constexpr GLuint kCameraBlockBinding = 0;

struct CameraBlockData {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::vec4 cameraPos{0.0f};
};

std::filesystem::path assetPath(const char* relative) {
    return platform::assetPath(relative);
}

// Asset novo (Chess.glb / generator Khronos glTF Blender I/O) traz a cena
// completa: tabuleiro dividido em 4 partes + ambientação (mesa, tapete, relógio).
constexpr const char* kBoardMesh       = "Chess_Board";
constexpr const char* kBoardFrameMesh  = "Chess_Board_Frame";
constexpr const char* kBoardBorderMesh = "Chess_Board_Border";
constexpr const char* kBoardLabelsMesh = "Chess_Board_Labels";
constexpr const char* kTableMesh       = "Table";
constexpr const char* kCarpetMesh      = "Carpet";
constexpr const char* kClockMesh       = "Chess_Clock";

const char* meshNameFor(chess::PieceType type, chess::Color color) {
    using namespace chess;
    const bool w = (color == Color::White);
    switch (type) {
        case PieceType::Pawn:   return w ? "Pawn_White_01"   : "Pawn_Black_01";
        case PieceType::Rook:   return w ? "Rook_White_01"   : "Rook_Black_01";
        case PieceType::Knight: return w ? "Knight_White_01" : "Knight_Black_01";
        case PieceType::Bishop: return w ? "Bishop_White_01" : "Bishop_Black_01";
        case PieceType::Queen:  return w ? "Queen_White"     : "Queen_Black";
        case PieceType::King:   return w ? "King_White"      : "King_Black";
        default: return nullptr;
    }
}

const glm::vec3 kWhiteColor(0.95f, 0.92f, 0.85f);  // marfim quente — compensa "White_Plastic" 0.654
const glm::vec3 kBlackColor(0.06f, 0.06f, 0.07f);

GLuint loadGLTexture(const std::filesystem::path& path) {
    int w = 0, h = 0, ch = 0;
    stbi_uc* data = stbi_load(path.string().c_str(), &w, &h, &ch, 4);
    if (!data) {
        spdlog::warn("texture load fail: {} ({})", path.string(), stbi_failure_reason());
        return 0;
    }
    GLuint tex = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    const GLsizei levels = static_cast<GLsizei>(std::floor(std::log2(std::max(w, h))) + 1);
    glTextureStorage2D(tex, levels, GL_RGBA8, w, h);
    glTextureSubImage2D(tex, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateTextureMipmap(tex);
    glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GLfloat maxAniso = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    glTextureParameterf(tex, GL_TEXTURE_MAX_ANISOTROPY, std::min(16.0f, maxAniso));
    stbi_image_free(data);
    spdlog::info("  ext texture {}x{} ch={} -> id={} ({})", w, h, ch, tex, path.filename().string());
    return tex;
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
    camera_.setHomeView(glm::vec3(0.0f), 14.0f, glm::radians(35.0f), glm::radians(40.0f));

    input_.attach(window_.handle(), &camera_);
    input_.setOnLeftClick([this](double x, double y) { onClickAt(x, y); });
    input_.setOnGameKey([this](int key) {
        if (key == GLFW_KEY_F3) { gameUi_.toggleDebug(); return; }
        if (key == GLFW_KEY_T)  {
            usePbrTextures_ = !usePbrTextures_;
            spdlog::info("PBR textures: {}", usePbrTextures_ ? "ON" : "OFF");
            return;
        }
        if (key == GLFW_KEY_F4) {
            debugViewMode_ = (debugViewMode_ + 1) % 6;
            const char* names[] = {"lit", "albedo", "normal", "roughness", "metallic", "uv"};
            spdlog::info("Debug view: {} ({})", debugViewMode_, names[debugViewMode_]);
            return;
        }
        if (key == GLFW_KEY_G) {
            applyGamma_ = !applyGamma_;
            spdlog::info("Degamma (sRGB→linear) on albedo: {}", applyGamma_ ? "ON" : "OFF");
            return;
        }
        if (key == GLFW_KEY_L) {
            useNormalMap_ = !useNormalMap_;
            spdlog::info("Normal mapping: {}", useNormalMap_ ? "ON" : "OFF");
            return;
        }
        if (key == GLFW_KEY_P) {
            int fbW = window_.width(), fbH = window_.height();
            int wW  = window_.windowWidth(), wH = window_.windowHeight();
            double cx = 0, cy = 0;
            glfwGetCursorPos(window_.handle(), &cx, &cy);
            spdlog::info("DIAG fb=({}x{}) win=({}x{}) cursor=({:.1f},{:.1f}) fb/win=({:.3f},{:.3f})",
                         fbW, fbH, wW, wH, cx, cy,
                         (wW > 0 ? float(fbW)/wW : 0.0f), (wH > 0 ? float(fbH)/wH : 0.0f));
            return;
        }
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
        // Chess_Board é só a grade 8×8. O asset usa nodeTransform com scale embutida
        // (~4.86 nesse asset), então o bbox LOCAL não reflete o tamanho rendered.
        // Computa bbox mundial transformando os cantos via nodeTransform.
        if (const auto* board = gltf_.find(kBoardMesh)) {
            const glm::vec3 lMin = board->bboxMin;
            const glm::vec3 lMax = board->bboxMax;
            const glm::vec3 corners[8] = {
                {lMin.x, lMin.y, lMin.z}, {lMax.x, lMin.y, lMin.z},
                {lMin.x, lMax.y, lMin.z}, {lMax.x, lMax.y, lMin.z},
                {lMin.x, lMin.y, lMax.z}, {lMax.x, lMin.y, lMax.z},
                {lMin.x, lMax.y, lMax.z}, {lMax.x, lMax.y, lMax.z},
            };
            glm::vec3 wMin( std::numeric_limits<float>::max());
            glm::vec3 wMax(-std::numeric_limits<float>::max());
            for (const auto& c : corners) {
                const glm::vec3 w = glm::vec3(board->nodeTransform * glm::vec4(c, 1.0f));
                wMin = glm::min(wMin, w);
                wMax = glm::max(wMax, w);
            }
            const glm::vec3 size = wMax - wMin;
            const float gridWidth = std::max(size.x, size.z);
            if (gridWidth > 0.0f) gltfWorldScale_ = (8.0f * kSquareSize) / gridWidth;
            gltfWorldOffset_ = glm::vec3(-0.5f * (wMin.x + wMax.x),
                                         -wMax.y,
                                         -0.5f * (wMin.z + wMax.z));
            spdlog::info("Board world bbox: ({:.2f},{:.2f},{:.2f})..({:.2f},{:.2f},{:.2f}), world scale={:.4f}",
                         wMin.x, wMin.y, wMin.z, wMax.x, wMax.y, wMax.z, gltfWorldScale_);
        } else {
            spdlog::warn("Board mesh '{}' não encontrado no .glb", kBoardMesh);
        }

        // Constrói GL textures a partir das imagens do .glb (RGBA8 ou RGB8).
        imageTextures_.reserve(gltf_.images().size());
        for (const auto& img : gltf_.images()) {
            if (img.pixels.empty() || img.width <= 0 || img.height <= 0) {
                imageTextures_.push_back(0);
                continue;
            }
            GLuint tex = 0;
            glCreateTextures(GL_TEXTURE_2D, 1, &tex);
            const GLsizei levels = static_cast<GLsizei>(
                std::floor(std::log2(std::max(img.width, img.height))) + 1);
            glTextureStorage2D(tex, levels, GL_RGBA8, img.width, img.height);
            const GLenum format = (img.channels == 3) ? GL_RGB : GL_RGBA;
            glTextureSubImage2D(tex, 0, 0, 0, img.width, img.height, format,
                                GL_UNSIGNED_BYTE, img.pixels.data());
            glGenerateTextureMipmap(tex);
            glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_REPEAT);
            // LOD bias 0 — deixa GL escolher mipmap natural; o ruído anterior vinha
            // do normal map (agora off por default), não da escolha de mip.
            // Anisotropia máxima (16x) — preserva nitidez em ângulos rasantes.
            GLfloat maxAniso = 1.0f;
            glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
            glTextureParameterf(tex, GL_TEXTURE_MAX_ANISOTROPY, std::min(16.0f, maxAniso));
            imageTextures_.push_back(tex);
            spdlog::debug("gl texture {}x{} ch={} -> id={}", img.width, img.height, img.channels, tex);
        }

        // Texturas PBR externas (ambientCG): substituem cores planas do asset
        // por mármore/madeira reais. Ordem dos sets em externalSets_:
        //   0 = white piece, 1 = black piece, 2 = wood (frame + table)
        auto makeSet = [&](const char* base) {
            ExternalTextureSet s;
            const std::string b = std::string("textures/") + base;
            s.diff  = loadGLTexture(assetPath((b + "_diff.jpg").c_str()));
            s.nor   = loadGLTexture(assetPath((b + "_nor.jpg").c_str()));
            s.rough = loadGLTexture(assetPath((b + "_rough.jpg").c_str()));
            return s;
        };
        externalSets_.push_back(makeSet("white_piece"));
        externalSets_.push_back(makeSet("black_piece"));
        externalSets_.push_back(makeSet("wood"));

        // Mapeia material name → set externo.
        materialOverride_.assign(gltf_.materials().size(), -1);
        for (std::size_t i = 0; i < gltf_.materials().size(); ++i) {
            const std::string& n = gltf_.materials()[i].name;
            if      (n == "White_Plastic") materialOverride_[i] = 0;
            else if (n == "Black_Plastic") materialOverride_[i] = 1;
            else if (n == "Wood")          materialOverride_[i] = 2;
            else if (n == "Dark_Wood")     materialOverride_[i] = 2;
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
        sendMoveToPeer(m);  // promoção tem caminho separado de clique
    });
    gameUi_.setNewGame([this]() { startNewGame(gameUi_.setup()); });
    gameUi_.setBackToMenu([this]() { backToMenu(); });
    gameUi_.setUndo([this]() { undoLastTurn(); });
    gameUi_.setPause([this](bool p) { paused_ = p; });
    gameUi_.setSpeed([this](float s) { speedMultiplier_ = s; });
    gameUi_.setCancelLobby([this]() { closeLanConnection(); state_ = ui::AppState::MainMenu; });
    gameUi_.setEngineCatalog(ai::EngineCatalog::detect());

    // Inicializa cena vazia (Menu) — Animator com tabuleiro padrão pra ficar bonito de fundo
    board_.reset();
    animator_.initFromBoard(board_);
}

Application::~Application() {
    // Cancela RemoteAgent antes de esperar o future (senão deadlock: future espera
    // o peer que nunca vai responder porque a conexão vai fechar).
    if (remoteAgent_) remoteAgent_->cancel();
    if (aiFuture_.valid()) aiFuture_.wait();
    aiInFlight_.reset();
    closeLanConnection();
    input_.detach();
    if (cameraUbo_) {
        glDeleteBuffers(1, &cameraUbo_);
        cameraUbo_ = 0;
    }
    for (GLuint t : imageTextures_) {
        if (t) glDeleteTextures(1, &t);
    }
    imageTextures_.clear();
    for (const auto& s : externalSets_) {
        if (s.diff)  glDeleteTextures(1, &s.diff);
        if (s.nor)   glDeleteTextures(1, &s.nor);
        if (s.rough) glDeleteTextures(1, &s.rough);
    }
    externalSets_.clear();
}

void Application::startNewGame(const ui::GameSetup& setup) {
    // Cancela e limpa estado LAN anterior antes de começar.
    if (remoteAgent_) remoteAgent_->cancel();
    if (aiFuture_.valid()) { aiFuture_.wait(); aiFuture_ = {}; }
    aiInFlight_.reset();
    aiThinking_ = false;
    closeLanConnection();

    humanColor_ = setup.humanColor;
    aiVsAi_ = (setup.mode == ui::GameMode::AiVsAi);
    lanMode_ = (setup.mode == ui::GameMode::LanHost || setup.mode == ui::GameMode::LanClient);
    isLanHost_ = (setup.mode == ui::GameMode::LanHost);
    paused_ = false;
    speedMultiplier_ = 1.0f;
    handshakeDone_ = false;
    remoteAgent_.reset();
    whiteTimeMs_ = -1;
    blackTimeMs_ = -1;
    incrementMs_ = 0;
    myNick_ = setup.lanNick;

    const bool isHotseat = (setup.mode == ui::GameMode::Hotseat);

    // ── Modos sem rede ──────────────────────────────────────────────────────
    if (!lanMode_) {
        if (aiVsAi_) {
            whiteAgent_ = ai::makeAgent(setup.whiteAgent);
            blackAgent_ = ai::makeAgent(setup.blackAgent);
        } else if (isHotseat) {
            whiteAgent_.reset();
            blackAgent_.reset();
        } else {
            if (humanColor_ == chess::Color::White) {
                whiteAgent_.reset();
                blackAgent_ = ai::makeAgent(setup.blackAgent);
            } else {
                blackAgent_.reset();
                whiteAgent_ = ai::makeAgent(setup.whiteAgent);
            }
        }

        board_.reset();
        positionHistory_.clear();
        positionHistory_.push_back(chess::positionKey(board_));
        played_.clear();
        capturedByWhite_.clear();
        capturedByBlack_.clear();
        selectedSquare_ = chess::kNoSquare;
        lastMove_.reset();
        pendingPromotion_.reset();
        refreshLegalMoves();
        animator_.initFromBoard(board_);

        if (setup.timeControl.initialMs > 0) {
            whiteTimeMs_ = setup.timeControl.initialMs;
            blackTimeMs_ = setup.timeControl.initialMs;
            incrementMs_ = setup.timeControl.incrementMs;
        }

        if (aiVsAi_) {
            camera_.setHomeView(glm::vec3(0.0f), 14.0f,
                                 glm::radians(45.0f), glm::radians(55.0f));
        } else {
            const bool whiteSide = (humanColor_ == chess::Color::White);
            camera_.setHomeView(glm::vec3(0.0f), 14.0f,
                                 whiteSide ? glm::pi<float>() : 0.0f,
                                 glm::radians(40.0f));
        }
        state_ = ui::AppState::Playing;
        if (aiVsAi_) {
            spdlog::info("Nova partida IA vs IA: branco={}, preto={}",
                         whiteAgent_->name(), blackAgent_->name());
        } else {
            spdlog::info("Nova partida: modo={}, humanColor={}",
                         isHotseat ? "hotseat" : "HvAI",
                         (humanColor_ == chess::Color::White) ? "white" : "black");
        }
        return;
    }

    // ── Modos LAN ───────────────────────────────────────────────────────────
    connection_ = std::make_unique<net::LanConnection>();

    if (isLanHost_) {
        humanColor_ = setup.humanColor;
        remoteColor_ = (humanColor_ == chess::Color::White) ? chess::Color::Black
                                                            : chess::Color::White;
        if (!connection_->startListening(static_cast<uint16_t>(setup.lanPort))) {
            spdlog::error("startNewGame: falha ao abrir porta {}", setup.lanPort);
            connection_.reset();
            lanMode_ = false;
            return;
        }
        lobbyData_.localIp = connection_->localIp();
        lobbyData_.port    = setup.lanPort;
        state_ = ui::AppState::Lobby;
        spdlog::info("LAN host: aguardando em {}:{}", lobbyData_.localIp, lobbyData_.port);
    } else {
        // Cliente: conecta em background para não travar a UI.
        humanColor_ = chess::Color::White;  // será sobrescrito pelo ROLE do host
        lobbyData_.localIp = "...";
        lobbyData_.port    = setup.lanPort;
        state_ = ui::AppState::Lobby;
        const std::string host = setup.lanHost;
        const int port = setup.lanPort;
        connectThread_ = std::thread([this, host, port]() {
            if (!connection_ || !connection_->connectTo(host, port)) {
                spdlog::error("LAN client: falha ao conectar em {}:{}", host, port);
                // Marca como erro — handleLobbyFrame vai perceber isConnected()==false
            }
        });
        spdlog::info("LAN client: conectando a {}:{}...", host, port);
    }
}

void Application::backToMenu() {
    if (remoteAgent_) remoteAgent_->cancel();
    if (aiFuture_.valid()) { aiFuture_.wait(); aiFuture_ = {}; }
    aiInFlight_.reset();
    aiThinking_ = false;
    closeLanConnection();
    lanMode_ = false;
    state_ = ui::AppState::MainMenu;
    selectedSquare_ = chess::kNoSquare;
    pendingPromotion_.reset();
}

void Application::setDifficulty(ai::Difficulty d) {
    // Atalho de teclado 7/8/9: só faz sentido em HumanVsAi.
    // Troca o engine do lado da IA pra Minimax na dificuldade escolhida.
    if (aiVsAi_) return;
    if (aiFuture_.valid()) return;  // evita race condition: aguardar o lance terminar
    ai::AgentSpec spec;
    switch (d) {
        case ai::Difficulty::Easy:   spec.engine = ai::AgentSpec::Engine::MinimaxEasy;   break;
        case ai::Difficulty::Medium: spec.engine = ai::AgentSpec::Engine::MinimaxMedium; break;
        case ai::Difficulty::Hard:   spec.engine = ai::AgentSpec::Engine::MinimaxHard;   break;
        case ai::Difficulty::Master: spec.engine = ai::AgentSpec::Engine::Stockfish;     break;
    }
    auto a = ai::makeAgent(spec);
    if (humanColor_ == chess::Color::White) {
        gameUi_.setup().blackAgent = spec;
        blackAgent_ = std::move(a);
    } else {
        gameUi_.setup().whiteAgent = spec;
        whiteAgent_ = std::move(a);
    }
    ai::Agent* current = (humanColor_ == chess::Color::White) ? blackAgent_.get() : whiteAgent_.get();
    spdlog::info("Dificuldade trocada: {}", current ? current->name() : "?");
}

void Application::refreshLegalMoves() {
    legalMoves_.clear();
    chess::generateLegalMoves(board_, legalMoves_);
    const auto prevResult = result_;
    result_ = chess::evaluateGame(board_, positionHistory_);
    if (result_ != chess::GameResult::Ongoing && prevResult == chess::GameResult::Ongoing) {
        spdlog::info("Fim de jogo: {}", chess::gameResultName(result_));
        state_ = ui::AppState::GameOver;
        // Anima a queda do rei perdedor se for mate
        if (result_ == chess::GameResult::WhiteWins) {
            animator_.animateKingFall(chess::Color::Black);
        } else if (result_ == chess::GameResult::BlackWins) {
            animator_.animateKingFall(chess::Color::White);
        }
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

    // Increment Fischer pro lado que acabou de jogar (em modos temporizados).
    if (incrementMs_ > 0) {
        if (boardBefore.sideToMove() == chess::Color::White) whiteTimeMs_ += incrementMs_;
        else                                                 blackTimeMs_ += incrementMs_;
    }

    refreshLegalMoves();
    spdlog::info("{}{} {}",
                 (boardBefore.sideToMove() == chess::Color::White) ? "" : "  ",
                 san, chess::moveToUci(m));
}

void Application::undoLastTurn() {
    if (state_ != ui::AppState::Playing) return;
    if (lanMode_) return;  // sem undo em LAN (exigiria handshake com o oponente)
    if (animator_.isAnimating()) return;
    if (aiFuture_.valid()) return;  // não desfazer enquanto IA está pensando
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
    // Em AI vs AI ambos os lados são IA → humano não clica nunca.
    if (aiVsAi_) return;
    // O lado a mover é IA?
    const auto* curAgent = (board_.sideToMove() == chess::Color::White)
                         ? whiteAgent_.get() : blackAgent_.get();
    if (curAgent) return;

    // Constrói hitboxes cilíndricas a partir das peças visíveis (Animator) —
    // resolve picking impreciso em peças altas (raio que iria cair "atras" no plano).
    std::vector<PieceHitbox> hitboxes;
    hitboxes.reserve(animator_.pieces().size());
    for (const auto& v : animator_.pieces()) {
        if (!v.alive || v.alpha < 0.5f) continue;
        const int file = chess::fileOf(v.square);
        const int rank = chess::rankOf(v.square);
        if (file < 0 || file >= 8 || rank < 0 || rank >= 8) continue;
        // Altura aproximada por tipo (em coords mundo) — bbox * gltfWorldScale_.
        float h = 0.85f;  // pawn default
        switch (v.type) {
            case chess::PieceType::Pawn:   h = 0.85f; break;
            case chess::PieceType::Rook:   h = 0.95f; break;
            case chess::PieceType::Knight: h = 1.15f; break;
            case chess::PieceType::Bishop: h = 1.20f; break;
            case chess::PieceType::Queen:  h = 1.30f; break;
            case chess::PieceType::King:   h = 1.50f; break;
            default: break;
        }
        hitboxes.push_back({file, rank,
                            squareToWorld(file, rank),
                            0.35f * kSquareSize,
                            h});
    }

    // Cursor de glfwGetCursorPos vem em screen coords (logicas); usar windowSize
    // (não framebuffer) pra calcular NDC corretamente em telas com DPI scaling.
    // Para consistência: NDC do cursor usa windowSize, então projection do picker
    // tambem usa windowAspect (em algumas configs Windows com DPI scaling,
    // framebuffer e window diferem mais que apenas em escala).
    const float winAspect = (window_.windowHeight() > 0)
        ? static_cast<float>(window_.windowWidth()) / window_.windowHeight()
        : window_.aspect();
    const SquarePick pick = pickSquareWithPieces(mouseX, mouseY,
                                                 window_.windowWidth(), window_.windowHeight(),
                                                 camera_.viewMatrix(),
                                                 camera_.projectionMatrix(winAspect),
                                                 hitboxes,
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
        sendMoveToPeer(chosen);
        selectedSquare_ = chess::kNoSquare;
    } else if (!p.empty() && p.color == board_.sideToMove()) {
        selectedSquare_ = sq;
    } else {
        selectedSquare_ = chess::kNoSquare;
    }
}

void Application::maybeTriggerAi() {
    if (state_ != ui::AppState::Playing) return;
    if (paused_) return;
    if (result_ != chess::GameResult::Ongoing) return;
    if (animator_.isAnimating()) return;
    if (pendingPromotion_) return;

    const chess::Color side = board_.sideToMove();
    auto& agentPtr = (side == chess::Color::White) ? whiteAgent_ : blackAgent_;
    if (!agentPtr) {  // este lado é humano
        aiThinking_ = false;
        return;
    }

    // ── Coleta resultado se o future terminou ─────────────────────────────────
    if (aiFuture_.valid()) {
        if (aiFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return;  // ainda pensando; main thread continua livre
        }
        const chess::Move m = aiFuture_.get();
        const auto aiElapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - aiStart_).count();

        const auto info = aiInFlight_->lastInfo();
        const std::string aName = aiInFlight_->name();
        aiInFlight_.reset();
        aiThinking_ = false;

        if (whiteTimeMs_ >= 0) {
            int& clock = (side == chess::Color::White) ? whiteTimeMs_ : blackTimeMs_;
            clock = std::max(0, clock - static_cast<int>(aiElapsedMs));
            if (clock == 0 && result_ == chess::GameResult::Ongoing) {
                result_ = (side == chess::Color::White)
                    ? chess::GameResult::BlackWinsOnTime
                    : chess::GameResult::WhiteWinsOnTime;
                state_ = ui::AppState::GameOver;
                spdlog::info("Fim de jogo: {}", chess::gameResultName(result_));
                animator_.animateKingFall(side);
                return;
            }
        }
        lastFrameTime_ = static_cast<float>(glfwGetTime());
        if (m.isNull()) {
            spdlog::warn("AI ({}): chooseMove retornou null move", aName);
            return;
        }
        spdlog::info("AI ({}): {} eval={} cp nodes={} time={}ms (real={}ms)",
                     aName, chess::moveToUci(m), info.evaluation, info.nodesVisited,
                     info.elapsed.count() / 1000.0, aiElapsedMs);
        applyMove(m);
        return;
    }

    // ── Lança cálculo em thread separada ─────────────────────────────────────
    aiInFlight_ = agentPtr;
    aiThinking_ = true;
    aiStart_ = std::chrono::steady_clock::now();
    chess::Board snapshot = board_;  // cópia barata (mailbox 8×8)
    aiFuture_ = std::async(std::launch::async,
        [agent = aiInFlight_, snapshot]() mutable -> chess::Move {
            return agent->chooseMove(snapshot);
        });
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
    litShader_.setVec3("uLightDir",       glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f)));
    litShader_.setVec3("uLightColor",     glm::vec3(1.30f, 1.20f, 1.08f));
    litShader_.setVec3("uFillLightDir",   glm::normalize(glm::vec3( 0.6f, -0.4f,  0.7f)));
    litShader_.setVec3("uFillLightColor", glm::vec3(0.50f, 0.55f, 0.65f));
    litShader_.setVec3("uRimLightDir",    glm::normalize(glm::vec3( 0.0f, -0.2f, -1.0f)));
    litShader_.setVec3("uRimLightColor",  glm::vec3(0.30f, 0.34f, 0.40f));
    litShader_.setVec3("uAmbientSky",     glm::vec3(0.55f, 0.58f, 0.68f));
    litShader_.setVec3("uAmbientGround",  glm::vec3(0.20f, 0.18f, 0.14f));
    litShader_.setFloat("uAmbientWeight", 0.65f);
    litShader_.setFloat("uNormalIntensity", useNormalMap_ ? 1.0f : 0.0f);
    litShader_.setInt("uDebugMode", debugViewMode_);
    litShader_.setInt("uApplyGamma", applyGamma_ ? 1 : 0);

    auto bindMaterial = [&](int matIdx, const glm::vec3& fallback) {
        GLuint baseTex = 0, mrTex = 0, normalTex = 0;
        glm::vec4 baseColorFactor(1.0f);
        float metallic = 0.0f;
        float roughness = 0.7f;
        // Primeiro: tem override externo (mármore/madeira de verdade)?
        const int ov = (usePbrTextures_
                        && matIdx >= 0
                        && matIdx < static_cast<int>(materialOverride_.size()))
                       ? materialOverride_[matIdx] : -1;
        if (ov >= 0 && ov < static_cast<int>(externalSets_.size())) {
            const auto& s = externalSets_[ov];
            baseTex   = s.diff;
            normalTex = s.nor;
            // mrTex desligado por design (anti specular aliasing): a roughness
            // varia muito por pixel em mármore, e Blinn-Phong com shininess
            // alto produz sparkle. Roughness constante via uRoughnessFactor.
            mrTex     = 0;
            baseColorFactor = glm::vec4(1.0f);
            metallic  = 0.0f;
            roughness = 1.0f;
        } else if (usePbrTextures_
                   && matIdx >= 0 && matIdx < static_cast<int>(gltf_.materials().size())) {
            const auto& m = gltf_.materials()[matIdx];
            auto idxToTex = [&](int idx) -> GLuint {
                if (idx < 0 || idx >= static_cast<int>(imageTextures_.size())) return 0;
                return imageTextures_[idx];
            };
            baseTex   = idxToTex(m.baseColorImage);
            mrTex     = 0;  // mesma razão acima
            normalTex = idxToTex(m.normalImage);
            baseColorFactor = m.baseColorFactor;
            metallic  = m.metallicFactor;
            roughness = m.roughnessFactor;
        }
        glBindTextureUnit(0, baseTex);
        glBindTextureUnit(1, mrTex);
        glBindTextureUnit(2, normalTex);
        litShader_.setInt("uUseBaseColorMap", baseTex   ? 1 : 0);
        litShader_.setInt("uUseMrMap",        mrTex     ? 1 : 0);
        litShader_.setInt("uUseNormalMap",    normalTex ? 1 : 0);
        litShader_.setVec3("uAlbedo", baseTex ? glm::vec3(baseColorFactor) : fallback);
        litShader_.setFloat("uMetallicFactor",  metallic);
        litShader_.setFloat("uRoughnessFactor", roughness);
    };

    const glm::mat4 normalize =
        glm::scale(glm::mat4(1.0f), glm::vec3(gltfWorldScale_))
        * glm::translate(glm::mat4(1.0f), gltfWorldOffset_);

    // Cena estática: tabuleiro (grade + moldura + borda + labels) + ambientação
    // (mesa/tapete/relógio). Cada mesh tem nodeTransform próprio (ex: carpet em
    // y=-12.6 no asset space). Aplicamos: normalize * nodeTransform * vertex.
    litShader_.setFloat("uAlpha", 1.0f);
    const char* sceneMeshes[] = {
        kBoardMesh, kBoardFrameMesh, kBoardBorderMesh, kBoardLabelsMesh,
        kTableMesh, kCarpetMesh,
        // kClockMesh removido — agora temos relógio funcional no HUD.
    };
    for (const char* name : sceneMeshes) {
        const auto* item = gltf_.find(name);
        if (!item) continue;
        const glm::mat4 model = normalize * item->nodeTransform;
        litShader_.setMat4("uModel", model);
        litShader_.setMat3("uNormalMatrix", glm::mat3(glm::transpose(glm::inverse(model))));
        for (const auto& sub : item->submeshes) {
            bindMaterial(sub.materialIndex, glm::vec3(0.30f, 0.22f, 0.16f));
            sub.mesh.draw();
        }
    }

    bool blendEnabled = false;
    bool cwFront = false;  // estado do glFrontFace pra peças mirrored (scale negativo)
    for (const auto& v : animator_.pieces()) {
        if (!v.alive) continue;
        const char* meshName = meshNameFor(v.type, v.color);
        if (!meshName) continue;
        const auto* item = gltf_.find(meshName);
        if (!item) continue;
        // Aplica rotação+escala do nodeTransform do artista (uniforme no Blender),
        // mas zera a translação — queremos posicionar a peça NA NOSSA grade.
        glm::mat4 pieceRS = item->nodeTransform;
        pieceRS[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        // Asset tem peças brancas com scale uniforme negativo (mirror do preto).
        // Inverte o frontFace pra não rendar inside-out com backface culling on.
        const bool needCw = (glm::determinant(glm::mat3(pieceRS)) < 0.0f);
        if (needCw != cwFront) {
            glFrontFace(needCw ? GL_CW : GL_CCW);
            cwFront = needCw;
        }
        // Asset orienta peças com "files" no eixo X (white em -X, black em +X);
        // nosso mundo tem files no eixo X mas ranks no Z → rotação 90° em Y alinha.
        // O cavalo preto (Plane.020) foi modelado com o focinho 180° em relação aos
        // outros tipos (artista exportou de um lado da mesa), então leva 180° extra.
        float extraYawDeg = 0.0f;
        if (v.type == chess::PieceType::Knight && v.color == chess::Color::Black) {
            extraYawDeg = 180.0f;
        }
        const glm::mat4 axisFix =
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f + extraYawDeg),
                        glm::vec3(0, 1, 0));
        const glm::mat4 model =
            glm::translate(glm::mat4(1.0f), v.worldPos)
            * glm::rotate(glm::mat4(1.0f), glm::radians(v.yawDeg),   glm::vec3(0,1,0))
            * glm::rotate(glm::mat4(1.0f), glm::radians(v.pitchDeg), glm::vec3(1,0,0))
            * glm::scale(glm::mat4(1.0f), glm::vec3(gltfWorldScale_ * v.scale))
            * axisFix
            * pieceRS;
        litShader_.setMat4("uModel", model);
        litShader_.setMat3("uNormalMatrix", glm::mat3(glm::transpose(glm::inverse(model))));
        litShader_.setFloat("uAlpha", v.alpha);
        const bool needsBlend = v.alpha < 0.999f;
        if (needsBlend && !blendEnabled) {
            glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE); blendEnabled = true;
        } else if (!needsBlend && blendEnabled) {
            glDisable(GL_BLEND); glDepthMask(GL_TRUE); blendEnabled = false;
        }
        const glm::vec3 fallback = v.color == chess::Color::White ? kWhiteColor : kBlackColor;
        // Peças com UVs degeneradas (artist's fault: queen/knight/pawn) não conseguem
        // amostrar a textura. Forçamos fallback de cor uniforme nelas.
        const int matToUse = item->hasValidUvs ? -2 : -1;  // -2 = "use mat normal", -1 = força fallback
        for (const auto& sub : item->submeshes) {
            const int realMat = (matToUse == -1) ? -1 : sub.materialIndex;
            bindMaterial(realMat, fallback);
            sub.mesh.draw();
        }
    }
    if (blendEnabled) { glDisable(GL_BLEND); glDepthMask(GL_TRUE); }
    if (cwFront) glFrontFace(GL_CCW);  // restaura default
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

    // Hover indicator: não necessário em AI vs AI (nunca selecionável).
    if (!aiVsAi_ && !window_.imguiWantsMouse() && !animator_.isAnimating()) {
        double mx = 0.0, my = 0.0;
        glfwGetCursorPos(window_.handle(), &mx, &my);
        std::vector<PieceHitbox> hb;
        hb.reserve(animator_.pieces().size());
        for (const auto& v : animator_.pieces()) {
            if (!v.alive || v.alpha < 0.5f) continue;
            const int file = chess::fileOf(v.square);
            const int rank = chess::rankOf(v.square);
            if (file < 0 || file >= 8 || rank < 0 || rank >= 8) continue;
            float h = 0.85f;
            switch (v.type) {
                case chess::PieceType::Pawn:   h = 0.85f; break;
                case chess::PieceType::Rook:   h = 0.95f; break;
                case chess::PieceType::Knight: h = 1.15f; break;
                case chess::PieceType::Bishop: h = 1.20f; break;
                case chess::PieceType::Queen:  h = 1.30f; break;
                case chess::PieceType::King:   h = 1.50f; break;
                default: break;
            }
            hb.push_back({file, rank, squareToWorld(file, rank), 0.35f * kSquareSize, h});
        }
        const float winAspectH = (window_.windowHeight() > 0)
            ? static_cast<float>(window_.windowWidth()) / window_.windowHeight()
            : window_.aspect();
        const SquarePick hover = pickSquareWithPieces(
            mx, my, window_.windowWidth(), window_.windowHeight(),
            camera_.viewMatrix(),
            camera_.projectionMatrix(winAspectH),
            hb, 0.0f);
        if (hover.valid()
            && chess::makeSquare(hover.file, hover.rank) != selectedSquare_) {
            // Tom mais quente se a peça da casa for nossa e pode mover; senão neutro.
            const chess::Piece p = board_.pieceAt(chess::makeSquare(hover.file, hover.rank));
            // Selecionável quando: jogo em andamento, modo não é AI vs AI,
            // o lado a mover é humano (sem agent), peça é da cor do lado a mover.
            const auto* moverAgent2 = (board_.sideToMove() == chess::Color::White)
                                    ? whiteAgent_.get() : blackAgent_.get();
            const bool selectable = (state_ == ui::AppState::Playing) && !aiVsAi_
                                 && !moverAgent2
                                 && !p.empty() && p.color == board_.sideToMove();
            const glm::vec4 col = selectable
                ? glm::vec4(1.00f, 0.85f, 0.30f, 0.55f)   // amarelo vivo
                : glm::vec4(0.60f, 0.80f, 1.00f, 0.28f);  // azul claro neutro
            drawSquare(hover.file, hover.rank, col);
        }
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
    hud_.whiteTimeMs = whiteTimeMs_;
    hud_.blackTimeMs = blackTimeMs_;
    hud_.aiVsAi = aiVsAi_;
    hud_.paused = paused_;
    hud_.speedMultiplier = speedMultiplier_;
    hud_.whiteAgentName = whiteAgent_ ? whiteAgent_->name() : "Humano";
    hud_.blackAgentName = blackAgent_ ? blackAgent_->name() : "Humano";
    hud_.lanMode = lanMode_;
    hud_.opponentNick = opponentNick_;
}

void Application::renderUi() {
    window_.beginImGuiFrame();
    switch (state_) {
        case ui::AppState::MainMenu:
            gameUi_.renderMainMenu();
            break;
        case ui::AppState::Lobby:
            gameUi_.renderLobby(lobbyData_);
            break;
        case ui::AppState::Playing: {
            rebuildHudData();
            gameUi_.renderHud(hud_);
            // Debug panel mostra o último agente que pensou (lado a mover, ou último a jogar).
            ai::Agent* dbgAgent = (board_.sideToMove() == chess::Color::White)
                                ? whiteAgent_.get() : blackAgent_.get();
            if (!dbgAgent) dbgAgent = (board_.sideToMove() == chess::Color::White)
                                    ? blackAgent_.get() : whiteAgent_.get();
            if (dbgAgent) gameUi_.renderDebugPanel(dbgAgent->lastInfo(), dbgAgent->name());
            if (pendingPromotion_) {
                gameUi_.renderPromotionDialog(*pendingPromotion_);
            }
            break;
        }
        case ui::AppState::GameOver: {
            rebuildHudData();
            gameUi_.renderHud(hud_);
            // Mostra info do agente que jogou por último (quem deu mate/empate).
            ai::Agent* dbgAgent = nullptr;
            if (!played_.empty()) {
                const chess::Color lastSide = played_.back().boardBefore.sideToMove();
                dbgAgent = (lastSide == chess::Color::White) ? whiteAgent_.get() : blackAgent_.get();
                if (!dbgAgent)
                    dbgAgent = (lastSide == chess::Color::White) ? blackAgent_.get() : whiteAgent_.get();
            } else {
                dbgAgent = whiteAgent_ ? whiteAgent_.get() : blackAgent_.get();
            }
            if (dbgAgent) gameUi_.renderDebugPanel(dbgAgent->lastInfo(), dbgAgent->name());
            gameUi_.renderEndGame(result_, static_cast<int>(played_.size()));
            break;
        }
    }
    window_.endImGuiFrame();
}

int Application::run() {
    if (!window_.ok()) return 1;
    spdlog::info("Chess3D — main loop iniciado");
    lastFrameTime_ = static_cast<float>(glfwGetTime());

    while (!window_.shouldClose()) {
        const auto frameStart = std::chrono::steady_clock::now();
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
        const float realDt = std::min(now - lastFrameTime_, 0.1f);
        lastFrameTime_ = now;
        // Em IA vs IA, o slider de velocidade acelera/desacelera só a animação visual
        // — não afeta wall-clock do relógio nem do tempo de pensamento da IA.
        const float dt = aiVsAi_ ? realDt * speedMultiplier_ : realDt;
        animator_.update(dt);

        if (state_ == ui::AppState::Lobby) {
            handleLobbyFrame();
        }

        if (state_ == ui::AppState::Playing) {
            // Drena mensagens de rede antes da IA (para RemoteAgent + eventos de controle).
            if (lanMode_ && remoteAgent_) {
                auto ev = remoteAgent_->pollControlEvent();
                if (ev) handleControlEvent(*ev);
            }

            // Tick do relógio do lado que tá pra mover (apenas se modo temporizado).
            // Suspende durante animação, diálogo de promoção e pause.
            if (whiteTimeMs_ >= 0 && !animator_.isAnimating() && !pendingPromotion_ && !paused_) {
                const int dms = static_cast<int>(realDt * 1000.0f);
                int& clock = (board_.sideToMove() == chess::Color::White) ? whiteTimeMs_ : blackTimeMs_;
                clock = std::max(0, clock - dms);
                if (clock == 0 && result_ == chess::GameResult::Ongoing) {
                    result_ = (board_.sideToMove() == chess::Color::White)
                        ? chess::GameResult::BlackWinsOnTime
                        : chess::GameResult::WhiteWinsOnTime;
                    state_ = ui::AppState::GameOver;
                    spdlog::info("Fim de jogo: {}", chess::gameResultName(result_));
                    if (result_ == chess::GameResult::WhiteWinsOnTime) {
                        animator_.animateKingFall(chess::Color::Black);
                    } else {
                        animator_.animateKingFall(chess::Color::White);
                    }
                }
            }
            maybeTriggerAi();
        }

        glClearColor(0.07f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        renderScene(window_.aspect());
        renderUi();
        window_.swap();
        // Cap de ~60 FPS por software. Complementa vsync: se o driver ignorar
        // glfwSwapInterval (comum em fullscreen composto), evita loop quente.
        std::this_thread::sleep_until(frameStart + std::chrono::microseconds(16667));
    }
    return 0;
}

// ─── LAN helpers ─────────────────────────────────────────────────────────────

void Application::closeLanConnection() {
    if (connectThread_.joinable()) connectThread_.join();
    if (connection_) { connection_->close(); connection_.reset(); }
    remoteAgent_.reset();
    handshakeDone_ = false;
}

void Application::sendMoveToPeer(const chess::Move& m) {
    if (!lanMode_ || !connection_ || !connection_->isConnected()) return;
    // Quem acaba de jogar: board_ já comutou o sideToMove, então lado que jogou é oposto.
    const std::string uci = chess::moveToUci(m);
    const std::string msg = "MOVE " + uci
                          + " " + std::to_string(whiteTimeMs_)
                          + " " + std::to_string(blackTimeMs_);
    connection_->sendMessage(msg);
    spdlog::debug("LAN: enviado {}", msg);
}

void Application::handleControlEvent(const std::string& ev) {
    if (ev == "RESIGN" || ev == "BYE") {
        spdlog::info("LAN: {} recebido — {} vence", ev,
                     (remoteColor_ == chess::Color::White) ? "pretas" : "brancas");
        result_ = (remoteColor_ == chess::Color::White)
                ? chess::GameResult::BlackWins
                : chess::GameResult::WhiteWins;
        if (result_ == chess::GameResult::WhiteWins) {
            animator_.animateKingFall(chess::Color::Black);
        } else {
            animator_.animateKingFall(chess::Color::White);
        }
        state_ = ui::AppState::GameOver;
    }
    // DRAW_* e outros: ignorados nesta versão.
}

// Handshake após conectar. Chamado a cada frame enquanto state_==Lobby.
void Application::handleLobbyFrame() {
    if (!connection_) {
        // Conexão não existe (erro de bind/connect): volta pro menu.
        state_ = ui::AppState::MainMenu;
        lanMode_ = false;
        return;
    }

    if (!connection_->isConnected()) {
        // Ainda aguardando cliente (host) ou ainda conectando (client).
        return;
    }

    if (handshakeDone_) return;
    handshakeDone_ = true;

    if (isLanHost_) {
        // Host envia o setup completo.
        const std::string roleStr = (remoteColor_ == chess::Color::White) ? "WHITE" : "BLACK";
        connection_->sendMessage("HELLO chess3d/1 " + myNick_);
        connection_->sendMessage("ROLE " + roleStr);
        connection_->sendMessage("START " + chess::Board().toFen());
        connection_->sendMessage("TC -1 0");  // sem relógio por enquanto (LAN usa setup padrão)

        // Aguarda HELLO do cliente (polling rápido com timeout).
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline) {
            auto msg = connection_->pollIncoming();
            if (msg && msg->rfind("HELLO ", 0) == 0) {
                const std::size_t sp = msg->find(' ', 6);
                opponentNick_ = (sp != std::string::npos) ? msg->substr(sp + 1) : "cliente";
                spdlog::info("LAN: cliente identificado como '{}'", opponentNick_);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } else {
        // Cliente aguarda HELLO+ROLE+START+TC do host.
        connection_->sendMessage("HELLO chess3d/1 " + myNick_);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        bool gotRole = false;
        while (std::chrono::steady_clock::now() < deadline) {
            auto msg = connection_->pollIncoming();
            if (!msg) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            if (msg->rfind("HELLO ", 0) == 0) {
                const std::size_t sp = msg->find(' ', 6);
                opponentNick_ = (sp != std::string::npos) ? msg->substr(sp + 1) : "host";
                spdlog::info("LAN: host identificado como '{}'", opponentNick_);
            } else if (msg->rfind("ROLE ", 0) == 0) {
                const std::string role = msg->substr(5);
                // ROLE = COR DO CLIENTE; humanColor_ = minha cor.
                humanColor_ = (role == "WHITE") ? chess::Color::White : chess::Color::Black;
                remoteColor_ = (humanColor_ == chess::Color::White) ? chess::Color::Black
                                                                    : chess::Color::White;
                gotRole = true;
            } else if (msg->rfind("START ", 0) == 0) {
                // Recebemos posição inicial — já é standard, ignora FEN alternativo por ora.
            } else if (msg->rfind("TC ", 0) == 0) {
                // Tempo: ignorado nesta versão (sem relógio em LAN).
                break;  // TC é a última mensagem do handshake
            }
        }
        if (!gotRole) {
            spdlog::warn("LAN: handshake incompleto — sem ROLE recebido");
        }
    }

    // ── Transição Lobby → Playing ──────────────────────────────────────────
    board_.reset();
    positionHistory_.clear();
    positionHistory_.push_back(chess::positionKey(board_));
    played_.clear();
    capturedByWhite_.clear();
    capturedByBlack_.clear();
    selectedSquare_ = chess::kNoSquare;
    lastMove_.reset();
    pendingPromotion_.reset();
    refreshLegalMoves();
    animator_.initFromBoard(board_);

    // RemoteAgent joga o lado do oponente.
    remoteAgent_ = std::make_shared<ai::RemoteAgent>(connection_.get(), opponentNick_);
    if (remoteColor_ == chess::Color::White) {
        whiteAgent_ = remoteAgent_;
        blackAgent_.reset();
    } else {
        blackAgent_ = remoteAgent_;
        whiteAgent_.reset();
    }

    const bool whiteSide = (humanColor_ == chess::Color::White);
    camera_.setHomeView(glm::vec3(0.0f), 14.0f,
                        whiteSide ? glm::pi<float>() : 0.0f,
                        glm::radians(40.0f));

    state_ = ui::AppState::Playing;
    spdlog::info("LAN: jogo iniciado — eu={} oponente={}",
                 (humanColor_ == chess::Color::White) ? "brancas" : "pretas",
                 opponentNick_);
}

}  // namespace chess3d
