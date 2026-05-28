#include "Board.h"

#include <cctype>
#include <cstring>
#include <sstream>

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

char pieceChar(Piece p) {
    if (p.empty()) return '.';
    char c = '?';
    switch (p.type) {
        case PieceType::Pawn:   c = 'p'; break;
        case PieceType::Knight: c = 'n'; break;
        case PieceType::Bishop: c = 'b'; break;
        case PieceType::Rook:   c = 'r'; break;
        case PieceType::Queen:  c = 'q'; break;
        case PieceType::King:   c = 'k'; break;
        default: return '.';
    }
    return p.color == Color::White ? static_cast<char>(std::toupper(c)) : c;
}

std::optional<Piece> parseFenChar(char c) {
    Piece p;
    p.color = std::isupper(static_cast<unsigned char>(c)) ? Color::White : Color::Black;
    switch (std::tolower(static_cast<unsigned char>(c))) {
        case 'p': p.type = PieceType::Pawn;   return p;
        case 'n': p.type = PieceType::Knight; return p;
        case 'b': p.type = PieceType::Bishop; return p;
        case 'r': p.type = PieceType::Rook;   return p;
        case 'q': p.type = PieceType::Queen;  return p;
        case 'k': p.type = PieceType::King;   return p;
        default: return std::nullopt;
    }
}

}  // namespace

Board::Board() { reset(); }

void Board::clear() {
    squares_.fill(Piece{});
    kingSquare_ = {kNoSquare, kNoSquare};
    sideToMove_ = Color::White;
    castlingRights_ = 0;
    enPassant_ = kNoSquare;
    halfmoveClock_ = 0;
    fullmoveNumber_ = 1;
}

void Board::reset() {
    loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

bool Board::loadFen(const std::string& fen) {
    clear();
    std::istringstream is(fen);
    std::string placement, side, castling, ep, halfStr, fullStr;
    if (!(is >> placement >> side >> castling >> ep)) return false;
    is >> halfStr >> fullStr;  // opcionais

    // Placement: ranks de cima (8) para baixo (1).
    int rank = 7;
    int file = 0;
    for (char c : placement) {
        if (c == '/') {
            if (file != 8) return false;
            --rank;
            file = 0;
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            file += c - '0';
        } else {
            if (file > 7 || rank < 0) return false;
            auto pieceOpt = parseFenChar(c);
            if (!pieceOpt) return false;
            const Square sq = makeSquare(file, rank);
            squares_[sq] = *pieceOpt;
            if (pieceOpt->type == PieceType::King) {
                kingSquare_[static_cast<int>(pieceOpt->color)] = sq;
            }
            ++file;
        }
    }

    sideToMove_ = (side == "w") ? Color::White : Color::Black;

    castlingRights_ = 0;
    if (castling != "-") {
        for (char c : castling) {
            switch (c) {
                case 'K': castlingRights_ |= kCastleWK; break;
                case 'Q': castlingRights_ |= kCastleWQ; break;
                case 'k': castlingRights_ |= kCastleBK; break;
                case 'q': castlingRights_ |= kCastleBQ; break;
                default: break;
            }
        }
    }

    enPassant_ = (ep == "-") ? kNoSquare : parseSquare(ep);
    halfmoveClock_ = halfStr.empty() ? 0 : std::stoi(halfStr);
    fullmoveNumber_ = fullStr.empty() ? 1 : std::stoi(fullStr);
    return true;
}

std::string Board::toFen() const {
    std::string out;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const Piece p = pieceAt(file, rank);
            if (p.empty()) {
                ++empty;
            } else {
                if (empty) { out += std::to_string(empty); empty = 0; }
                out += pieceChar(p);
            }
        }
        if (empty) out += std::to_string(empty);
        if (rank > 0) out += '/';
    }
    out += sideToMove_ == Color::White ? " w " : " b ";
    std::string rights;
    if (castlingRights_ & kCastleWK) rights += 'K';
    if (castlingRights_ & kCastleWQ) rights += 'Q';
    if (castlingRights_ & kCastleBK) rights += 'k';
    if (castlingRights_ & kCastleBQ) rights += 'q';
    out += rights.empty() ? "-" : rights;
    out += ' ';
    out += (enPassant_ == kNoSquare) ? "-" : squareName(enPassant_);
    out += ' ';
    out += std::to_string(halfmoveClock_);
    out += ' ';
    out += std::to_string(fullmoveNumber_);
    return out;
}

