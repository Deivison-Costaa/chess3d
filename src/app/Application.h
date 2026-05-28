#pragma once

#include "ai/Agent.h"
#include "ai/DifficultyLevels.h"
#include "ai/RemoteAgent.h"
#include "anim/Animator.h"
#include "app/Window.h"
#include "chess/Board.h"
#include "chess/MoveGenerator.h"
#include "chess/Rules.h"
#include "input/InputHandler.h"
#include "net/LanConnection.h"
#include "render/Camera.h"
#include "render/GltfLoader.h"
#include "render/Mesh.h"
#include "render/Shader.h"
#include "ui/GameUi.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <chrono>
#include <deque>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <thread>
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

    // ── LAN / Hotseat ──
    void sendMoveToPeer(const chess::Move& m);
    void handleLobbyFrame();     // transição Lobby→Playing via handshake
    void handleControlEvent(const std::string& ev);
    void closeLanConnection();   // cancel + close + join thread cliente

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
    std::vector<GLuint> imageTextures_;  // GL texture por GltfImage

    // Override de textura por material index. Carregamos texturas PBR externas
    // (assets/textures/*.jpg) e mapeamos por material name do .glb pra dar PBR
    // bonito sem depender das texturas embutidas no asset (que estão ausentes).
    struct ExternalTextureSet {
        GLuint diff  = 0;
        GLuint nor   = 0;
        GLuint rough = 0;
    };
    std::vector<ExternalTextureSet> externalSets_;
    std::vector<int> materialOverride_;  // index em externalSets_ ou -1

    bool usePbrTextures_ = true;  // toggle pela tecla T
    int debugViewMode_ = 0;       // 0=lit, 1=albedo, 2=normal, 3=roughness, 4=metallic, 5=uv (F4)
    bool applyGamma_ = true;      // toggle pela tecla G — tira degamma de albedo
    bool useNormalMap_ = true;    // default ON com texturas externas (ambientCG). Toggle: L

    chess::Board board_;
    chess::MoveList legalMoves_;
    std::vector<std::string> positionHistory_;
    chess::GameResult result_ = chess::GameResult::Ongoing;

    chess::Square selectedSquare_ = chess::kNoSquare;
    std::optional<chess::Move> lastMove_;
    std::optional<ui::PromotionRequest> pendingPromotion_;

    anim::Animator animator_;
    float lastFrameTime_ = 0.0f;

    // Um agente por lado. Em HumanVsAi um deles é null (o lado humano).
    // Em AiVsAi ambos estão preenchidos.
    // shared_ptr porque o future captura o agent por valor para manter vivo durante o async.
    std::shared_ptr<ai::Agent> whiteAgent_;
    std::shared_ptr<ai::Agent> blackAgent_;
    chess::Color humanColor_ = chess::Color::White;  // só usado em HumanVsAi
    bool aiVsAi_ = false;
    bool aiThinking_ = false;

    // Execução assíncrona da IA (impede bloqueio do main thread).
    std::future<chess::Move> aiFuture_;
    std::shared_ptr<ai::Agent> aiInFlight_;         // mantém o agent vivo até o future terminar
    std::chrono::steady_clock::time_point aiStart_; // instante em que o async foi lançado

    // Playback (só em IA vs IA).
    bool paused_ = false;
    float speedMultiplier_ = 1.0f;

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

    // Relógio (modo temporizado). -1 = sem tempo (modo casual).
    int whiteTimeMs_ = -1;
    int blackTimeMs_ = -1;
    int incrementMs_ = 0;

    // ── LAN / Hotseat ──────────────────────────────────────────────────────
    std::unique_ptr<net::LanConnection> connection_;
    std::shared_ptr<ai::RemoteAgent>    remoteAgent_;
    bool lanMode_     = false;
    bool isLanHost_   = false;  // true = host, false = client
    std::string opponentNick_;
    std::string myNick_;
    chess::Color remoteColor_ = chess::Color::Black;
    ui::LobbyData lobbyData_;
    std::thread connectThread_;  // thread do cliente conectando

    // Estado de handshake para o host aguardar HELLO do cliente.
    bool handshakeDone_ = false;

    GLuint cameraUbo_ = 0;
};

}  // namespace chess3d
