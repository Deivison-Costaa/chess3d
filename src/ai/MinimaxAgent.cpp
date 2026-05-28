#include "MinimaxAgent.h"

#include "chess/MoveGenerator.h"
#include "chess/Rules.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <random>

namespace chess3d::ai {

namespace {

constexpr int kInfScore = 1'000'000;
constexpr int kMateScore = 100'000;

// MVV-LVA: Most Valuable Victim, Least Valuable Attacker.
// Captura de rainha por peão é a primeira; peão por rainha vai pro fim.
int mvvLvaScore(const chess::Move& m, const chess::Board& board) {
    if (!m.isCapture()) return 0;
    const chess::Piece attacker = board.pieceAt(m.from);

    // Vítima depende do tipo de captura
    int victimValue = 0;
    if (m.flag == chess::MoveFlag::EnPassant) {
        victimValue = pieceValue(chess::PieceType::Pawn);
    } else {
        victimValue = pieceValue(board.pieceAt(m.to).type);
    }
    return 10 * victimValue - pieceValue(attacker.type) + 10000;
}

int moveOrderScore(const chess::Move& m, const chess::Board& board) {
    int score = mvvLvaScore(m, board);
    if (m.isPromotion()) {
        score += 9000 + pieceValue(m.promotion);  // promoção a rainha tem prioridade
    }
    return score;
}

}  // namespace

MinimaxAgent::MinimaxAgent(SearchConfig searchCfg, EvaluatorConfig evalCfg)
    : search_(searchCfg), evaluator_(evalCfg) {}

std::string MinimaxAgent::name() const {
    return "Minimax(depth=" + std::to_string(search_.depth) +
           (search_.useAlphaBeta ? ", a/b" : "") +
           (search_.useMoveOrdering ? ", MVV-LVA" : "") + ")";
}

void MinimaxAgent::orderMoves(chess::MoveList& moves, const chess::Board& board) {
    if (!search_.useMoveOrdering) return;
    std::sort(moves.begin(), moves.end(),
              [&](const chess::Move& a, const chess::Move& b) {
                  return moveOrderScore(a, board) > moveOrderScore(b, board);
              });
}

int MinimaxAgent::search(chess::Board& board, int depth, int alpha, int beta) {
    ++info_.nodesVisited;

    chess::MoveList moves;
    chess::generateLegalMoves(board, moves);

    if (moves.size() == 0) {
        if (board.inCheck(board.sideToMove())) {
            return -kMateScore + (search_.depth - depth);
        }
        return 0;
    }

    if (depth == 0) {
        return evaluator_.evaluate(board);
    }

    orderMoves(moves, board);

    int best = -kInfScore;
    chess::UndoInfo undo;
    for (const auto& m : moves) {
        board.makeMove(m, undo);
        const int score = -search(board, depth - 1, -beta, -alpha);
        board.unmakeMove(undo);
        if (score > best) best = score;
        if (search_.useAlphaBeta) {
            if (best > alpha) alpha = best;
            if (alpha >= beta) break;  // poda beta
        }
    }
    return best;
}

chess::Move MinimaxAgent::chooseMove(chess::Board& board) {
    info_ = SearchInfo{};
    const auto t0 = std::chrono::steady_clock::now();

    chess::MoveList moves;
    chess::generateLegalMoves(board, moves);
    orderMoves(moves, board);

    chess::Move best{};
    int bestScore = -kInfScore;
    int alpha = -kInfScore;
    constexpr int beta = kInfScore;
    chess::UndoInfo undo;

    static thread_local std::mt19937 rng{std::random_device{}()};
    int tiedCount = 0;

    for (const auto& m : moves) {
        board.makeMove(m, undo);
        const int score = -search(board, search_.depth - 1, -beta, -alpha);
        board.unmakeMove(undo);

        if (score > bestScore) {
            bestScore = score;
            best = m;
            tiedCount = 1;
            if (search_.useAlphaBeta && score > alpha) alpha = score;
        } else if (score == bestScore) {
            ++tiedCount;
            std::uniform_int_distribution<int> dist(0, tiedCount - 1);
            if (dist(rng) == 0) best = m;
        }
    }

    const auto t1 = std::chrono::steady_clock::now();
    info_.depthReached = search_.depth;
    info_.evaluation = bestScore;
    info_.elapsed = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);
    info_.principalVariation = {best};
    return best;
}

std::unique_ptr<Agent> makeAgent(Difficulty d) {
    const DifficultyConfig cfg = configFor(d);
    SearchConfig sc;
    sc.depth = cfg.depth;
    sc.useMoveOrdering = cfg.useMoveOrdering;
    sc.useAlphaBeta = true;  // sempre on a partir da Fase 8
    EvaluatorConfig ec;
    ec.usePsts = cfg.usePsts;
    ec.useMobility = cfg.useMobility;
    return std::make_unique<MinimaxAgent>(sc, ec);
}

}  // namespace chess3d::ai
