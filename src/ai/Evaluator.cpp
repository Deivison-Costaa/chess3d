#include "Evaluator.h"

namespace chess3d::ai {

int Evaluator::materialBalance(const chess::Board& board) const {
    int score = 0;
    for (chess::Square s = 0; s < 64; ++s) {
        const chess::Piece p = board.pieceAt(s);
        if (p.empty()) continue;
        const int v = pieceValue(p.type);
        score += (p.color == chess::Color::White) ? v : -v;
    }
    return score;
}

int Evaluator::evaluate(const chess::Board& board) const {
    // Por enquanto: só material. PSTs + mobilidade ficam pra Fase 8.
    int score = materialBalance(board);
    // POV do lado a mover.
    return board.sideToMove() == chess::Color::White ? score : -score;
}

}  // namespace chess3d::ai
