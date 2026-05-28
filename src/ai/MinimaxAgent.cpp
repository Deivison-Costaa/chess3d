#include "MinimaxAgent.h"

#include "EngineCatalog.h"
#include "UciEngineAgent.h"
#include "chess/MoveGenerator.h"
#include "chess/Rules.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <memory>
#include <random>

namespace chess3d::ai {

namespace {

constexpr int kInfScore = 1'000'000;
constexpr int kMateScore = 100'000;

int mvvLvaScore(const chess::Move& m, const chess::Board& board) {
    if (!m.isCapture()) return 0;
    const chess::Piece attacker = board.pieceAt(m.from);
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
        score += 9000 + pieceValue(m.promotion);
    }
    return score;
}

}  // namespace

MinimaxAgent::MinimaxAgent(SearchConfig searchCfg, EvaluatorConfig evalCfg)
    : search_(searchCfg), evaluator_(evalCfg) {}

std::string MinimaxAgent::name() const {
    std::string s = "Minimax(depth=" + std::to_string(search_.depth);
    if (search_.useAlphaBeta) s += ", a/b";
    if (search_.useMoveOrdering) s += ", MVV-LVA";
    if (search_.useIterativeDeepening) s += ", ID";
    if (search_.timeLimitMs > 0)
        s += ", t<=" + std::to_string(search_.timeLimitMs) + "ms";
    s += ")";
    return s;
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

    // Verifica orçamento a cada 4096 nós para não segurar a thread.
    if (search_.timeLimitMs > 0) {
        if (++nodesSinceCheck_ >= 4096) {
            nodesSinceCheck_ = 0;
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - searchStart_).count();
            if (elapsed >= search_.timeLimitMs) searchAborted_ = true;
        }
        if (searchAborted_) return alpha;
    }

    chess::MoveList moves;
    chess::generateLegalMoves(board, moves);

    if (moves.size() == 0) {
        if (board.inCheck(board.sideToMove())) {
            return -kMateScore + (info_.depthReached - depth);
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
            if (alpha >= beta) break;
        }
    }
    return best;
}

chess::Move MinimaxAgent::chooseMove(chess::Board& board) {
    info_ = SearchInfo{};
    searchAborted_ = false;
    nodesSinceCheck_ = 0;
    const auto t0 = std::chrono::steady_clock::now();
    searchStart_ = t0;

    chess::MoveList rootMoves;
    chess::generateLegalMoves(board, rootMoves);
    orderMoves(rootMoves, board);

    if (rootMoves.size() == 0) {
        return chess::Move{};  // mate ou stalemate
    }

    static thread_local std::mt19937 rng{std::random_device{}()};

    auto runDepth = [&](int depth) -> std::vector<SearchInfo::Candidate> {
        info_.depthReached = depth;
        searchAborted_ = false;
        std::vector<SearchInfo::Candidate> results;
        results.reserve(rootMoves.size());
        int alpha = -kInfScore;
        constexpr int beta = kInfScore;
        chess::UndoInfo undo;
        for (const auto& m : rootMoves) {
            if (searchAborted_) break;  // orçamento esgotado durante esta iteração
            board.makeMove(m, undo);
            const int score = -search(board, depth - 1, -beta, -alpha);
            board.unmakeMove(undo);
            results.push_back({m, score});
            if (search_.useAlphaBeta && score > alpha) alpha = score;
        }
        std::sort(results.begin(), results.end(),
                  [](const auto& a, const auto& b) { return a.score > b.score; });
        return results;
    };

    auto budgetExceeded = [&]() {
        if (search_.timeLimitMs <= 0) return false;
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - t0).count();
        return elapsed >= search_.timeLimitMs;
    };

    std::vector<SearchInfo::Candidate> bestComplete;
    if (search_.useIterativeDeepening) {
        for (int d = 1; d <= search_.depth; ++d) {
            auto results = runDepth(d);
            // Só atualiza o melhor resultado se a iteração foi completa.
            // Uma iteração abortada pode ter resultados parciais enviesados.
            if (!searchAborted_) {
                bestComplete = std::move(results);
                // Reordena raiz pelo resultado da última iteração completa — acelera a próxima
                rootMoves.clear();
                for (const auto& c : bestComplete) rootMoves.push(c.move);
            }
            if (budgetExceeded()) break;
        }
    } else {
        bestComplete = runDepth(search_.depth);
    }

    // Se ID expirou antes de completar sequer uma iteração, usa o primeiro move disponível.
    if (bestComplete.empty()) {
        bestComplete.push_back({rootMoves[0], 0});
    }

    // Top 5 e desempate aleatório entre os melhores
    info_.topCandidates.assign(bestComplete.begin(),
                               bestComplete.begin() + std::min<std::size_t>(5, bestComplete.size()));

    chess::Move best = bestComplete.front().move;
    const int bestScore = bestComplete.front().score;
    int tied = 0;
    for (const auto& c : bestComplete) {
        if (c.score != bestScore) break;
        ++tied;
        std::uniform_int_distribution<int> dist(0, tied - 1);
        if (dist(rng) == 0) best = c.move;
    }

    const auto t1 = std::chrono::steady_clock::now();
    info_.evaluation = bestScore;
    info_.elapsed = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0);
    info_.principalVariation = {best};
    return best;
}

