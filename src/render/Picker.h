#pragma once

#include <glm/glm.hpp>

#include <optional>

namespace chess3d {

// Converte coordenadas de mouse (em pixels, origem top-left) para um raio mundo
// e intersecta com o plano y = planeY. Retorna ponto de interseção em mundo, se houver.
std::optional<glm::vec3> rayPlaneHit(double mouseX,
                                     double mouseY,
                                     int fbWidth,
                                     int fbHeight,
                                     const glm::mat4& view,
                                     const glm::mat4& projection,
                                     float planeY);

// Atalho: pixel → (file, rank) na grade do tabuleiro. Retorna {-1,-1} se fora.
struct SquarePick {
    int file = -1;
    int rank = -1;
    bool valid() const { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }
};

SquarePick pickSquare(double mouseX,
                      double mouseY,
                      int fbWidth,
                      int fbHeight,
                      const glm::mat4& view,
                      const glm::mat4& projection,
                      float boardTopY = 0.0f);

}  // namespace chess3d
