#include "chess/Board.h"
#include "chess/MoveGenerator.h"
#include "chess/Rules.h"

#include <catch2/catch_test_macros.hpp>

using namespace chess3d::chess;

TEST_CASE("Rules — fool's mate (mate de pastor)", "[rules][mate]") {
    Board b;
    // 1.f3 e5 2.g4 Qh4#
    REQUIRE(b.loadFen("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3"));
    REQUIRE(isCheckmate(b));
    REQUIRE(evaluatePosition(b) == GameResult::BlackWins);
}

TEST_CASE("Rules — stalemate clássico", "[rules][stalemate]") {
    Board b;
    REQUIRE(b.loadFen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"));
    REQUIRE_FALSE(b.inCheck(Color::Black));
    REQUIRE(isStalemate(b));
    REQUIRE(evaluatePosition(b) == GameResult::DrawStalemate);
}

TEST_CASE("Rules — K vs K = material insuficiente", "[rules][material]") {
    Board b;
    REQUIRE(b.loadFen("8/8/4k3/8/8/4K3/8/8 w - - 0 1"));
    REQUIRE(isInsufficientMaterial(b));
}

TEST_CASE("Rules — K+B vs K = material insuficiente", "[rules][material]") {
    Board b;
    REQUIRE(b.loadFen("8/8/4k3/8/8/4K3/8/3B4 w - - 0 1"));
    REQUIRE(isInsufficientMaterial(b));
}

TEST_CASE("Rules — K+N vs K = material insuficiente", "[rules][material]") {
    Board b;
    REQUIRE(b.loadFen("8/8/4k3/8/8/4K3/8/3N4 w - - 0 1"));
    REQUIRE(isInsufficientMaterial(b));
}

TEST_CASE("Rules — K+R vs K = material suficiente", "[rules][material]") {
    Board b;
    REQUIRE(b.loadFen("8/8/4k3/8/8/4K3/8/3R4 w - - 0 1"));
    REQUIRE_FALSE(isInsufficientMaterial(b));
}

TEST_CASE("Rules — regra dos 50 lances dispara em halfmoveClock=100", "[rules][50move]") {
    Board b;
    REQUIRE(b.loadFen("4k3/8/8/8/8/8/8/4K3 w - - 100 50"));
    REQUIRE(isFiftyMoveRule(b));
    REQUIRE(evaluatePosition(b) == GameResult::DrawFiftyMoveRule);
}

TEST_CASE("Rules — repeticao tripla via historico", "[rules][repetition]") {
    Board b;
    std::vector<std::string> hist;
    hist.push_back(positionKey(b));
    hist.push_back(positionKey(b));  // duas ocorrências passadas
    // A posição atual é a terceira → empate por repetição
    REQUIRE(evaluateGame(b, hist) == GameResult::DrawThreefoldRepetition);
}
