#include "MoveGenerator.h"

namespace chess3d::chess {

namespace {

constexpr int kKnightDeltas[8][2] = {
    { 1,  2}, { 2,  1}, { 2, -1}, { 1, -2},
    {-1, -2}, {-2, -1}, {-2,  1}, {-1,  2},
};
constexpr int kKingDeltas[8][2] = {
    { 1,  0}, { 1,  1}, { 0,  1}, {-1,  1},
    {-1,  0}, {-1, -1}, { 0, -1}, { 1, -1},
};
constexpr int kBishopDeltas[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
constexpr int kRookDeltas[4][2]   = {{1,0},{-1,0},{0,1},{0,-1}};

void addPawnMoves(const Board& b, Square from, MoveList& out) {
    const Piece p = b.pieceAt(from);
    const Color us = p.color;
    const int dir = pawnForward(us);
    const int f = fileOf(from);
    const int r = rankOf(from);
    const int promotionR = promotionRank(us);

    auto pushPawnMove = [&](Square to, MoveFlag baseFlag) {
        if (rankOf(to) == promotionR) {
            const MoveFlag promoFlag = (baseFlag == MoveFlag::Capture)
                                       ? MoveFlag::PromotionCapture : MoveFlag::Promotion;
            for (PieceType pt : {PieceType::Queen, PieceType::Rook,
                                 PieceType::Bishop, PieceType::Knight}) {
                out.push({from, to, pt, promoFlag});
            }
        } else {
            out.push({from, to, PieceType::None, baseFlag});
        }
    };

    // avanço simples
    const int r1 = r + dir;
    if (onBoard(f, r1)) {
        const Square s1 = makeSquare(f, r1);
        if (b.pieceAt(s1).empty()) {
            pushPawnMove(s1, MoveFlag::Quiet);
            // duplo
            const int startR = startRank(us);
            if (r == startR) {
                const int r2 = r + 2 * dir;
                const Square s2 = makeSquare(f, r2);
                if (b.pieceAt(s2).empty()) {
                    out.push({from, s2, PieceType::None, MoveFlag::DoublePawnPush});
                }
            }
        }
    }

    // capturas diagonais
    for (int df : {-1, +1}) {
        const int nf = f + df;
        const int nr = r + dir;
        if (!onBoard(nf, nr)) continue;
        const Square to = makeSquare(nf, nr);
        const Piece target = b.pieceAt(to);
        if (!target.empty() && target.color != us) {
            pushPawnMove(to, MoveFlag::Capture);
        }
    }

    // en passant
    if (b.enPassantSquare() != kNoSquare) {
        const Square ep = b.enPassantSquare();
        if (rankOf(ep) == r + dir && std::abs(fileOf(ep) - f) == 1) {
            out.push({from, ep, PieceType::None, MoveFlag::EnPassant});
        }
    }
}

void addLeaperMoves(const Board& b, Square from, const int deltas[][2], int count, MoveList& out) {
    const Piece p = b.pieceAt(from);
    const int f = fileOf(from);
    const int r = rankOf(from);
    for (int i = 0; i < count; ++i) {
        const int nf = f + deltas[i][0];
        const int nr = r + deltas[i][1];
        if (!onBoard(nf, nr)) continue;
        const Square to = makeSquare(nf, nr);
        const Piece target = b.pieceAt(to);
        if (target.empty()) {
            out.push({from, to, PieceType::None, MoveFlag::Quiet});
        } else if (target.color != p.color) {
            out.push({from, to, PieceType::None, MoveFlag::Capture});
        }
    }
}

void addSlidingMoves(const Board& b, Square from, const int deltas[][2], int count, MoveList& out) {
    const Piece p = b.pieceAt(from);
    const int f = fileOf(from);
    const int r = rankOf(from);
    for (int i = 0; i < count; ++i) {
        int nf = f + deltas[i][0];
        int nr = r + deltas[i][1];
        while (onBoard(nf, nr)) {
            const Square to = makeSquare(nf, nr);
            const Piece target = b.pieceAt(to);
            if (target.empty()) {
                out.push({from, to, PieceType::None, MoveFlag::Quiet});
            } else {
                if (target.color != p.color) {
                    out.push({from, to, PieceType::None, MoveFlag::Capture});
                }
                break;
            }
            nf += deltas[i][0];
            nr += deltas[i][1];
        }
    }
}

void addCastlingMoves(const Board& b, MoveList& out) {
    const Color us = b.sideToMove();
    const Color them = other(us);
    if (b.inCheck(us)) return;  // não pode rocar em xeque

    const int r = (us == Color::White) ? 0 : 7;
    const std::uint8_t rights = b.castlingRights();
    const std::uint8_t kingsideMask  = (us == Color::White) ? kCastleWK : kCastleBK;
    const std::uint8_t queensideMask = (us == Color::White) ? kCastleWQ : kCastleBQ;
    const Square kingFrom = makeSquare(4, r);

    if (rights & kingsideMask) {
        const Square f1 = makeSquare(5, r);
        const Square g1 = makeSquare(6, r);
        const Square rookSq = makeSquare(7, r);
        if (b.pieceAt(f1).empty() && b.pieceAt(g1).empty()) {
            const Piece rook = b.pieceAt(rookSq);
            if (!rook.empty() && rook.type == PieceType::Rook && rook.color == us) {
                if (!b.isSquareAttacked(f1, them) && !b.isSquareAttacked(g1, them)) {
                    out.push({kingFrom, g1, PieceType::None, MoveFlag::CastleKingside});
                }
            }
        }
    }
    if (rights & queensideMask) {
        const Square d1 = makeSquare(3, r);
        const Square c1 = makeSquare(2, r);
        const Square b1 = makeSquare(1, r);
        const Square rookSq = makeSquare(0, r);
        if (b.pieceAt(d1).empty() && b.pieceAt(c1).empty() && b.pieceAt(b1).empty()) {
            const Piece rook = b.pieceAt(rookSq);
            if (!rook.empty() && rook.type == PieceType::Rook && rook.color == us) {
                if (!b.isSquareAttacked(d1, them) && !b.isSquareAttacked(c1, them)) {
                    out.push({kingFrom, c1, PieceType::None, MoveFlag::CastleQueenside});
                }
            }
        }
    }
}

}  // namespace

void generatePseudoLegalMoves(const Board& board, MoveList& out) {
    const Color us = board.sideToMove();
    for (Square s = 0; s < 64; ++s) {
        const Piece p = board.pieceAt(s);
        if (p.empty() || p.color != us) continue;
        switch (p.type) {
            case PieceType::Pawn:   addPawnMoves(board, s, out); break;
            case PieceType::Knight: addLeaperMoves(board, s, kKnightDeltas, 8, out); break;
            case PieceType::Bishop: addSlidingMoves(board, s, kBishopDeltas, 4, out); break;
            case PieceType::Rook:   addSlidingMoves(board, s, kRookDeltas,   4, out); break;
            case PieceType::Queen:
                addSlidingMoves(board, s, kBishopDeltas, 4, out);
                addSlidingMoves(board, s, kRookDeltas,   4, out);
                break;
            case PieceType::King:   addLeaperMoves(board, s, kKingDeltas, 8, out); break;
            default: break;
        }
    }
    addCastlingMoves(board, out);
}

void generateLegalMoves(Board& board, MoveList& out) {
    out.clear();
    MoveList pseudo;
    generatePseudoLegalMoves(board, pseudo);
    const Color us = board.sideToMove();
    UndoInfo undo;
    for (const Move& m : pseudo) {
        board.makeMove(m, undo);
        if (!board.inCheck(us)) {
            out.push(m);
        }
        board.unmakeMove(undo);
    }
}

std::uint64_t perft(Board& board, int depth) {
    if (depth == 0) return 1;
    MoveList moves;
    generateLegalMoves(board, moves);
    if (depth == 1) return moves.size();
    std::uint64_t nodes = 0;
    UndoInfo undo;
    for (const Move& m : moves) {
        board.makeMove(m, undo);
        nodes += perft(board, depth - 1);
        board.unmakeMove(undo);
    }
    return nodes;
}

std::vector<PerftDivideEntry> perftDivide(Board& board, int depth) {
    std::vector<PerftDivideEntry> entries;
    if (depth <= 0) return entries;
    MoveList moves;
    generateLegalMoves(board, moves);
    UndoInfo undo;
    entries.reserve(moves.size());
    for (const Move& m : moves) {
        board.makeMove(m, undo);
        const std::uint64_t n = (depth == 1) ? 1 : perft(board, depth - 1);
        entries.push_back({m, n});
        board.unmakeMove(undo);
    }
    return entries;
}

}  // namespace chess3d::chess
