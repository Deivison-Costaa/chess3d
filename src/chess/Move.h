#pragma once

#include "Types.h"

#include <cstdint>
#include <string>

namespace chess3d::chess {

enum class MoveFlag : std::uint8_t {
    Quiet = 0,
    DoublePawnPush,
    Capture,
    EnPassant,
    CastleKingside,
    CastleQueenside,
    Promotion,
    PromotionCapture,
};

struct Move {
    Square from = kNoSquare;
    Square to = kNoSquare;
    PieceType promotion = PieceType::None;  // só usado em flags de promoção
    MoveFlag flag = MoveFlag::Quiet;

    constexpr bool isCapture() const {
        return flag == MoveFlag::Capture
            || flag == MoveFlag::EnPassant
            || flag == MoveFlag::PromotionCapture;
    }
    constexpr bool isPromotion() const {
        return flag == MoveFlag::Promotion || flag == MoveFlag::PromotionCapture;
    }
    constexpr bool isCastle() const {
        return flag == MoveFlag::CastleKingside || flag == MoveFlag::CastleQueenside;
    }
    constexpr bool isNull() const { return from == kNoSquare; }
    constexpr bool operator==(const Move&) const = default;
};

// "e2e4", "e7e8q" — formato UCI (Long Algebraic Notation puro)
std::string moveToUci(const Move& m);

}  // namespace chess3d::chess
