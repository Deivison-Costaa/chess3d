#include "Move.h"

namespace chess3d::chess {

std::string moveToUci(const Move& m) {
    if (m.isNull()) return "0000";
    std::string s = squareName(m.from) + squareName(m.to);
    if (m.isPromotion()) {
        switch (m.promotion) {
            case PieceType::Queen:  s += 'q'; break;
            case PieceType::Rook:   s += 'r'; break;
            case PieceType::Bishop: s += 'b'; break;
            case PieceType::Knight: s += 'n'; break;
            default: break;
        }
    }
    return s;
}

}  // namespace chess3d::chess
