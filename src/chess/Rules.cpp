#include "Rules.h"

#include "MoveGenerator.h"

#include <sstream>

namespace chess3d::chess {

const char* gameResultName(GameResult r) {
    switch (r) {
        case GameResult::Ongoing: return "ongoing";
        case GameResult::WhiteWins: return "1-0 (mate)";
        case GameResult::BlackWins: return "0-1 (mate)";
        case GameResult::DrawStalemate: return "1/2-1/2 (stalemate)";
        case GameResult::DrawFiftyMoveRule: return "1/2-1/2 (50 lances)";
        case GameResult::DrawInsufficientMaterial: return "1/2-1/2 (material insuficiente)";
        case GameResult::DrawThreefoldRepetition: return "1/2-1/2 (repeticao tripla)";
    }
    return "?";
}

bool isCheckmate(Board& board) {
    MoveList legal;
    generateLegalMoves(board, legal);
    return legal.size() == 0 && board.inCheck(board.sideToMove());
}

bool isStalemate(Board& board) {
    MoveList legal;
    generateLegalMoves(board, legal);
    return legal.size() == 0 && !board.inCheck(board.sideToMove());
}

bool isFiftyMoveRule(const Board& board) {
    return board.halfmoveClock() >= 100;  // 50 lances = 100 ply
}

bool isInsufficientMaterial(const Board& board) {
    // Conta as peças que ainda têm poder de mate.
    int whiteMinors = 0, blackMinors = 0;
    int whiteBishopColor = -1, blackBishopColor = -1;  // 0=quadrado claro, 1=escuro
    for (Square s = 0; s < 64; ++s) {
        const Piece p = board.pieceAt(s);
        if (p.empty() || p.type == PieceType::King) continue;
        // Qualquer peão, torre ou rainha: material suficiente.
        if (p.type == PieceType::Pawn || p.type == PieceType::Rook || p.type == PieceType::Queen) {
            return false;
        }
        const int sqColor = ((fileOf(s) + rankOf(s)) & 1);  // 0=escuro, 1=claro
        if (p.type == PieceType::Bishop || p.type == PieceType::Knight) {
            if (p.color == Color::White) {
                ++whiteMinors;
                if (p.type == PieceType::Bishop) whiteBishopColor = sqColor;
            } else {
                ++blackMinors;
                if (p.type == PieceType::Bishop) blackBishopColor = sqColor;
            }
        }
    }

    // K vs K
    if (whiteMinors == 0 && blackMinors == 0) return true;
    // K + B/N vs K
    if ((whiteMinors == 1 && blackMinors == 0) || (whiteMinors == 0 && blackMinors == 1)) return true;
    // K + B vs K + B (bispos do mesmo cor de casa)
    if (whiteMinors == 1 && blackMinors == 1 &&
        whiteBishopColor != -1 && blackBishopColor != -1 &&
        whiteBishopColor == blackBishopColor) {
        return true;
    }
    return false;
}

std::string positionKey(const Board& board) {
    // Versão reduzida da FEN: placement + side + castling + en passant.
    std::string fen = board.toFen();
    // Trunca após os 4 primeiros tokens (descarta halfmove + fullmove).
    std::istringstream is(fen);
    std::string placement, side, castling, ep;
    is >> placement >> side >> castling >> ep;
    return placement + " " + side + " " + castling + " " + ep;
}

GameResult evaluatePosition(Board& board) {
    MoveList legal;
    generateLegalMoves(board, legal);
    if (legal.size() == 0) {
        if (board.inCheck(board.sideToMove())) {
            return board.sideToMove() == Color::White
                       ? GameResult::BlackWins
                       : GameResult::WhiteWins;
        }
        return GameResult::DrawStalemate;
    }
    if (isFiftyMoveRule(board)) return GameResult::DrawFiftyMoveRule;
    if (isInsufficientMaterial(board)) return GameResult::DrawInsufficientMaterial;
    return GameResult::Ongoing;
}

GameResult evaluateGame(Board& board, const std::vector<std::string>& history) {
    if (auto r = evaluatePosition(board); r != GameResult::Ongoing) return r;
    // Repetição tripla.
    const std::string current = positionKey(board);
    int repeats = 0;
    for (const auto& past : history) {
        if (past == current) {
            ++repeats;
            if (repeats >= 2) return GameResult::DrawThreefoldRepetition;
        }
    }
    return GameResult::Ongoing;
}

}  // namespace chess3d::chess
