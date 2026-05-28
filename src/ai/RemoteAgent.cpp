#include "RemoteAgent.h"

#include "chess/MoveGenerator.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <thread>

namespace chess3d::ai {

namespace {

// Converte UCI string → Move legal no board atual. Retorna Move{} se inválido.
chess::Move uciToMove(const std::string& uci, chess::Board& board) {
    if (uci.size() < 4) return {};
    const int fromFile = uci[0] - 'a';
    const int fromRank = uci[1] - '1';
    const int toFile   = uci[2] - 'a';
    const int toRank   = uci[3] - '1';
    if (fromFile < 0 || fromFile > 7 || fromRank < 0 || fromRank > 7 ||
        toFile   < 0 || toFile   > 7 || toRank   < 0 || toRank   > 7) {
        return {};
    }
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

void RemoteAgent::enqueueControl(std::string msg) {
    std::lock_guard<std::mutex> lk(ctrlMutex_);
    controlEvents_.push(std::move(msg));
}

std::optional<std::string> RemoteAgent::pollControlEvent() {
    std::lock_guard<std::mutex> lk(ctrlMutex_);
    if (controlEvents_.empty()) return std::nullopt;
    std::string ev = std::move(controlEvents_.front());
    controlEvents_.pop();
    return ev;
}

chess::Move RemoteAgent::chooseMove(chess::Board& board) {
    // RemoteAgent é o único consumidor de conn_->pollIncoming() durante o jogo.
    // Mensagens de controle (RESIGN/BYE/PING) são enfileiradas em controlEvents_;
    // Application as drena via pollControlEvent() no frame seguinte.
    using namespace std::chrono_literals;

    while (!cancelled_.load()) {
        if (!conn_->isConnected()) {
            spdlog::warn("RemoteAgent: conexão perdida aguardando lance");
            enqueueControl("BYE");
            return {};
        }
        auto msg = conn_->pollIncoming();
        if (msg) {
            const std::string& s = *msg;
            if (s.rfind("MOVE ", 0) == 0) {
                // Formato: "MOVE <uci> [<whiteMs> <blackMs>]"
                const std::size_t sp1 = s.find(' ', 5);
                const std::string uci = (sp1 == std::string::npos)
                                        ? s.substr(5)
                                        : s.substr(5, sp1 - 5);
                const chess::Move m = uciToMove(uci, board);
                if (!m.isNull()) return m;
                spdlog::warn("RemoteAgent: UCI inválido '{}' — ignorado", uci);
            } else if (s.rfind("PING ", 0) == 0) {
                // Responde PONG inline (conn_->sendMessage é thread-safe).
                conn_->sendMessage("PONG " + s.substr(5));
            } else {
                // RESIGN, BYE, DRAW_OFFER, etc. — enfileira para o Application tratar.
                enqueueControl(s);
            }
            continue;
        }
        std::this_thread::sleep_for(10ms);
    }
    return {};  // cancelado
}

}  // namespace chess3d::ai
