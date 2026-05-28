#include "MinimaxAgent.h"

#include "chess/MoveGenerator.h"
#include "chess/Rules.h"

#include <algorithm>
#include <limits>
#include <random>

namespace chess3d::ai {

namespace {

constexpr int kInfScore = 1'000'000;
constexpr int kMateScore = 100'000;  // grande, mas com folga para penalidades por depth

}  // namespace

MinimaxAgent::MinimaxAgent(int depth, EvaluatorConfig evalCfg)
    : depth_(depth), evaluator_(evalCfg) {}

std::string MinimaxAgent::name() const {
    return "Minimax(depth=" + std::to_string(depth_) + ")";
}

int MinimaxAgent::negamax(chess::Board& board, int depth) {
    ++info_.nodesVisited;

    chess::MoveList moves;
    chess::generateLegalMoves(board, moves);

    if (moves.size() == 0) {
        // Sem movimentos: mate ou stalemate.
        if (board.inCheck(board.sideToMove())) {
            // Mate — descontamos a profundidade para preferir mates curtos.
            return -kMateScore + (depth_ - depth);
        }
        return 0;  // stalemate
    }

    if (depth == 0) {
        return evaluator_.evaluate(board);
    }

    int best = -kInfScore;
    chess::UndoInfo undo;
    for (const auto& m : moves) {
        board.makeMove(m, undo);
        const int score = -negamax(board, depth - 1);
        board.unmakeMove(undo);
        if (score > best) best = score;
    }
    return best;
}

chess::Move MinimaxAgent::chooseMove(chess::Board& board) {
    info_ = SearchInfo{};
    const auto t0 = std::chrono::steady_clock::now();

    chess::MoveList moves;
    chess::generateLegalMoves(board, moves);

    chess::Move best{};
    int bestScore = -kInfScore;
    chess::UndoInfo undo;

    // Entre vários movimentos com mesma pontuação, escolhemos aleatoriamente
    // para evitar partidas determinísticas.
    static thread_local std::mt19937 rng{std::random_device{}()};
    int tiedCount = 0;

    for (const auto& m : moves) {
        board.makeMove(m, undo);
        const int score = -negamax(board, depth_ - 1);
        board.unmakeMove(undo);

        if (score > bestScore) {
            bestScore = score;
            best = m;
            tiedCount = 1;
        } else if (score == bestScore) {
            // Reservoir sampling: cada novo empate tem 1/n chance de substituir.
            ++tiedCount;
            std::uniform_int_distribution<int> dist(0, tiedCount - 1);
            if (dist(rng) == 0) best = m;
        }
    }

    const auto t1 = std::chrono::steady_clock::now();
    info_.depthReached = depth_;
    info_.evaluation = bestScore;
    info_.elapsed = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);
    info_.principalVariation = {best};
    return best;
}

}  // namespace chess3d::ai
