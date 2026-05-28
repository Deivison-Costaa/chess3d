#include "GameSession.h"

#include "ai/MinimaxAgent.h"
#include "chess/Notation.h"
#include "net/Protocol.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <thread>

namespace chess3d {

namespace {

// Converts a UCI string to a legal Move in the current position.
chess::Move uciToMove(const std::string& uci, chess::Board& board) {
    if (uci.size() < 4) return {};
    const int fromFile = uci[0] - 'a';
    const int fromRank = uci[1] - '1';
    const int toFile   = uci[2] - 'a';
    const int toRank   = uci[3] - '1';
    if (fromFile < 0 || fromFile > 7 || fromRank < 0 || fromRank > 7 ||
        toFile   < 0 || toFile   > 7 || toRank   < 0 || toRank   > 7) return {};
    const chess::Square from = chess::makeSquare(fromFile, fromRank);
    const chess::Square to   = chess::makeSquare(toFile,   toRank);
    chess::PieceType promo   = chess::PieceType::None;
    if (uci.size() >= 5) {
        switch (uci[4]) {
            case 'q': promo = chess::PieceType::Queen;  break;
            case 'r': promo = chess::PieceType::Rook;   break;
            case 'b': promo = chess::PieceType::Bishop; break;
            case 'n': promo = chess::PieceType::Knight; break;
            default: break;
        }
    }
    chess::MoveList legal;
    chess::generateLegalMoves(board, legal);
    for (std::size_t i = 0; i < legal.size(); ++i) {
        const auto& m = legal[i];
        if (m.from == from && m.to == to && m.promotion == promo) return m;
    }
    return {};
}

}  // namespace

// ─── Lifecycle ───────────────────────────────────────────────────────────────

GameSession::~GameSession() {
    if (remoteAgent_) remoteAgent_->cancel();
    closeLan();
}

bool GameSession::start(const ui::GameSetup& setup) {
    // Reset state
    if (remoteAgent_) remoteAgent_->cancel();
    closeLan();

    myNick_    = setup.lanNick;
    aiVsAi_    = (setup.mode == ui::GameMode::AiVsAi);
    lanMode_   = (setup.mode == ui::GameMode::LanHost || setup.mode == ui::GameMode::LanClient);
    isLanHost_ = (setup.mode == ui::GameMode::LanHost);
    humanColor_ = setup.humanColor;
    result_     = chess::GameResult::Ongoing;
    whiteTimeMs_ = (setup.timeControl.initialMs > 0) ? setup.timeControl.initialMs : -1;
    blackTimeMs_ = whiteTimeMs_;
    incrementMs_ = setup.timeControl.incrementMs;

    whiteAgent_.reset();
    blackAgent_.reset();
    remoteAgent_.reset();

    const bool isHotseat = (setup.mode == ui::GameMode::Hotseat);

    if (!lanMode_) {
        if (aiVsAi_) {
            whiteAgent_ = ai::makeAgent(setup.whiteAgent);
            blackAgent_ = ai::makeAgent(setup.blackAgent);
        } else if (!isHotseat) {
            if (humanColor_ == chess::Color::White) {
                blackAgent_ = ai::makeAgent(setup.blackAgent);
            } else {
                whiteAgent_ = ai::makeAgent(setup.whiteAgent);
            }
        }
    }

    board_.reset();
    positionHistory_.clear();
    positionHistory_.push_back(chess::positionKey(board_));
    played_.clear();
    capturedByWhite_.clear();
    capturedByBlack_.clear();
    lastMove_.reset();
    pendingPromotion_.reset();
    refreshLegalMoves();

    if (lanMode_) {
        return doLanHandshake(setup);
    }
    return true;
}

// ─── Human input ─────────────────────────────────────────────────────────────

bool GameSession::submitMove(const std::string& uci) {
    if (isOver()) return false;
    if (pendingPromotion_) return false;

    // Verify it's a human turn (no agent for this side)
    const auto* agentPtr = (board_.sideToMove() == chess::Color::White)
                         ? whiteAgent_.get() : blackAgent_.get();
    if (agentPtr) return false;  // AI turn

    chess::Move m = uciToMove(uci, board_);
    if (m.isNull()) return false;

    // If it's a promotion without a piece suffix, queue a promotion request.
    if (m.isPromotion() && m.promotion == chess::PieceType::None) {
        pendingPromotion_ = ui::PromotionRequest{m, m.isCapture()};
        return true;
    }

    applyMoveInternal(m);
    if (lanMode_) sendMoveToPeer(m);
    return true;
}

bool GameSession::acceptPromotion(chess::PieceType pt) {
    if (!pendingPromotion_) return false;
    chess::Move m = pendingPromotion_->baseMove;
    m.promotion = pt;
    m.flag = pendingPromotion_->isCapture ? chess::MoveFlag::PromotionCapture
                                          : chess::MoveFlag::Promotion;
    pendingPromotion_.reset();
    applyMoveInternal(m);
    if (lanMode_) sendMoveToPeer(m);
    return true;
}

// ─── Tick ────────────────────────────────────────────────────────────────────

void GameSession::tick(int dtMs) {
    if (isOver()) return;

    // Drain LAN control events (RESIGN, BYE, etc.)
    if (lanMode_ && remoteAgent_) {
        auto ev = remoteAgent_->pollControlEvent();
        if (ev) handleControlEvent(*ev);
    }
    if (isOver()) return;

    // Clock decrement
    if (whiteTimeMs_ >= 0 && dtMs > 0) {
        int& clock = (board_.sideToMove() == chess::Color::White) ? whiteTimeMs_ : blackTimeMs_;
        clock = std::max(0, clock - dtMs);
        if (clock == 0) {
            result_ = (board_.sideToMove() == chess::Color::White)
                    ? chess::GameResult::BlackWinsOnTime
                    : chess::GameResult::WhiteWinsOnTime;
            spdlog::info("GameSession: clock expired — {}", chess::gameResultName(result_));
            return;
        }
    }

    // AI move (synchronous — blocks until agent returns)
    if (!pendingPromotion_) {
        doAiMove();
    }
}

// ─── Undo ────────────────────────────────────────────────────────────────────

bool GameSession::undo() {
    if (lanMode_) return false;
    if (isOver()) return false;
    if (played_.empty()) return false;

    const int count = (played_.size() >= 2) ? 2 : 1;
    for (int i = 0; i < count; ++i) {
        if (played_.empty()) break;
        const auto& last = played_.back();
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
    lastMove_ = played_.empty() ? std::optional<chess::Move>{} : std::optional<chess::Move>(played_.back().move);
    refreshLegalMoves();
    return true;
}

// ─── Internal helpers ────────────────────────────────────────────────────────

void GameSession::refreshLegalMoves() {
    legalMoves_.clear();
    chess::generateLegalMoves(board_, legalMoves_);
    const auto prev = result_;
    result_ = chess::evaluateGame(board_, positionHistory_);
    if (result_ != chess::GameResult::Ongoing && prev == chess::GameResult::Ongoing) {
        spdlog::info("GameSession: {}", chess::gameResultName(result_));
    }
}

void GameSession::applyMoveInternal(const chess::Move& m) {
    chess::Board boardBefore = board_;
    const std::string san = chess::moveToSan(m, boardBefore);

    // Track capture
    chess::Piece captured{};
    if (m.flag == chess::MoveFlag::Capture || m.flag == chess::MoveFlag::PromotionCapture) {
        captured = board_.pieceAt(m.to);
    } else if (m.flag == chess::MoveFlag::EnPassant) {
        const int dir = chess::pawnForward(boardBefore.pieceAt(m.from).color);
        captured = board_.pieceAt(chess::makeSquare(chess::fileOf(m.to),
                                                    chess::rankOf(m.to) - dir));
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

    if (incrementMs_ > 0) {
        if (boardBefore.sideToMove() == chess::Color::White) whiteTimeMs_ += incrementMs_;
        else                                                 blackTimeMs_ += incrementMs_;
    }

    refreshLegalMoves();
    spdlog::debug("GameSession: {} {}", san, chess::moveToUci(m));
}

bool GameSession::doAiMove() {
    auto& agentPtr = (board_.sideToMove() == chess::Color::White) ? whiteAgent_ : blackAgent_;
    if (!agentPtr) return false;  // human turn

    chess::Board snapshot = board_;
    const chess::Move m = agentPtr->chooseMove(snapshot);
    if (m.isNull()) {
        spdlog::warn("GameSession: agent '{}' returned null move", agentPtr->name());
        return false;
    }

    applyMoveInternal(m);
    // Only send to peer if this was OUR local move, not a move received from the remote.
    // RemoteAgent already consumed the MOVE message from the peer; echoing it back would
    // confuse the other side into treating our own move as their opponent's.
    if (lanMode_ && agentPtr != remoteAgent_) {
        sendMoveToPeer(m);
    }
    return true;
}

void GameSession::handleControlEvent(const std::string& ev) {
    if (ev == "RESIGN" || ev == "BYE") {
        result_ = (remoteColor_ == chess::Color::White)
                ? chess::GameResult::BlackWins
                : chess::GameResult::WhiteWins;
        spdlog::info("GameSession LAN: {} → {}", ev, chess::gameResultName(result_));
    }
}

// ─── LAN ────────────────────────────────────────────────────────────────────

void GameSession::sendMoveToPeer(const chess::Move& m) {
    if (!connection_ || !connection_->isConnected()) return;
    const std::string msg = net::proto::move(chess::moveToUci(m), whiteTimeMs_, blackTimeMs_);
    connection_->sendMessage(msg);
    spdlog::debug("GameSession LAN send: {}", msg);
}

bool GameSession::doLanHandshake(const ui::GameSetup& setup) {
    connection_ = std::make_unique<net::LanConnection>();

    if (isLanHost_) {
        humanColor_ = setup.humanColor;
        remoteColor_ = (humanColor_ == chess::Color::White) ? chess::Color::Black
                                                            : chess::Color::White;
        if (!connection_->startListening(static_cast<uint16_t>(setup.lanPort))) {
            spdlog::error("GameSession LAN: startListening failed on port {}", setup.lanPort);
            connection_.reset();
            lanMode_ = false;
            return false;
        }
        // Wait for client to connect (blocking with timeout)
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (!connection_->isConnected() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!connection_->isConnected()) {
            spdlog::error("GameSession LAN: timed out waiting for client");
            connection_.reset(); lanMode_ = false;
            return false;
        }
        // Send setup to client
        const std::string roleStr = (remoteColor_ == chess::Color::White) ? "WHITE" : "BLACK";
        connection_->sendMessage(net::proto::hello(myNick_));
        connection_->sendMessage(net::proto::role(remoteColor_));
        connection_->sendMessage(net::proto::start(chess::Board().toFen()));
        connection_->sendMessage(net::proto::tc(whiteTimeMs_, blackTimeMs_));
        // Await HELLO from client
        const auto d2 = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < d2) {
            auto msg = connection_->pollIncoming();
            if (msg) {
                auto nick = net::proto::parseHelloNick(*msg);
                if (nick) { opponentNick_ = *nick; break; }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } else {
        // Client: connect to host
        const std::string host = setup.lanHost;
        const uint16_t port = static_cast<uint16_t>(setup.lanPort);
        if (!connection_->connectTo(host, port)) {
            spdlog::error("GameSession LAN: connectTo {}:{} failed", host, port);
            connection_.reset(); lanMode_ = false;
            return false;
        }
        // Send HELLO first
        connection_->sendMessage(net::proto::hello(myNick_));
        // Await HELLO + ROLE + START + TC from host
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        bool gotRole = false;
        while (std::chrono::steady_clock::now() < deadline) {
            auto msg = connection_->pollIncoming();
            if (!msg) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); continue; }
            if (auto nick = net::proto::parseHelloNick(*msg)) {
                opponentNick_ = *nick;
            } else if (auto color = net::proto::parseRole(*msg)) {
                humanColor_  = *color;
                remoteColor_ = chess::other(*color);
                gotRole = true;
            } else if (net::proto::parseStartFen(*msg)) {
                // ignore FEN for now — always start from standard position
            } else if (auto clocks = net::proto::parseTc(*msg)) {
                whiteTimeMs_ = clocks->first;
                blackTimeMs_ = clocks->second;
                break;  // TC is last handshake message
            }
        }
        if (!gotRole) {
            spdlog::warn("GameSession LAN: handshake incomplete — no ROLE received");
        }
    }

    // Wire up RemoteAgent for the opponent's side.
    // Also create the LOCAL agent when a non-human spec was requested
    // (headless/automated play — the original Application always used a human locally).
    remoteAgent_ = std::make_shared<ai::RemoteAgent>(connection_.get(), opponentNick_);

    auto makeLocalAgent = [&](const ai::AgentSpec& spec) -> std::shared_ptr<ai::Agent> {
        // "human" means no agent (null); anything else creates an AI.
        if (spec.engine == ai::AgentSpec::Engine::MinimaxEasy   ||
            spec.engine == ai::AgentSpec::Engine::MinimaxMedium ||
            spec.engine == ai::AgentSpec::Engine::MinimaxHard   ||
            ai::isUciEngine(spec.engine)) {
            return ai::makeAgent(spec);
        }
        return nullptr;  // human side
    };

    if (remoteColor_ == chess::Color::White) {
        whiteAgent_ = remoteAgent_;
        blackAgent_ = makeLocalAgent(setup.blackAgent);  // local plays black
    } else {
        blackAgent_ = remoteAgent_;
        whiteAgent_ = makeLocalAgent(setup.whiteAgent);  // local plays white
    }
    spdlog::info("GameSession LAN: ready — me={} opponent={}",
                 (humanColor_ == chess::Color::White) ? "white" : "black", opponentNick_);
    return true;
}

void GameSession::closeLan() {
    if (connectThread_.joinable()) connectThread_.join();
    if (connection_) { connection_->close(); connection_.reset(); }
    remoteAgent_.reset();
}

}  // namespace chess3d
