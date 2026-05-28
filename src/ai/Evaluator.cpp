#include "Evaluator.h"

#include "PieceSquareTables.h"
#include "chess/MoveGenerator.h"

namespace chess3d::ai {

namespace {

// Limiar de "endgame" — quando o material total (excluindo reis) cai abaixo
// disso, usamos a tabela de rei de final em vez da de meio-jogo.
constexpr int kEndgameMaterialThreshold = 2 * (kQueenValue + 2 * kRookValue);

const PstTable& pstFor(chess::PieceType type, bool endgame) {
    switch (type) {
        case chess::PieceType::Pawn:   return kPawnPst;
        case chess::PieceType::Knight: return kKnightPst;
        case chess::PieceType::Bishop: return kBishopPst;
        case chess::PieceType::Rook:   return kRookPst;
        case chess::PieceType::Queen:  return kQueenPst;
        case chess::PieceType::King:   return endgame ? kKingEndPst : kKingMidPst;
        default:                       return kPawnPst;  // jamais usado
    }
}

}  // namespace

int Evaluator::materialBalance(const chess::Board& board) const {
    int materialAndPst = 0;
    int totalNonKingMaterial = 0;

    if (cfg_.usePsts) {
        // Primeira passada: descobre se estamos em final pra escolher tabela do rei.
        for (chess::Square s = 0; s < 64; ++s) {
            const chess::Piece p = board.pieceAt(s);
            if (p.empty() || p.type == chess::PieceType::King) continue;
            totalNonKingMaterial += pieceValue(p.type);
        }
    }
    const bool endgame = cfg_.usePsts && totalNonKingMaterial <= kEndgameMaterialThreshold;

    for (chess::Square s = 0; s < 64; ++s) {
        const chess::Piece p = board.pieceAt(s);
        if (p.empty()) continue;
        int v = pieceValue(p.type);
        if (cfg_.usePsts) {
            v += pstLookup(pstFor(p.type, endgame), p.color, s);
        }
        materialAndPst += (p.color == chess::Color::White) ? v : -v;
    }
    return materialAndPst;
}

int Evaluator::evaluate(const chess::Board& board) const {
    int score = materialBalance(board);

    if (cfg_.useMobility) {
        // Diferença de movimentos pseudo-legais — barato. ~3 cp por movimento.
        chess::Board copy = board;
        chess::MoveList whiteMoves, blackMoves;
        if (board.sideToMove() == chess::Color::White) {
            chess::generatePseudoLegalMoves(copy, whiteMoves);
            copy.setSideToMove(chess::Color::Black);
            chess::generatePseudoLegalMoves(copy, blackMoves);
        } else {
            chess::generatePseudoLegalMoves(copy, blackMoves);
            copy.setSideToMove(chess::Color::White);
            chess::generatePseudoLegalMoves(copy, whiteMoves);
        }
        score += 3 * (static_cast<int>(whiteMoves.size()) - static_cast<int>(blackMoves.size()));
    }

    return board.sideToMove() == chess::Color::White ? score : -score;
}

}  // namespace chess3d::ai
