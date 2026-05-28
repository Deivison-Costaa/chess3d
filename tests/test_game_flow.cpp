#include "app/GameSession.h"
#include "chess/Rules.h"
#include "ui/GameUi.h"

#include <catch2/catch_test_macros.hpp>

using namespace chess3d;
using namespace chess3d::chess;
using namespace chess3d::ui;

namespace {

GameSetup makeAiVsAi(ai::AgentSpec::Engine white = ai::AgentSpec::Engine::MinimaxEasy,
                     ai::AgentSpec::Engine black = ai::AgentSpec::Engine::MinimaxEasy) {
    GameSetup s;
    s.mode = GameMode::AiVsAi;
    s.whiteAgent.engine = white;
    s.blackAgent.engine = black;
    return s;
}

GameSetup makeHumanVsAi(Color humanColor = Color::White,
                        ai::AgentSpec::Engine aiEngine = ai::AgentSpec::Engine::MinimaxEasy) {
    GameSetup s;
    s.mode = GameMode::HumanVsAi;
    s.humanColor = humanColor;
    if (humanColor == Color::White) s.blackAgent.engine = aiEngine;
    else                            s.whiteAgent.engine = aiEngine;
    return s;
}

GameSetup makeHotseat() {
    GameSetup s;
    s.mode = GameMode::Hotseat;
    return s;
}

}  // namespace

// ---- AI vs AI --------------------------------------------------------------

TEST_CASE("GameSession AI vs AI finishes with valid result", "[game][ai]") {
    GameSession session;
    REQUIRE(session.start(makeAiVsAi()));

    int plies = 0;
    constexpr int kMax = 250;
    while (!session.isOver() && plies < kMax) {
        session.tick(0);
        ++plies;
    }

    INFO("plies=" << plies << " result=" << gameResultName(session.result()));
    REQUIRE((session.isOver() || plies >= kMax));
    REQUIRE(session.played().size() == static_cast<std::size_t>(plies));
    REQUIRE(session.positionHistory().size() == session.played().size() + 1);
}

TEST_CASE("GameSession AI vs AI does not produce null moves", "[game][ai]") {
    GameSession session;
    REQUIRE(session.start(makeAiVsAi()));

    constexpr int kMax = 50;
    for (int i = 0; i < kMax && !session.isOver(); ++i) {
        const std::size_t before = session.played().size();
        session.tick(0);
        const std::size_t after = session.played().size();
        if (after > before) {
            REQUIRE_FALSE(session.played().back().move.isNull());
        }
    }
}

// ---- Human vs AI scripted --------------------------------------------------

TEST_CASE("GameSession Human vs AI e7e5 submitted and AI responds", "[game][human]") {
    GameSession session;
    auto setup = makeHumanVsAi(Color::Black);  // human plays black
    REQUIRE(session.start(setup));

    // White is AI -- advance it first
    session.tick(0);
    REQUIRE(session.played().size() == 1);

    // Now it's black's (human) turn
    REQUIRE(session.board().sideToMove() == Color::Black);
    REQUIRE(session.submitMove("e7e5"));
    REQUIRE(session.played().size() == 2);

    // AI responds (white)
    session.tick(0);
    REQUIRE(session.played().size() == 3);
}

TEST_CASE("GameSession Human vs AI illegal move rejected", "[game][human]") {
    GameSession session;
    REQUIRE(session.start(makeHumanVsAi(Color::White)));

    // e2e5 is illegal
    REQUIRE_FALSE(session.submitMove("e2e5"));
    REQUIRE(session.played().empty());

    // e2e4 is legal
    REQUIRE(session.submitMove("e2e4"));
    REQUIRE(session.played().size() == 1);
}

TEST_CASE("GameSession Human vs AI rejects move when it is AI turn", "[game][human]") {
    GameSession session;
    REQUIRE(session.start(makeHumanVsAi(Color::Black)));
    // White is AI's turn -- human (black) cannot play
    REQUIRE_FALSE(session.submitMove("e2e4"));
}

// ---- Hotseat ---------------------------------------------------------------

TEST_CASE("GameSession Hotseat alternates turns", "[game][hotseat]") {
    GameSession session;
    REQUIRE(session.start(makeHotseat()));

    REQUIRE(session.board().sideToMove() == Color::White);
    REQUIRE(session.submitMove("e2e4"));
    REQUIRE(session.board().sideToMove() == Color::Black);
    REQUIRE(session.submitMove("e7e5"));
    REQUIRE(session.board().sideToMove() == Color::White);
    REQUIRE(session.played().size() == 2);
}

