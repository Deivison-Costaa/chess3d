#include "Animator.h"

#include "core/BoardCoords.h"

#include <algorithm>
#include <cmath>

namespace chess3d::anim {

namespace {

constexpr float kMoveDuration   = 0.40f;
constexpr float kKnightDuration = 0.55f;
constexpr float kCaptureFade    = 0.30f;
constexpr float kCastleRookDelay = 0.10f;
constexpr float kKnightArcHeight = 0.7f;  // multiplica pela distância

float yawForColor(chess::Color c) {
    return c == chess::Color::Black ? 180.0f : 0.0f;
}

}  // namespace

void Animator::initFromBoard(const chess::Board& board) {
    pieces_.clear();
    tweens_.clear();
    nextId_ = 1;

    for (chess::Square s = 0; s < 64; ++s) {
        const chess::Piece p = board.pieceAt(s);
        if (p.empty()) continue;
        VisualPiece v;
        v.id = nextId_++;
        v.type = p.type;
        v.color = p.color;
        v.square = s;
        v.worldPos = squareToWorld(chess::fileOf(s), chess::rankOf(s));
        v.yawDeg = yawForColor(p.color);
        v.scale = 1.0f;
        v.alpha = 1.0f;
        v.alive = true;
        pieces_.push_back(v);
    }
}

int Animator::findPieceAtSquare(chess::Square s) const {
    for (std::size_t i = 0; i < pieces_.size(); ++i) {
        if (pieces_[i].alive && pieces_[i].square == s) return static_cast<int>(i);
    }
    return -1;
}

void Animator::animateMove(const chess::Move& move, const chess::Board& boardBefore) {
    const chess::Piece movingPiece = boardBefore.pieceAt(move.from);
    if (movingPiece.empty()) return;
    const chess::Color us = movingPiece.color;

    const int movingIdx = findPieceAtSquare(move.from);
    if (movingIdx < 0) return;

    // 1) Captura: identifica peça capturada e cria tween de fade-out + scale.
    chess::Square captureSquare = chess::kNoSquare;
    if (move.flag == chess::MoveFlag::Capture
        || move.flag == chess::MoveFlag::PromotionCapture) {
        captureSquare = move.to;
    } else if (move.flag == chess::MoveFlag::EnPassant) {
        // peão capturado está "atrás" do destino no sentido de avanço
        const int dir = chess::pawnForward(us);
        captureSquare = chess::makeSquare(chess::fileOf(move.to),
                                          chess::rankOf(move.to) - dir);
    }
    if (captureSquare != chess::kNoSquare) {
        const int capIdx = findPieceAtSquare(captureSquare);
        if (capIdx >= 0) {
            VisualPiece& cap = pieces_[capIdx];
            // tira a peça capturada do "tabuleiro lógico" de imediato pra não interferir.
            cap.square = chess::kNoSquare;
            Tween t;
            t.pieceId = cap.id;
            t.startDelay = std::max(0.0f, kMoveDuration - kCaptureFade);
            t.duration = kCaptureFade;
            t.fromPos = cap.worldPos;
            t.toPos = cap.worldPos;
            t.fromScale = 1.0f;
            t.toScale = 1.15f;
            t.fromAlpha = 1.0f;
            t.toAlpha = 0.0f;
            t.easing = &easeOutCubic;
            t.removeAtEnd = true;
            tweens_.push_back(t);
        }
    }

    // 2) Movimento da peça principal (cavalo em arco, demais em slide ease-in-out).
    {
        VisualPiece& v = pieces_[movingIdx];
        v.square = move.to;

        Tween t;
        t.pieceId = v.id;
        t.fromPos = v.worldPos;
        t.toPos = squareToWorld(chess::fileOf(move.to), chess::rankOf(move.to));
        t.fromYaw = v.yawDeg;
        t.toYaw = v.yawDeg;
        if (movingPiece.type == chess::PieceType::Knight) {
            const float dist = glm::length(t.toPos - t.fromPos);
            t.arcHeight = kKnightArcHeight * dist;
            t.duration = kKnightDuration;
            t.easing = &easeOutQuad;
            t.toYaw = v.yawDeg + 25.0f;  // rotação leve durante o pulo
        } else {
            t.duration = kMoveDuration;
            t.easing = &easeInOutCubic;
        }
        if (move.isPromotion()) {
            t.promoteAtEnd = move.promotion;
        }
        tweens_.push_back(t);
    }

    // 3) Roque: a torre acompanha com pequeno delay.
    if (move.flag == chess::MoveFlag::CastleKingside
        || move.flag == chess::MoveFlag::CastleQueenside) {
        const int r = chess::rankOf(move.to);
        chess::Square rookFrom, rookTo;
        if (move.flag == chess::MoveFlag::CastleKingside) {
            rookFrom = chess::makeSquare(7, r);
            rookTo   = chess::makeSquare(5, r);
        } else {
            rookFrom = chess::makeSquare(0, r);
            rookTo   = chess::makeSquare(3, r);
        }
        const int rookIdx = findPieceAtSquare(rookFrom);
        if (rookIdx >= 0) {
            VisualPiece& rk = pieces_[rookIdx];
            rk.square = rookTo;
            Tween t;
            t.pieceId = rk.id;
            t.startDelay = kCastleRookDelay;
            t.duration = kMoveDuration;
            t.fromPos = rk.worldPos;
            t.toPos = squareToWorld(chess::fileOf(rookTo), chess::rankOf(rookTo));
            t.fromYaw = rk.yawDeg;
            t.toYaw = rk.yawDeg;
            t.easing = &easeInOutCubic;
            tweens_.push_back(t);
        }
    }
}

void Animator::animateKingFall(chess::Color loserColor) {
    for (auto& v : pieces_) {
        if (!v.alive) continue;
        if (v.type != chess::PieceType::King) continue;
        if (v.color != loserColor) continue;
        Tween t;
        t.pieceId = v.id;
        t.startDelay = 0.4f;  // pausa breve antes da queda
        t.duration = 1.0f;
        t.fromPos = v.worldPos;
        t.toPos = v.worldPos;
        t.fromYaw = v.yawDeg;
        t.toYaw = v.yawDeg;
        t.fromPitch = v.pitchDeg;
        t.toPitch = 90.0f;  // tomba 90° no eixo X
        t.easing = &easeOutBounce;
        tweens_.push_back(t);
        break;
    }
}

int Animator::spawnPiece(const VisualPiece& tpl) {
    VisualPiece v = tpl;
    v.id = nextId_++;
    pieces_.push_back(v);
    return static_cast<int>(pieces_.size() - 1);
}

void Animator::update(float dt) {
    for (auto& t : tweens_) {
        if (t.startDelay > 0.0f) {
            t.startDelay -= dt;
            if (t.startDelay > 0.0f) continue;
            // sobra de dt vira parte do elapsed inicial
            t.elapsed = -t.startDelay;
            t.startDelay = 0.0f;
        } else {
            t.elapsed = std::max(0.0f, t.elapsed) + dt;
        }

        VisualPiece* v = nullptr;
        for (auto& p : pieces_) if (p.id == t.pieceId) { v = &p; break; }
        if (!v) continue;

        const float raw = (t.duration > 0.0f)
            ? std::clamp(t.elapsed / t.duration, 0.0f, 1.0f)
            : 1.0f;
        const float u = t.easing ? t.easing(raw) : raw;

        v->worldPos = glm::mix(t.fromPos, t.toPos, u);
        if (t.arcHeight > 0.0f) {
            // arco parabólico h(t) = 4*H*t*(1-t)
            v->worldPos.y += 4.0f * t.arcHeight * raw * (1.0f - raw);
        }
        v->yawDeg   = glm::mix(t.fromYaw,   t.toYaw,   u);
        v->pitchDeg = glm::mix(t.fromPitch, t.toPitch, u);
        v->scale    = glm::mix(t.fromScale, t.toScale, u);
        v->alpha    = glm::mix(t.fromAlpha, t.toAlpha, u);

        if (raw >= 1.0f) {
            v->worldPos = t.toPos;
            v->yawDeg = t.toYaw;
            v->pitchDeg = t.toPitch;
            v->scale = t.toScale;
            v->alpha = t.toAlpha;
            if (t.promoteAtEnd != chess::PieceType::None) {
                v->type = t.promoteAtEnd;
            }
            if (t.removeAtEnd) {
                v->alive = false;
            }
        }
    }

    // Remove tweens completados
    tweens_.erase(std::remove_if(tweens_.begin(), tweens_.end(),
                                 [](const Tween& t) {
                                     return t.elapsed >= t.duration && t.startDelay <= 0.0f;
                                 }),
                  tweens_.end());

    // Remove peças mortas
    pieces_.erase(std::remove_if(pieces_.begin(), pieces_.end(),
                                 [](const VisualPiece& p) { return !p.alive; }),
                  pieces_.end());
}

bool Animator::isAnimating() const {
    return !tweens_.empty();
}

}  // namespace chess3d::anim
