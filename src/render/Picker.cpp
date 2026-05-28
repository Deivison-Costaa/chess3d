#include "Picker.h"

#include "core/BoardCoords.h"

#include <glm/gtc/matrix_inverse.hpp>

#include <cmath>
#include <limits>

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

namespace {

// Constrói raio mundo (origem + direção normalizada).
struct WorldRay { glm::vec3 origin; glm::vec3 dir; };

std::optional<WorldRay> buildRay(double mouseX, double mouseY, int fbW, int fbH,
                                  const glm::mat4& view, const glm::mat4& proj) {
    if (fbW <= 0 || fbH <= 0) return std::nullopt;
    const float ndcX = static_cast<float>(2.0 * mouseX / fbW - 1.0);
    const float ndcY = static_cast<float>(1.0 - 2.0 * mouseY / fbH);
    const glm::mat4 invVP = glm::inverse(proj * view);
    glm::vec4 nH = invVP * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 fH = invVP * glm::vec4(ndcX, ndcY,  1.0f, 1.0f);
    if (std::abs(nH.w) < 1e-6f || std::abs(fH.w) < 1e-6f) return std::nullopt;
    const glm::vec3 nW = glm::vec3(nH) / nH.w;
    const glm::vec3 fW = glm::vec3(fH) / fH.w;
    return WorldRay{nW, glm::normalize(fW - nW)};
}

// Intersecção raio-cilindro vertical. Retorna t (na escala da direção normalizada).
std::optional<float> rayCylinderHit(const WorldRay& ray,
                                     const glm::vec3& base,
                                     float radius,
                                     float height) {
    // Equação no plano XZ
    const float dx = ray.dir.x;
    const float dz = ray.dir.z;
    const float ox = ray.origin.x - base.x;
    const float oz = ray.origin.z - base.z;
    const float a = dx*dx + dz*dz;
    if (a < 1e-8f) return std::nullopt;  // raio quase vertical
    const float b = 2.0f * (ox*dx + oz*dz);
    const float c = ox*ox + oz*oz - radius*radius;
    const float disc = b*b - 4.0f*a*c;
    if (disc < 0.0f) return std::nullopt;
    const float sd = std::sqrt(disc);
    const float t1 = (-b - sd) / (2.0f * a);
    const float t2 = (-b + sd) / (2.0f * a);
    for (float t : {t1, t2}) {
        if (t < 0.0f) continue;
        const float y = ray.origin.y + t * ray.dir.y;
        if (y >= base.y && y <= base.y + height) return t;
    }
    return std::nullopt;
}

}  // namespace

SquarePick pickSquareWithPieces(double mouseX,
                                double mouseY,
                                int fbWidth,
                                int fbHeight,
                                const glm::mat4& view,
                                const glm::mat4& projection,
                                std::span<const PieceHitbox> hitboxes,
                                float boardTopY) {
    SquarePick out;
    auto ray = buildRay(mouseX, mouseY, fbWidth, fbHeight, view, projection);
    if (!ray) return out;

    // Testa cilindros — pega o de menor t (mais perto da camera).
    float bestT = std::numeric_limits<float>::max();
    const PieceHitbox* bestHit = nullptr;
    for (const auto& hb : hitboxes) {
        if (auto t = rayCylinderHit(*ray, hb.baseCenter, hb.radius, hb.height)) {
            if (*t < bestT) {
                bestT = *t;
                bestHit = &hb;
            }
        }
    }

    if (bestHit) {
        out.file = bestHit->file;
        out.rank = bestHit->rank;
        return out;
    }

    // Sem peça atingida — fallback no plano do tabuleiro.
    return pickSquare(mouseX, mouseY, fbWidth, fbHeight, view, projection, boardTopY);
}

}  // namespace chess3d
