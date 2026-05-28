#pragma once

#include "chess/Board.h"
#include "chess/Move.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace chess3d::ai {

struct SearchInfo {
    int depthReached = 0;
    std::uint64_t nodesVisited = 0;
    int evaluation = 0;  // em centipawns, POV do lado a mover
    std::chrono::microseconds elapsed{0};
    std::vector<chess::Move> principalVariation;  // melhor sequência prevista
};

class Agent {
public:
    virtual ~Agent() = default;
    virtual chess::Move chooseMove(chess::Board& board) = 0;
    virtual std::string name() const = 0;
    virtual SearchInfo lastInfo() const { return {}; }
};

}  // namespace chess3d::ai
