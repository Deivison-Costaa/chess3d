#include "chess/Board.h"
#include "chess/MoveGenerator.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>

using namespace chess3d::chess;

namespace {

struct PerftCase {
    int depth;
    std::uint64_t nodes;
};

void runPerftSuite(const char* fen, const PerftCase* cases, std::size_t count) {
    Board b;
    REQUIRE(b.loadFen(fen));
    for (std::size_t i = 0; i < count; ++i) {
        const auto& c = cases[i];
        const std::uint64_t got = perft(b, c.depth);
        INFO("FEN=" << fen << " depth=" << c.depth
             << " expected=" << c.nodes << " got=" << got);
        REQUIRE(got == c.nodes);
    }
}

}  // namespace

TEST_CASE("Board reset position is standard starting position", "[board][fen]") {
    Board b;
    REQUIRE(b.toFen() == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

TEST_CASE("Board FEN round-trip", "[board][fen]") {
    const std::string positions[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    };
    for (const auto& fen : positions) {
        Board b;
        REQUIRE(b.loadFen(fen));
        REQUIRE(b.toFen() == fen);
    }
}

TEST_CASE("Perft — starting position", "[perft][slow]") {
    // https://www.chessprogramming.org/Perft_Results
    constexpr PerftCase cases[] = {
        {1, 20},
        {2, 400},
        {3, 8902},
        {4, 197281},
    };
    runPerftSuite("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                  cases, std::size(cases));
}

TEST_CASE("Perft — Kiwipete (position 2)", "[perft][slow]") {
    // Posição clássica que cobre roque, en passant e capturas com promoção
    constexpr PerftCase cases[] = {
        {1, 48},
        {2, 2039},
        {3, 97862},
    };
    runPerftSuite("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                  cases, std::size(cases));
}

TEST_CASE("Perft — Position 3 (deep endgame with pawns)", "[perft][slow]") {
    constexpr PerftCase cases[] = {
        {1, 14},
        {2, 191},
        {3, 2812},
        {4, 43238},
    };
    runPerftSuite("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
                  cases, std::size(cases));
}

TEST_CASE("Perft — Position 4 (mirror of 5)", "[perft][slow]") {
    constexpr PerftCase cases[] = {
        {1, 6},
        {2, 264},
        {3, 9467},
    };
    runPerftSuite("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
                  cases, std::size(cases));
}

TEST_CASE("Check detection — back-rank mate", "[rules]") {
    Board b;
    REQUIRE(b.loadFen("6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1"));
    // Ra8+ → xeque-mate
    Move m{makeSquare(0, 0), makeSquare(0, 7), PieceType::None, MoveFlag::Quiet};
    UndoInfo undo;
    b.makeMove(m, undo);
    REQUIRE(b.inCheck(Color::Black));
    MoveList legal;
    generateLegalMoves(b, legal);
    REQUIRE(legal.size() == 0);  // sem fugas — mate
}