namespace {

std::unique_ptr<Agent> makeMinimax(Difficulty d) {
    const DifficultyConfig cfg = configFor(d);
    SearchConfig sc;
    sc.depth = cfg.depth;
    sc.useMoveOrdering = cfg.useMoveOrdering;
    sc.useAlphaBeta = true;
    sc.useIterativeDeepening = (d == Difficulty::Hard);
    sc.timeLimitMs = cfg.timeLimitMs;
    EvaluatorConfig ec;
    ec.usePsts = cfg.usePsts;
    ec.useMobility = cfg.useMobility;
    return std::make_unique<MinimaxAgent>(sc, ec);
}

std::unique_ptr<Agent> makeUci(
        const std::filesystem::path& exe,
        const char* displayName,
        std::vector<std::pair<std::string,std::string>> options,
        int moveTimeMs,
        const char* engineLabel) {
    if (!std::filesystem::exists(exe)) {
        spdlog::warn("{} não encontrado em {} — fallback pra Hard Minimax",
                     engineLabel, exe.string());
        return makeMinimax(Difficulty::Hard);
    }
    auto a = std::make_unique<UciEngineAgent>(exe, displayName, std::move(options), moveTimeMs);
    if (a->ok()) return a;
    spdlog::warn("{} falhou ao iniciar — fallback pra Hard Minimax", engineLabel);
    return makeMinimax(Difficulty::Hard);
}

}  // namespace

std::unique_ptr<Agent> makeAgent(Difficulty d) {
    // Shim de compat: Master → Stockfish via novo factory.
    AgentSpec spec;
    switch (d) {
        case Difficulty::Easy:   spec.engine = AgentSpec::Engine::MinimaxEasy;   break;
        case Difficulty::Medium: spec.engine = AgentSpec::Engine::MinimaxMedium; break;
        case Difficulty::Hard:   spec.engine = AgentSpec::Engine::MinimaxHard;   break;
        case Difficulty::Master: spec.engine = AgentSpec::Engine::Stockfish;     break;
    }
    spec.moveTimeMs = configFor(d).timeLimitMs;
    if (spec.moveTimeMs <= 0) spec.moveTimeMs = 1000;
    return makeAgent(spec);
}

std::unique_ptr<Agent> makeAgent(const AgentSpec& spec) {
    const auto cat = EngineCatalog::detect();
    switch (spec.engine) {
        case AgentSpec::Engine::MinimaxEasy:   return makeMinimax(Difficulty::Easy);
        case AgentSpec::Engine::MinimaxMedium: return makeMinimax(Difficulty::Medium);
        case AgentSpec::Engine::MinimaxHard:   return makeMinimax(Difficulty::Hard);
        case AgentSpec::Engine::Stockfish: {
            // Limita a 1 thread + 32MB hash: evita saturar CPU em AI vs AI.
            std::vector<std::pair<std::string,std::string>> opts = {
                {"Threads", "1"}, {"Hash", "32"},
            };
            return makeUci(cat.stockfishPath, "Stockfish 18", std::move(opts), spec.moveTimeMs, "Stockfish");
        }
        case AgentSpec::Engine::Lc0: {
            std::vector<std::pair<std::string,std::string>> opts = {
                {"WeightsFile", cat.lc0WeightsPath.filename().string()},
                {"Threads", "1"}, {"NNCacheSize", "2000000"},
            };
            return makeUci(cat.lc0ExePath, "Lc0", std::move(opts), spec.moveTimeMs, "Lc0");
        }
        case AgentSpec::Engine::Berserk: {
            std::vector<std::pair<std::string,std::string>> opts = {
                {"EvalFile", cat.berserkNetPath.string()},
                {"Threads", "1"}, {"Hash", "32"},
            };
            return makeUci(cat.berserkExePath, "Berserk 14", std::move(opts), spec.moveTimeMs, "Berserk");
        }
    }
    return makeMinimax(Difficulty::Medium);
}

}  // namespace chess3d::ai
