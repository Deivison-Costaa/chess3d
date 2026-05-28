#pragma once

#include <glm/glm.hpp>

#include <optional>
#include <span>

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

// Hitbox cilíndrica vertical para uma peça no tabuleiro.
struct PieceHitbox {
    int file;
    int rank;
    glm::vec3 baseCenter;  // posição no mundo do pé da peça
    float radius;          // raio em XZ
    float height;          // altura em Y
};

// Picking que prefere intersecao com cilindros (peças altas). Se nenhum cilindro
// for atingido, cai pro plano do tabuleiro. Isso evita o problema de clicar
// no corpo de um rei e o raio cair uma casa "atras" no plano.
SquarePick pickSquareWithPieces(double mouseX,
                                double mouseY,
                                int fbWidth,
                                int fbHeight,
                                const glm::mat4& view,
                                const glm::mat4& projection,
                                std::span<const PieceHitbox> hitboxes,
                                float boardTopY = 0.0f);

}  // namespace chess3d
