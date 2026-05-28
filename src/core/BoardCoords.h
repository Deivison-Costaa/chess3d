#pragma once

#include <glm/glm.hpp>

namespace chess3d {

inline constexpr float kSquareSize = 1.0f;
inline constexpr float kBoardHalfExtent = 4.0f;  // 8 casas / 2

inline glm::vec3 squareToWorld(int file, int rank) {
    return glm::vec3((file - 3.5f) * kSquareSize, 0.0f, (rank - 3.5f) * kSquareSize);
}

inline bool worldToSquare(const glm::vec3& world, int& outFile, int& outRank) {
    const float f = world.x / kSquareSize + 3.5f;
    const float r = world.z / kSquareSize + 3.5f;
    outFile = static_cast<int>(std::floor(f));
    outRank = static_cast<int>(std::floor(r));
    return outFile >= 0 && outFile < 8 && outRank >= 0 && outRank < 8;
}

}  // namespace chess3d