bool Board::isSquareAttacked(Square s, Color attacker) const {
    if (s < 0 || s >= 64) return false;
    const int f = fileOf(s);
    const int r = rankOf(s);

    // Pawns: atacam diagonais "para frente" no sentido da cor atacante.
    const int dir = pawnForward(attacker);
    for (int df : {-1, +1}) {
        const int af = f + df;
        const int ar = r - dir;  // peão está "atrás" do alvo no sentido do seu avanço
        if (onBoard(af, ar)) {
            const Piece p = pieceAt(af, ar);
            if (!p.empty() && p.color == attacker && p.type == PieceType::Pawn) return true;
        }
    }

    // Knights
    for (const auto& d : kKnightDeltas) {
        const int nf = f + d[0];
        const int nr = r + d[1];
        if (!onBoard(nf, nr)) continue;
        const Piece p = pieceAt(nf, nr);
        if (!p.empty() && p.color == attacker && p.type == PieceType::Knight) return true;
    }

    // King (adjacente)
    for (const auto& d : kKingDeltas) {
        const int nf = f + d[0];
        const int nr = r + d[1];
        if (!onBoard(nf, nr)) continue;
        const Piece p = pieceAt(nf, nr);
        if (!p.empty() && p.color == attacker && p.type == PieceType::King) return true;
    }

    // Bishop/Queen — diagonais
    for (const auto& d : kBishopDeltas) {
        int nf = f + d[0], nr = r + d[1];
        while (onBoard(nf, nr)) {
            const Piece p = pieceAt(nf, nr);
            if (!p.empty()) {
                if (p.color == attacker && (p.type == PieceType::Bishop || p.type == PieceType::Queen)) {
                    return true;
                }
                break;
            }
            nf += d[0]; nr += d[1];
        }
    }

    // Rook/Queen — ortogonais
    for (const auto& d : kRookDeltas) {
        int nf = f + d[0], nr = r + d[1];
        while (onBoard(nf, nr)) {
            const Piece p = pieceAt(nf, nr);
            if (!p.empty()) {
                if (p.color == attacker && (p.type == PieceType::Rook || p.type == PieceType::Queen)) {
                    return true;
                }
                break;
            }
            nf += d[0]; nr += d[1];
        }
    }

    return false;
}

