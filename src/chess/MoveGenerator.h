#pragma once

#include "Board.h"
#include "Move.h"

#include <vector>

namespace chess3d::chess {

// Lista de movimentos — usa std::vector mas com reserve para evitar realocação.
class MoveList {
public:
    MoveList() { moves_.reserve(64); }
    void push(const Move& m) { moves_.push_back(m); }
    void clear() { moves_.clear(); }
    std::size_t size() const { return moves_.size(); }
    const Move& operator[](std::size_t i) const { return moves_[i]; }
    Move& operator[](std::size_t i) { return moves_[i]; }
    auto begin() { return moves_.begin(); }
    auto end()   { return moves_.end(); }
    auto begin() const { return moves_.begin(); }
    auto end()   const { return moves_.end(); }
private:
    std::vector<Move> moves_;
};

// Gera todos os movimentos pseudo-legais (sem filtro de xeque do próprio rei).
void generatePseudoLegalMoves(const Board& board, MoveList& out);

// Filtra para deixar apenas legais.
void generateLegalMoves(Board& board, MoveList& out);

// Perft: número de folhas alcançáveis em `depth` ply. Útil para validar o gerador.
std::uint64_t perft(Board& board, int depth);

// Perft "divide" — imprime contagem por movimento raiz. Útil para depurar discrepâncias.
struct PerftDivideEntry {
    Move move;
    std::uint64_t nodes;
};
std::vector<PerftDivideEntry> perftDivide(Board& board, int depth);

}  // namespace chess3d::chess
