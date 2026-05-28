#pragma once

#include "Agent.h"
#include "DifficultyLevels.h"
#include "Evaluator.h"
#include "chess/MoveGenerator.h"

namespace chess3d::ai {

struct SearchConfig {
    int depth = 2;
    bool useAlphaBeta = true;
    bool useMoveOrdering = false;  // MVV-LVA
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
};

std::unique_ptr<Agent> makeAgent(Difficulty d);

}  // namespace chess3d::ai