TEST_CASE("GameSession Hotseat no pending promotion initially", "[game][hotseat]") {
    GameSession session;
    REQUIRE(session.start(makeHotseat()));
    REQUIRE_FALSE(session.hasPendingPromotion());
}

TEST_CASE("GameSession Hotseat 10 move sequence without crash", "[game][hotseat]") {
    GameSession session;
    REQUIRE(session.start(makeHotseat()));

    const std::vector<std::string> moves = {
        "e2e4", "e7e5",
        "g1f3", "b8c6",
        "f1c4", "f8c5",
        "b1c3", "g8f6",
        "d2d3", "d7d6",
    };
    for (const auto& uci : moves) {
        REQUIRE_FALSE(session.isOver());
        INFO("move: " << uci);
        REQUIRE(session.submitMove(uci));
    }
    REQUIRE(session.played().size() == 10);
}

// ---- Undo ------------------------------------------------------------------

TEST_CASE("GameSession Hotseat undo restores state", "[game][undo]") {
    GameSession session;
    REQUIRE(session.start(makeHotseat()));

    const std::string initialFen = session.board().toFen();

    REQUIRE(session.submitMove("e2e4"));
    REQUIRE(session.submitMove("e7e5"));
    REQUIRE(session.submitMove("g1f3"));

    // Undo 2 plies (g1f3 + e7e5) -> back to after e2e4, which is black's turn
    REQUIRE(session.undo());
    REQUIRE(session.played().size() == 1);
    REQUIRE(session.board().sideToMove() == Color::Black);

    // Undo again (e2e4) -> back to start
    REQUIRE(session.undo());
    REQUIRE(session.played().empty());
    REQUIRE(session.board().toFen() == initialFen);
}

TEST_CASE("GameSession undo returns false when no moves played", "[game][undo]") {
    GameSession session;
    REQUIRE(session.start(makeHotseat()));
    REQUIRE_FALSE(session.undo());
}

TEST_CASE("GameSession undo after AI move removes 2 plies", "[game][undo]") {
    GameSession session;
    REQUIRE(session.start(makeHumanVsAi(Color::White)));

    const std::string fenStart = session.board().toFen();

    REQUIRE(session.submitMove("e2e4"));
    session.tick(0);  // AI responds
    REQUIRE(session.played().size() == 2);

    REQUIRE(session.undo());
    REQUIRE(session.played().empty());
    REQUIRE(session.board().toFen() == fenStart);
}

// ---- Clock -----------------------------------------------------------------

TEST_CASE("GameSession clock decrements on tick", "[game][clock]") {
    GameSetup setup = makeAiVsAi();
    setup.timeControl.initialMs   = 5000;
    setup.timeControl.incrementMs = 0;

    GameSession session;
    REQUIRE(session.start(setup));

    REQUIRE(session.whiteTimeMs() == 5000);
    session.tick(500);
    REQUIRE(session.whiteTimeMs() <= 4500);
}

TEST_CASE("GameSession clock flag when time runs out", "[game][clock]") {
    GameSetup setup = makeAiVsAi();
    setup.timeControl.initialMs = 10;  // very short
    setup.timeControl.incrementMs = 0;

    GameSession session;
    REQUIRE(session.start(setup));

    session.tick(500);  // force flag

    REQUIRE(session.isOver());
    const auto r = session.result();
    REQUIRE((r == GameResult::BlackWinsOnTime || r == GameResult::WhiteWinsOnTime));
}

// ---- Promotion workflow ----------------------------------------------------

TEST_CASE("GameSession acceptPromotion returns false when none pending", "[game][promotion]") {
    GameSession session;
    REQUIRE(session.start(makeHotseat()));
    REQUIRE_FALSE(session.hasPendingPromotion());
    REQUIRE_FALSE(session.acceptPromotion(PieceType::Queen));
}

// ---- State invariants ------------------------------------------------------

TEST_CASE("GameSession result is Ongoing before game ends", "[game][result]") {
    GameSession session;
    REQUIRE(session.start(makeAiVsAi()));
    REQUIRE(session.result() == GameResult::Ongoing);
    REQUIRE_FALSE(session.isOver());
}

TEST_CASE("GameSession played history size matches positionHistory size", "[game][history]") {
    GameSession session;
    REQUIRE(session.start(makeHotseat()));

    const std::string moves[] = {"e2e4", "e7e5", "d2d4", "d7d5"};
    for (const auto& m : moves) session.submitMove(m);

    REQUIRE(session.played().size() == 4);
    REQUIRE(session.positionHistory().size() == 5);  // initial + 4 moves
}
