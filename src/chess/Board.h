#pragma once

#include "Move.h"
#include "Types.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace chess3d::chess {

struct UndoInfo {
    Piece captured{};
    std::uint8_t prevCastlingRights = 0;
    Square prevEnPassant = kNoSquare;
    int prevHalfmoveClock = 0;
    Move move{};
};

class Board {
public:
    Board();

    // Posição inicial.
    void reset();
    // Limpa o tabuleiro (sem peças, brancas a mover, sem direitos).
    void clear();

    // FEN
    bool loadFen(const std::string& fen);
    std::string toFen() const;

    // Acesso a peças.
    Piece pieceAt(Square s) const { return squares_[s]; }
    Piece pieceAt(int file, int rank) const { return squares_[makeSquare(file, rank)]; }
    void setPiece(Square s, Piece p) { squares_[s] = p; }

    Color sideToMove() const { return sideToMove_; }
    void setSideToMove(Color c) { sideToMove_ = c; }

    std::uint8_t castlingRights() const { return castlingRights_; }
    void setCastlingRights(std::uint8_t r) { castlingRights_ = r; }

    Square enPassantSquare() const { return enPassant_; }
    void setEnPassantSquare(Square s) { enPassant_ = s; }

    int halfmoveClock() const { return halfmoveClock_; }
    int fullmoveNumber() const { return fullmoveNumber_; }

    Square kingSquare(Color c) const { return kingSquare_[static_cast<int>(c)]; }

    // Aplica/desfaz movimento — chamador é responsável pela legalidade.
    void makeMove(const Move& m, UndoInfo& undo);
    void unmakeMove(const UndoInfo& undo);

    // Square é atacada por peça da cor `attacker`? (usado para xeque e roque)
    bool isSquareAttacked(Square s, Color attacker) const;

    bool inCheck(Color c) const { return isSquareAttacked(kingSquare(c), other(c)); }

private:
    std::array<Piece, 64> squares_{};
    std::array<Square, 2> kingSquare_{kNoSquare, kNoSquare};
    Color sideToMove_ = Color::White;
    std::uint8_t castlingRights_ = 0;
    Square enPassant_ = kNoSquare;
    int halfmoveClock_ = 0;
    int fullmoveNumber_ = 1;
};

}  // namespace chess3d::chess
