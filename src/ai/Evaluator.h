#pragma once

#include "chess/Board.h"

namespace chess3d::ai {

constexpr int kPawnValue   = 100;
constexpr int kKnightValue = 320;
constexpr int kBishopValue = 330;
constexpr int kRookValue   = 500;
constexpr int kQueenValue  = 900;
constexpr int kKingValue   = 20000;

constexpr int pieceValue(chess::PieceType t) {
    switch (t) {
        case chess::PieceType::Pawn:   return kPawnValue;
        case chess::PieceType::Knight: return kKnightValue;
        case chess::PieceType::Bishop: return kBishopValue;
        case chess::PieceType::Rook:   return kRookValue;
        case chess::PieceType::Queen:  return kQueenValue;
        case chess::PieceType::King:   return kKingValue;
        default: return 0;
    }
}

struct EvaluatorConfig {
    bool usePsts = false;
    bool useMobility = false;
};

class Evaluator {
public:
    explicit Evaluator(EvaluatorConfig cfg = {}) : cfg_(cfg) {}

    // Avaliação em centipawns no POV do lado a mover.
    // Positivo = quem está a mover está melhor.
    int evaluate(const chess::Board& board) const;

private:
    int materialBalance(const chess::Board& board) const;

    EvaluatorConfig cfg_;
};

}  // namespace chess3d::ai
