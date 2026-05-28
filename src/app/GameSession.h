#pragma once

#include "ai/Agent.h"
#include "ai/DifficultyLevels.h"
#include "ai/RemoteAgent.h"
#include "chess/Board.h"
#include "chess/MoveGenerator.h"
#include "chess/Rules.h"
#include "net/LanConnection.h"
#include "ui/GameUi.h"

#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace chess3d {

// Pure game-logic session with no rendering or window dependencies.
// Used by HeadlessRunner for CLI/automated play and by tests.
// Application owns a parallel implementation; GameSession does NOT replace it.
class GameSession {
public:
    struct Played {
        chess::Move move;
        std::string san;
        chess::Board boardBefore;
    };

    GameSession() = default;
    ~GameSession();

    // Starts a new game from the given setup.
    // For LAN modes, performs the TCP handshake (blocking).
    // Returns false on LAN connection failure.
    bool start(const ui::GameSetup& setup);

    // Apply a human move given as UCI string (e.g. "e2e4", "e7e8q").
    // Returns false if it's not the human's turn, the move is illegal,
    // or a promotion choice is still pending.
    bool submitMove(const std::string& uci);

    // Resolve a pending promotion. Returns false if none is pending.
    bool acceptPromotion(chess::PieceType pt);
    bool hasPendingPromotion() const { return pendingPromotion_.has_value(); }

    // Advance the game by one tick (synchronous/blocking AI + network drain).
    // dtMs: elapsed wall-clock milliseconds for the clock decrement.
    void tick(int dtMs = 16);

    // Undo the last turn (1 or 2 half-moves, back to the human's turn).
    // Returns false if not applicable (LAN, over, or nothing to undo).
    bool undo();

    bool isOver() const { return result_ != chess::GameResult::Ongoing; }
    chess::GameResult result() const { return result_; }

    const chess::Board& board()           const { return board_; }
    const chess::MoveList& legalMoves()   const { return legalMoves_; }
    const std::vector<Played>& played()   const { return played_; }
    const std::vector<std::string>& positionHistory() const { return positionHistory_; }
    const std::vector<chess::Piece>& capturedByWhite() const { return capturedByWhite_; }
    const std::vector<chess::Piece>& capturedByBlack() const { return capturedByBlack_; }

    int whiteTimeMs() const { return whiteTimeMs_; }
    int blackTimeMs() const { return blackTimeMs_; }
    bool aiVsAi()    const { return aiVsAi_; }
    bool lanMode()   const { return lanMode_; }
    const std::string& opponentNick() const { return opponentNick_; }

    // Close LAN connection and join threads (safe to call multiple times).
    void closeLan();

private:
    void refreshLegalMoves();
    void applyMoveInternal(const chess::Move& m);
    bool doAiMove();   // synchronous: calls agent->chooseMove; returns false if no agent this turn
    void handleControlEvent(const std::string& ev);
    bool doLanHandshake(const ui::GameSetup& setup);
    void sendMoveToPeer(const chess::Move& m);

    chess::Board board_;
    chess::MoveList legalMoves_;
    std::vector<std::string> positionHistory_;
    chess::GameResult result_ = chess::GameResult::Ongoing;

    std::optional<chess::Move> lastMove_;
    std::optional<ui::PromotionRequest> pendingPromotion_;

    std::vector<Played> played_;
    std::vector<chess::Piece> capturedByWhite_;
    std::vector<chess::Piece> capturedByBlack_;

    std::shared_ptr<ai::Agent> whiteAgent_;
    std::shared_ptr<ai::Agent> blackAgent_;
    chess::Color humanColor_ = chess::Color::White;
    bool aiVsAi_    = false;
    bool lanMode_   = false;
    bool isLanHost_ = false;

    int whiteTimeMs_ = -1;
    int blackTimeMs_ = -1;
    int incrementMs_ = 0;

    std::unique_ptr<net::LanConnection> connection_;
    std::shared_ptr<ai::RemoteAgent>    remoteAgent_;
    chess::Color remoteColor_ = chess::Color::Black;
    std::string  opponentNick_;
    std::string  myNick_;
    std::thread  connectThread_;
};

}  // namespace chess3d
