#pragma once

#include "ai/Agent.h"
#include "ai/DifficultyLevels.h"
#include "ai/EngineCatalog.h"
#include "chess/Move.h"
#include "chess/Rules.h"
#include "chess/Types.h"

#include <functional>
#include <string>
#include <vector>

namespace chess3d::ui {

enum class AppState { MainMenu, Lobby, Playing, GameOver };
enum class GameMode { HumanVsAi, AiVsAi, Hotseat, LanHost, LanClient };

// Modo de tempo. initialMs=0 significa partida sem relógio.
struct TimeControl {
    int initialMs   = 0;  // tempo inicial por jogador
    int incrementMs = 0;  // Fischer increment (não usado nos presets atuais)
    const char* label = "Sem tempo";
};

inline TimeControl kTimeUnlimited{0, 0, "Sem tempo"};
inline TimeControl kTimeBlitz5{5 * 60 * 1000, 0, "Blitz 5+0"};
inline TimeControl kTimeRapid10{10 * 60 * 1000, 0, "Rapid 10+0"};

// Configuração escolhida no menu principal.
struct GameSetup {
    GameMode mode = GameMode::HumanVsAi;
    chess::Color humanColor = chess::Color::White;
    ai::AgentSpec whiteAgent{ai::AgentSpec::Engine::MinimaxMedium, 1000};
    ai::AgentSpec blackAgent{ai::AgentSpec::Engine::MinimaxMedium, 1000};
    bool animateAi = true;
    TimeControl timeControl = kTimeUnlimited;
    // LAN / Hotseat
    std::string lanHost = "192.168.1.100";
    int lanPort = 5021;
    char lanNick[32] = "jogador";
};

struct PromotionRequest {
    chess::Move baseMove;      // o movimento sem o tipo de promoção setado
    bool isCapture = false;
};

// Estado vivo do HUD, atualizado a cada frame pelo Application.
struct HudData {
    chess::Color sideToMove = chess::Color::White;
    bool inCheck = false;
    bool aiThinking = false;
    int fullmove = 1;
    std::vector<std::string> sanHistory;
    std::vector<chess::Piece> capturedByWhite;  // peças pretas capturadas
    std::vector<chess::Piece> capturedByBlack;
    // Relógio (-1 = sem tempo / oculto). Em ms.
    int whiteTimeMs = -1;
    int blackTimeMs = -1;
    // Modo IA vs IA: controles de playback no HUD.
    bool aiVsAi = false;
    bool paused = false;
    float speedMultiplier = 1.0f;
    // Labels dos agentes pra mostrar no HUD em modo IA vs IA.
    std::string whiteAgentName;
    std::string blackAgentName;
    // LAN: esconde Desfazer e mostra nick do oponente.
    bool lanMode = false;
    std::string opponentNick;
};

// Dados exibidos enquanto o host aguarda o cliente.
struct LobbyData {
    std::string localIp;
    int port = 5021;
};

class GameUi {
public:
    using StartGameCb     = std::function<void(const GameSetup&)>;
    using ExitCb          = std::function<void()>;
    using PromotionPickCb = std::function<void(chess::PieceType)>;
    using NewGameCb       = std::function<void()>;
    using BackToMenuCb    = std::function<void()>;
    using UndoCb          = std::function<void()>;
    using PauseCb         = std::function<void(bool)>;
    using SpeedCb         = std::function<void(float)>;
    using CancelLobbyCb   = std::function<void()>;
    using ResumeCb        = std::function<void()>;
    using OpenPauseMenuCb = std::function<void()>;

    // Callbacks
    void setStartGame(StartGameCb cb)         { onStartGame_ = std::move(cb); }
    void setExit(ExitCb cb)                   { onExit_ = std::move(cb); }
    void setPromotionPick(PromotionPickCb cb) { onPromotion_ = std::move(cb); }
    void setNewGame(NewGameCb cb)             { onNewGame_ = std::move(cb); }
    void setBackToMenu(BackToMenuCb cb)       { onBackToMenu_ = std::move(cb); }
    void setUndo(UndoCb cb)                   { onUndo_ = std::move(cb); }
    void setPause(PauseCb cb)                 { onPause_ = std::move(cb); }
    void setSpeed(SpeedCb cb)                 { onSpeed_ = std::move(cb); }
    void setCancelLobby(CancelLobbyCb cb)     { onCancelLobby_ = std::move(cb); }
    void setResume(ResumeCb cb)               { onResume_ = std::move(cb); }
    void setOpenPauseMenu(OpenPauseMenuCb cb) { onOpenPauseMenu_ = std::move(cb); }

    void setEngineCatalog(const ai::EngineCatalog& c) { catalog_ = c; }

    void renderMainMenu();
    void renderLobby(const LobbyData& data);
    void renderHud(const HudData& data);
    void renderPauseMenu(bool lanMode);
    void renderEndGame(chess::GameResult result, int totalPlies);
    void renderPromotionDialog(const PromotionRequest& req);
    void renderDebugPanel(const ai::SearchInfo& info, const std::string& agentName,
                          chess::Color agentSide);

    GameSetup& setup() { return setup_; }
    const GameSetup& setup() const { return setup_; }

    void toggleDebug() { showDebug_ = !showDebug_; }
    bool isDebugVisible() const { return showDebug_; }

private:
    void renderEngineCombo(const char* idLabel, ai::AgentSpec& spec);

    StartGameCb     onStartGame_;
    ExitCb          onExit_;
    PromotionPickCb onPromotion_;
    NewGameCb       onNewGame_;
    BackToMenuCb    onBackToMenu_;
    UndoCb          onUndo_;
    PauseCb         onPause_;
    SpeedCb         onSpeed_;
    CancelLobbyCb   onCancelLobby_;
    ResumeCb        onResume_;
    OpenPauseMenuCb onOpenPauseMenu_;

    GameSetup setup_;
    ai::EngineCatalog catalog_;
    bool showDebug_ = false;
};

}  // namespace chess3d::ui
