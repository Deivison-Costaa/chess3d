#pragma once

#include "Easing.h"
#include "chess/Board.h"
#include "chess/Move.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace chess3d::anim {

struct VisualPiece {
    int id = -1;
    chess::PieceType type = chess::PieceType::None;
    chess::Color color = chess::Color::White;
    chess::Square square = chess::kNoSquare;  // casa lógica (depois do movimento)

    glm::vec3 worldPos{0.0f};
    float yawDeg = 0.0f;
    float pitchDeg = 0.0f;  // tombamento do rei no mate (rotação eixo X)
    float scale = 1.0f;
    float alpha = 1.0f;
    bool alive = true;
};

struct Tween {
    int pieceId = -1;
    float startDelay = 0.0f;
    float elapsed = -1.0f;
    float duration = 0.0f;

    glm::vec3 fromPos{0.0f}, toPos{0.0f};
    float arcHeight = 0.0f;
    float fromYaw = 0.0f, toYaw = 0.0f;
    float fromPitch = 0.0f, toPitch = 0.0f;
    float fromScale = 1.0f, toScale = 1.0f;
    float fromAlpha = 1.0f, toAlpha = 1.0f;

    EasingFn easing = &easeInOutCubic;

    bool removeAtEnd = false;
    chess::PieceType promoteAtEnd = chess::PieceType::None;
};

class Animator {
public:
    void initFromBoard(const chess::Board& board);

    // Aplica animação para um movimento JÁ EXECUTADO no Board.
    // `boardBefore` reflete o estado antes de makeMove (precisamos saber peça capturada,
    // peça em movimento, posições anteriores).
    void animateMove(const chess::Move& move, const chess::Board& boardBefore);
    void animateKingFall(chess::Color loserColor);

    void update(float dt);
    bool isAnimating() const;

    const std::vector<VisualPiece>& pieces() const { return pieces_; }

private:
    int findPieceAtSquare(chess::Square s) const;
    int spawnPiece(const VisualPiece& tpl);

    std::vector<VisualPiece> pieces_;
    std::vector<Tween> tweens_;
    int nextId_ = 1;
};

}  // namespace chess3d::anim
