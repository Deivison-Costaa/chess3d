#pragma once

#include "Board.h"
#include "Move.h"

#include <string>

namespace chess3d::chess {

// Standard Algebraic Notation. boardBefore deve ser o estado ANTES da jogada.
std::string moveToSan(const Move& move, const Board& boardBefore);

// Pequeno glifo unicode pra cada peca (usado no painel de capturas, p.ex.).
const char* pieceGlyph(Piece p);

}  // namespace chess3d::chess
