#pragma once

#include "Agent.h"
#include "Evaluator.h"

namespace chess3d::ai {

class MinimaxAgent : public Agent {
public:
    explicit MinimaxAgent(int depth = 2, EvaluatorConfig evalCfg = {});

    chess::Move chooseMove(chess::Board& board) override;
    std::string name() const override;
    SearchInfo lastInfo() const override { return info_; }

    int depth() const { return depth_; }

private:
    // Negamax simples (sem alpha-beta — adicionado na Fase 8).
    int negamax(chess::Board& board, int depth);

    int depth_;
    Evaluator evaluator_;
    SearchInfo info_;
};

}  // namespace chess3d::ai
