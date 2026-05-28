#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace chess3d::chess {

enum class PieceType : std::uint8_t { None, Pawn, Knight, Bishop, Rook, Queen, King };
enum class Color : std::uint8_t { White, Black };

constexpr Color other(Color c) {
    return c == Color::White ? Color::Black : Color::White;
}

struct Piece {
    PieceType type = PieceType::None;
    Color color = Color::White;

    constexpr bool empty() const { return type == PieceType::None; }
    constexpr bool operator==(const Piece&) const = default;
};

// Square: 0..63, index = rank * 8 + file. File 0=a..7=h, rank 0..7 (rank 1..8 in chess).
using Square = std::int8_t;
constexpr Square kNoSquare = -1;

constexpr Square makeSquare(int file, int rank) { return static_cast<Square>(rank * 8 + file); }
constexpr int fileOf(Square s) { return s & 7; }
constexpr int rankOf(Square s) { return s >> 3; }
constexpr bool onBoard(int file, int rank) {
    return file >= 0 && file < 8 && rank >= 0 && rank < 8;
}

inline std::string squareName(Square s) {
    if (s < 0 || s >= 64) return "-";
    std::string out;
    out += static_cast<char>('a' + fileOf(s));
    out += static_cast<char>('1' + rankOf(s));
    return out;
}

inline Square parseSquare(const std::string& s) {
    if (s.size() < 2) return kNoSquare;
    const int f = s[0] - 'a';
    const int r = s[1] - '1';
    if (!onBoard(f, r)) return kNoSquare;
    return makeSquare(f, r);
}

// Direção de avanço do peão (rank delta) por cor.
constexpr int pawnForward(Color c) { return c == Color::White ? +1 : -1; }
constexpr int promotionRank(Color c) { return c == Color::White ? 7 : 0; }
constexpr int startRank(Color c) { return c == Color::White ? 1 : 6; }

// Direitos de roque — bitmask de 4 bits.
constexpr std::uint8_t kCastleWK = 1 << 0;  // white kingside
constexpr std::uint8_t kCastleWQ = 1 << 1;  // white queenside
constexpr std::uint8_t kCastleBK = 1 << 2;  // black kingside
constexpr std::uint8_t kCastleBQ = 1 << 3;  // black queenside

}  // namespace chess3d::chess
