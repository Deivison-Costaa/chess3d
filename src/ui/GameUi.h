#pragma once

#include "ai/DifficultyLevels.h"
#include "chess/Move.h"
#include "chess/Rules.h"
#include "chess/Types.h"

#include <functional>
#include <string>
#include <vector>

namespace chess3d::ui {

enum class AppState { MainMenu, Playing, GameOver };

// Configuração escolhida no menu principal.
struct GameSetup {
    chess::Color humanColor = chess::Color::White;
    ai::Difficulty difficulty = ai::Difficulty::Medium;
    bool animateAi = true;
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
};

class GameUi {
public:
    using StartGameCb     = std::function<void(const GameSetup&)>;
    using ExitCb          = std::function<void()>;
    using PromotionPickCb = std::function<void(chess::PieceType)>;
    using NewGameCb       = std::function<void()>;
    using BackToMenuCb    = std::function<void()>;
    using UndoCb          = std::function<void()>;

    // Callbacks
    void setStartGame(StartGameCb cb)         { onStartGame_ = std::move(cb); }
    void setExit(ExitCb cb)                   { onExit_ = std::move(cb); }
    void setPromotionPick(PromotionPickCb cb) { onPromotion_ = std::move(cb); }
    void setNewGame(NewGameCb cb)             { onNewGame_ = std::move(cb); }
    void setBackToMenu(BackToMenuCb cb)       { onBackToMenu_ = std::move(cb); }
    void setUndo(UndoCb cb)                   { onUndo_ = std::move(cb); }

    void renderMainMenu();
    void renderHud(const HudData& data);
    void renderEndGame(chess::GameResult result, int totalPlies);
    void renderPromotionDialog(const PromotionRequest& req);

    GameSetup& setup() { return setup_; }
    const GameSetup& setup() const { return setup_; }

private:
    StartGameCb     onStartGame_;
    ExitCb          onExit_;
    PromotionPickCb onPromotion_;
    NewGameCb       onNewGame_;
    BackToMenuCb    onBackToMenu_;
    UndoCb          onUndo_;

    GameSetup setup_;
};

}  // namespace chess3d::ui
