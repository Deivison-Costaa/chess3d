#pragma once

#include "Agent.h"
#include "DifficultyLevels.h"
#include "Evaluator.h"
#include "chess/MoveGenerator.h"

#include <chrono>

namespace chess3d::ai {

struct SearchConfig {
    int depth = 2;
    bool useAlphaBeta = true;
    bool useMoveOrdering = false;  // MVV-LVA
    bool useIterativeDeepening = false;
    int timeLimitMs = 0;  // 0 = ilimitado (parar por depth apenas)
};

class MinimaxAgent : public Agent {
public:
    explicit MinimaxAgent(SearchConfig searchCfg = {}, EvaluatorConfig evalCfg = {});
    // Conveniência: cria com config padrão na profundidade dada.
    explicit MinimaxAgent(int depth) : MinimaxAgent(SearchConfig{depth}, EvaluatorConfig{}) {}

    chess::Move chooseMove(chess::Board& board) override;
    std::string name() const override;
    SearchInfo lastInfo() const override { return info_; }

    int depth() const { return search_.depth; }

private:
    int search(chess::Board& board, int depth, int alpha, int beta);
    void orderMoves(chess::MoveList& moves, const chess::Board& board);

    SearchConfig search_;
    Evaluator evaluator_;
    SearchInfo info_;

    // Timeout interno: verificado a cada 4096 nós dentro de search().
    std::chrono::steady_clock::time_point searchStart_;
    bool searchAborted_ = false;
    int nodesSinceCheck_ = 0;
};

std::unique_ptr<Agent> makeAgent(Difficulty d);
std::unique_ptr<Agent> makeAgent(const AgentSpec& spec);

}  // namespace chess3d::ai