void Board::makeMove(const Move& m, UndoInfo& undo) {
    undo.move = m;
    undo.prevCastlingRights = castlingRights_;
    undo.prevEnPassant = enPassant_;
    undo.prevHalfmoveClock = halfmoveClock_;
    undo.captured = Piece{};

    const Piece mover = squares_[m.from];
    const Color us = mover.color;
    const Color them = other(us);

    const bool isCapture = (m.flag == MoveFlag::Capture)
                        || (m.flag == MoveFlag::EnPassant)
                        || (m.flag == MoveFlag::PromotionCapture);

    // Halfmove clock: reset em captura ou movimento de peão; senão incrementa.
    if (mover.type == PieceType::Pawn || isCapture) {
        halfmoveClock_ = 0;
    } else {
        ++halfmoveClock_;
    }

    // Captura "normal"
    if (m.flag == MoveFlag::Capture || m.flag == MoveFlag::PromotionCapture) {
        undo.captured = squares_[m.to];
    }

    // En passant: peça capturada está atrás do destino
    if (m.flag == MoveFlag::EnPassant) {
        const int dir = pawnForward(us);
        const Square capSq = makeSquare(fileOf(m.to), rankOf(m.to) - dir);
        undo.captured = squares_[capSq];
        squares_[capSq] = Piece{};
    }

    // Move da peça principal
    squares_[m.to] = mover;
    squares_[m.from] = Piece{};

    // Promoção
    if (m.flag == MoveFlag::Promotion || m.flag == MoveFlag::PromotionCapture) {
        squares_[m.to] = Piece{m.promotion, us};
    }

    // Roque: move a torre também
    if (m.flag == MoveFlag::CastleKingside) {
        const int r = rankOf(m.to);
        const Square rookFrom = makeSquare(7, r);
        const Square rookTo   = makeSquare(5, r);
        squares_[rookTo] = squares_[rookFrom];
        squares_[rookFrom] = Piece{};
    } else if (m.flag == MoveFlag::CastleQueenside) {
        const int r = rankOf(m.to);
        const Square rookFrom = makeSquare(0, r);
        const Square rookTo   = makeSquare(3, r);
        squares_[rookTo] = squares_[rookFrom];
        squares_[rookFrom] = Piece{};
    }

    // Atualiza posição do rei
    if (mover.type == PieceType::King) {
        kingSquare_[static_cast<int>(us)] = m.to;
    }

    // Atualiza direitos de roque
    if (mover.type == PieceType::King) {
        if (us == Color::White) castlingRights_ &= ~(kCastleWK | kCastleWQ);
        else                    castlingRights_ &= ~(kCastleBK | kCastleBQ);
    }
    // Se a torre saiu de sua casa inicial OU foi capturada
    auto clearRookRights = [&](Square sq) {
        switch (sq) {
            case 0:  castlingRights_ &= ~kCastleWQ; break;  // a1
            case 7:  castlingRights_ &= ~kCastleWK; break;  // h1
            case 56: castlingRights_ &= ~kCastleBQ; break;  // a8
            case 63: castlingRights_ &= ~kCastleBK; break;  // h8
            default: break;
        }
    };
    clearRookRights(m.from);
    clearRookRights(m.to);

    // En passant target square (apenas em double pawn push)
    if (m.flag == MoveFlag::DoublePawnPush) {
        const int dir = pawnForward(us);
        enPassant_ = makeSquare(fileOf(m.from), rankOf(m.from) + dir);
    } else {
        enPassant_ = kNoSquare;
    }

    // Turno + número de lance
    if (us == Color::Black) ++fullmoveNumber_;
    sideToMove_ = them;
    (void)them;  // silencia warning quando otimização remove uso
}

void Board::unmakeMove(const UndoInfo& undo) {
    const Move m = undo.move;
    // O lado que jogou era o oposto do atual
    sideToMove_ = other(sideToMove_);
    if (sideToMove_ == Color::Black) --fullmoveNumber_;

    const Color us = sideToMove_;
    const Color them = other(us);
    (void)them;

    castlingRights_ = undo.prevCastlingRights;
    enPassant_ = undo.prevEnPassant;
    halfmoveClock_ = undo.prevHalfmoveClock;

    // Promoção: a peça em m.to é a promovida; ao desfazer, restaurar como peão.
    Piece mover = squares_[m.to];
    if (m.flag == MoveFlag::Promotion || m.flag == MoveFlag::PromotionCapture) {
        mover = Piece{PieceType::Pawn, us};
    }
    squares_[m.from] = mover;
    squares_[m.to] = Piece{};

    // Restaura captura normal
    if (m.flag == MoveFlag::Capture || m.flag == MoveFlag::PromotionCapture) {
        squares_[m.to] = undo.captured;
    }

    // Restaura captura en passant
    if (m.flag == MoveFlag::EnPassant) {
        const int dir = pawnForward(us);
        const Square capSq = makeSquare(fileOf(m.to), rankOf(m.to) - dir);
        squares_[capSq] = undo.captured;
        squares_[m.to] = Piece{};  // já estava vazio antes do en passant
    }

    // Desfaz movimento da torre no roque
    if (m.flag == MoveFlag::CastleKingside) {
        const int r = rankOf(m.to);
        const Square rookFrom = makeSquare(7, r);
        const Square rookTo   = makeSquare(5, r);
        squares_[rookFrom] = squares_[rookTo];
        squares_[rookTo] = Piece{};
    } else if (m.flag == MoveFlag::CastleQueenside) {
        const int r = rankOf(m.to);
        const Square rookFrom = makeSquare(0, r);
        const Square rookTo   = makeSquare(3, r);
        squares_[rookFrom] = squares_[rookTo];
        squares_[rookTo] = Piece{};
    }

    // Restaura posição do rei
    if (mover.type == PieceType::King) {
        kingSquare_[static_cast<int>(us)] = m.from;
    }
}

}  // namespace chess3d::chess
