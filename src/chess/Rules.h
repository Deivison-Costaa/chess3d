#pragma once

#include "Board.h"

#include <cstdint>
#include <vector>

namespace chess3d::chess {

enum class GameResult : std::uint8_t {
    Ongoing,
    WhiteWins,         // mate de pretas
    BlackWins,         // mate de brancas
    DrawStalemate,
    DrawFiftyMoveRule,
    DrawInsufficientMaterial,
    DrawThreefoldRepetition,
};

const char* gameResultName(GameResult r);

// As de mate/empate dependem apenas do tabuleiro atual.
bool isCheckmate(Board& board);
bool isStalemate(Board& board);
bool isInsufficientMaterial(const Board& board);
bool isFiftyMoveRule(const Board& board);

// Resultado completo, sem considerar repetição (que depende de histórico).
GameResult evaluatePosition(Board& board);

// Versão com histórico para detectar repetição tripla (vetor de FENs sem clocks).
GameResult evaluateGame(Board& board, const std::vector<std::string>& positionHistory);

// FEN reduzida para checar repetição: placement + side + castling + ep.
std::string positionKey(const Board& board);

}  // namespace chess3d::chess
