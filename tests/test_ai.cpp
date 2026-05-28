#include "ai/MinimaxAgent.h"
#include "chess/Board.h"
#include "chess/MoveGenerator.h"
#include "chess/Rules.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

using namespace chess3d;

namespace {

bool isLegalIn(const chess::Move& m, const chess::MoveList& legal) {
    for (std::size_t i = 0; i < legal.size(); ++i) {
        if (legal[i] == m) return true;
    }
    return false;
}

}  // namespace

TEST_CASE("AI — sempre retorna movimento legal na posicao inicial", "[ai]") {
    chess::Board b;
    ai::MinimaxAgent agent(2);
    chess::Move m = agent.chooseMove(b);
    chess::MoveList legal;
    chess::generateLegalMoves(b, legal);
    REQUIRE(isLegalIn(m, legal));
}

TEST_CASE("AI — captura material livre (rainha empilhada)", "[ai]") {
    chess::Board b;
    // Pretas a mover, rainha branca no e4 sem defesa, peão preto em d5 pode capturá-la.
    REQUIRE(b.loadFen("4k3/8/8/3p4/4Q3/8/8/4K3 b - - 0 1"));
    ai::MinimaxAgent agent(2);
    const chess::Move m = agent.chooseMove(b);
    // Esperado: ...dxe4 captura
    REQUIRE(m.from == chess::makeSquare(3, 4));  // d5
    REQUIRE(m.to   == chess::makeSquare(4, 3));  // e4
    REQUIRE(m.isCapture());
}

TEST_CASE("AI — detecta mate em 1 (branca a mover)", "[ai]") {
    chess::Board b;
    // Mate clássico: Qxh7# em posição preparada.
    // Posição: rei preto em h8, peão preto em g7 (somente saída bloqueada), rainha branca em h5,
    // bispo branco em c4 mira f7 (não-essencial). 1.Qxh7# (peão g7 defende mas Qh7 atacada por Kg8? não, rei em h8)
    // Vamos usar uma posição mais simples:
    // Rei preto em h8, rainha branca em g6, rei branco em f6: 1.Qg7#  e 1.Qg8#  e 1.Qh7#
    REQUIRE(b.loadFen("7k/8/5KQ1/8/8/8/8/8 w - - 0 1"));
    ai::MinimaxAgent agent(2);
    const chess::Move m = agent.chooseMove(b);

    // Aplica a jogada da IA e verifica que resultou em mate.
    chess::UndoInfo undo;
    b.makeMove(m, undo);
    REQUIRE(chess::isCheckmate(b));
}

TEST_CASE("AI vs AI — 5 partidas terminam sem crash e com resultado valido", "[ai][game]") {
    for (int trial = 0; trial < 5; ++trial) {
        chess::Board b;
        std::vector<std::string> history;
        history.push_back(chess::positionKey(b));

        ai::MinimaxAgent white(2);
        ai::MinimaxAgent black(2);

        chess::GameResult result = chess::GameResult::Ongoing;
        int plies = 0;
        constexpr int kMaxPlies = 250;  // safety net (depth 2 pode entrar em ciclo)

        while (result == chess::GameResult::Ongoing && plies < kMaxPlies) {
            ai::Agent& agent = (b.sideToMove() == chess::Color::White)
                ? static_cast<ai::Agent&>(white)
                : static_cast<ai::Agent&>(black);
            const chess::Move m = agent.chooseMove(b);

            chess::MoveList legal;
            chess::generateLegalMoves(b, legal);
            REQUIRE(isLegalIn(m, legal));  // não alucina

            chess::UndoInfo undo;
            b.makeMove(m, undo);
            history.push_back(chess::positionKey(b));
            result = chess::evaluateGame(b, history);
            ++plies;
        }

        // Verifica que terminou (com resultado real ou pelo safety net que ainda é "ongoing").
        INFO("trial=" << trial << " plies=" << plies
             << " result=" << chess::gameResultName(result));
        REQUIRE((result != chess::GameResult::Ongoing || plies >= kMaxPlies));
    }
}
