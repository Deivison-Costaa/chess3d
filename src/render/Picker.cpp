#include "Picker.h"

#include "core/BoardCoords.h"

#include <glm/gtc/matrix_inverse.hpp>

#include <cmath>

namespace chess3d {

std::optional<glm::vec3> rayPlaneHit(double mouseX,
                                     double mouseY,
                                     int fbWidth,
                                     int fbHeight,
                                     const glm::mat4& view,
                                     const glm::mat4& projection,
                                     float planeY) {
    if (fbWidth <= 0 || fbHeight <= 0) return std::nullopt;

    // NDC ∈ [-1, 1], y invertido pois GLFW tem origem no topo.
    const float ndcX = static_cast<float>(2.0 * mouseX / fbWidth - 1.0);
    const float ndcY = static_cast<float>(1.0 - 2.0 * mouseY / fbHeight);

    const glm::mat4 invVP = glm::inverse(projection * view);
    glm::vec4 nearH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farH  = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
    if (std::abs(nearH.w) < 1e-6f || std::abs(farH.w) < 1e-6f) return std::nullopt;

    const glm::vec3 nearW = glm::vec3(nearH) / nearH.w;
    const glm::vec3 farW  = glm::vec3(farH)  / farH.w;
    const glm::vec3 dir = farW - nearW;
    if (std::abs(dir.y) < 1e-6f) return std::nullopt;  // raio paralelo ao plano

    const float t = (planeY - nearW.y) / dir.y;
    if (t < 0.0f || t > 1.0f) return std::nullopt;  // interseção fora do segmento near→far

    return nearW + t * dir;
}

SquarePick pickSquare(double mouseX,
                      double mouseY,
                      int fbWidth,
                      int fbHeight,
                      const glm::mat4& view,
                      const glm::mat4& projection,
                      float boardTopY) {
    SquarePick out;
    auto hit = rayPlaneHit(mouseX, mouseY, fbWidth, fbHeight, view, projection, boardTopY);
    if (!hit) return out;
    int f = 0, r = 0;
    if (worldToSquare(*hit, f, r)) {
        out.file = f;
        out.rank = r;
    }
    return out;
}

}  // namespace chess3d
