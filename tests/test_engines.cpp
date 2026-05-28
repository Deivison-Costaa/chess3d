#include "ai/EngineCatalog.h"
#include "ai/DifficultyLevels.h"
#include "ai/MinimaxAgent.h"
#include "chess/Board.h"
#include "chess/MoveGenerator.h"

#include <catch2/catch_test_macros.hpp>

using namespace chess3d;

// ---- Engine catalog --------------------------------------------------------

TEST_CASE("EngineCatalog detect completes without hang", "[engines]") {
    const ai::EngineCatalog cat = ai::EngineCatalog::detect();
    SUCCEED("EngineCatalog::detect() completed");
}

// ---- makeAgent factory -----------------------------------------------------

TEST_CASE("makeAgent Minimax Easy created and returns legal move", "[engines]") {
    ai::AgentSpec spec;
    spec.engine = ai::AgentSpec::Engine::MinimaxEasy;
    auto agent = ai::makeAgent(spec);
    REQUIRE(agent != nullptr);
    REQUIRE_FALSE(agent->name().empty());

    chess::Board b;
    chess::Move m = agent->chooseMove(b);
    chess::MoveList legal; chess::generateLegalMoves(b, legal);
    bool found = false;
    for (std::size_t i = 0; i < legal.size(); ++i)
        if (legal[i] == m) { found = true; break; }
    REQUIRE(found);
}

TEST_CASE("makeAgent Minimax Medium returns legal move", "[engines]") {
    ai::AgentSpec spec;
    spec.engine = ai::AgentSpec::Engine::MinimaxMedium;
    auto agent = ai::makeAgent(spec);
    REQUIRE(agent != nullptr);

    chess::Board b;
    chess::Move m = agent->chooseMove(b);
    chess::MoveList legal; chess::generateLegalMoves(b, legal);
    bool found = false;
    for (std::size_t i = 0; i < legal.size(); ++i)
        if (legal[i] == m) { found = true; break; }
    REQUIRE(found);
}

TEST_CASE("makeAgent Minimax Hard returns legal move", "[engines][slow]") {
    ai::AgentSpec spec;
    spec.engine = ai::AgentSpec::Engine::MinimaxHard;
    auto agent = ai::makeAgent(spec);
    REQUIRE(agent != nullptr);

    chess::Board b;
    chess::Move m = agent->chooseMove(b);
    chess::MoveList legal; chess::generateLegalMoves(b, legal);
    bool found = false;
    for (std::size_t i = 0; i < legal.size(); ++i)
        if (legal[i] == m) { found = true; break; }
    REQUIRE(found);
}

// ---- UCI engines (conditional) ---------------------------------------------

TEST_CASE("makeAgent Stockfish returns legal move if available", "[engines][.uci]") {
    const ai::EngineCatalog cat = ai::EngineCatalog::detect();
    if (!cat.stockfish) {
        SKIP("Stockfish not found in assets/engines/");
    }

    ai::AgentSpec spec;
    spec.engine = ai::AgentSpec::Engine::Stockfish;
    spec.moveTimeMs = 500;
    auto agent = ai::makeAgent(spec);
    REQUIRE(agent != nullptr);

    chess::Board b;
    chess::Move m = agent->chooseMove(b);
    REQUIRE_FALSE(m.isNull());

    chess::MoveList legal; chess::generateLegalMoves(b, legal);
    bool found = false;
    for (std::size_t i = 0; i < legal.size(); ++i)
        if (legal[i] == m) { found = true; break; }
    REQUIRE(found);
}

TEST_CASE("makeAgent Stockfish fallback when binary absent", "[engines]") {
    const ai::EngineCatalog cat = ai::EngineCatalog::detect();
    if (cat.stockfish) {
        SKIP("Stockfish present -- fallback test not applicable");
    }

    ai::AgentSpec spec;
    spec.engine = ai::AgentSpec::Engine::Stockfish;
    auto agent = ai::makeAgent(spec);
    REQUIRE(agent != nullptr);  // must not be null -- fallback kicks in

    chess::Board b;
    chess::Move m = agent->chooseMove(b);
    chess::MoveList legal; chess::generateLegalMoves(b, legal);
    bool found = false;
    for (std::size_t i = 0; i < legal.size(); ++i)
        if (legal[i] == m) { found = true; break; }
    REQUIRE(found);
}

// ---- Agent move legality on repeated calls ----------------------------------

TEST_CASE("AI both calls return legal moves on identical positions", "[engines]") {
    // MinimaxAgent uses random tie-breaking among equal-scored moves, so
    // two calls may return different (but both legal) moves. Verify legality.
    ai::MinimaxAgent agent(2);
    chess::Board b1, b2;

    const chess::Move m1 = agent.chooseMove(b1);
    const chess::Move m2 = agent.chooseMove(b2);

    auto isLegal = [](const chess::Move& m, chess::Board& b) {
        chess::MoveList legal; chess::generateLegalMoves(b, legal);
        for (std::size_t i = 0; i < legal.size(); ++i)
            if (legal[i] == m) return true;
        return false;
    };

    REQUIRE(isLegal(m1, b1));
    REQUIRE(isLegal(m2, b2));
}
