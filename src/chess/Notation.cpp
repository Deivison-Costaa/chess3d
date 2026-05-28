#include "Notation.h"

#include "MoveGenerator.h"
#include "Rules.h"

namespace chess3d::chess {

namespace {

char pieceLetter(PieceType t) {
    switch (t) {
        case PieceType::Knight: return 'N';
        case PieceType::Bishop: return 'B';
        case PieceType::Rook:   return 'R';
        case PieceType::Queen:  return 'Q';
        case PieceType::King:   return 'K';
        default: return ' ';  // peão não é escrito
    }
}

}  // namespace

const char* pieceGlyph(Piece p) {
    // Letras FEN: maiusculas = brancas, minusculas = pretas.
    // (A fonte padrao do ImGui nao tem os glifos unicode ♔♕♖.)
    if (p.empty()) return " ";
    if (p.color == Color::White) {
        switch (p.type) {
            case PieceType::King:   return "K";
            case PieceType::Queen:  return "Q";
            case PieceType::Rook:   return "R";
            case PieceType::Bishop: return "B";
            case PieceType::Knight: return "N";
            case PieceType::Pawn:   return "P";
            default: return " ";
        }
    }
    switch (p.type) {
        case PieceType::King:   return "k";
        case PieceType::Queen:  return "q";
        case PieceType::Rook:   return "r";
        case PieceType::Bishop: return "b";
        case PieceType::Knight: return "n";
        case PieceType::Pawn:   return "p";
        default: return " ";
    }
}

std::string moveToSan(const Move& move, const Board& boardBefore) {
    if (move.isNull()) return "(null)";

    // Roque
    if (move.flag == MoveFlag::CastleKingside)  return "O-O";
    if (move.flag == MoveFlag::CastleQueenside) return "O-O-O";

    const Piece mover = boardBefore.pieceAt(move.from);
    const bool isPawn = (mover.type == PieceType::Pawn);
    const bool capture = move.isCapture();

    std::string san;

    if (isPawn) {
        if (capture) {
            san += static_cast<char>('a' + fileOf(move.from));
        }
    } else {
        san += pieceLetter(mover.type);

        // Disambiguação: outras peças do mesmo tipo/cor que também alcançam move.to
        Board copy = boardBefore;
        MoveList legal;
        generateLegalMoves(copy, legal);
        bool sameFile = false;
        bool sameRank = false;
        bool needAny = false;
        for (const auto& m : legal) {
            if (m.from == move.from) continue;
            if (m.to != move.to) continue;
            const Piece p = boardBefore.pieceAt(m.from);
            if (p.type != mover.type) continue;
            needAny = true;
            if (fileOf(m.from) == fileOf(move.from)) sameFile = true;
            if (rankOf(m.from) == rankOf(move.from)) sameRank = true;
        }
        if (needAny) {
            if (!sameFile) {
                san += static_cast<char>('a' + fileOf(move.from));
            } else if (!sameRank) {
                san += static_cast<char>('1' + rankOf(move.from));
            } else {
                san += static_cast<char>('a' + fileOf(move.from));
                san += static_cast<char>('1' + rankOf(move.from));
            }
        }
    }

    if (capture) san += 'x';
    san += squareName(move.to);

    if (move.isPromotion()) {
        san += '=';
        san += pieceLetter(move.promotion);
    }

    // Verifica + ou # aplicando o lance temporariamente
    Board copy = boardBefore;
    UndoInfo undo;
    copy.makeMove(move, undo);
    if (copy.inCheck(copy.sideToMove())) {
        if (isCheckmate(copy)) san += '#';
        else                   san += '+';
    }
    return san;
}

}  // namespace chess3d::chess
