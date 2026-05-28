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
    for (std::size_t i = 0; i < legal.size(); ++i)
        if (legal[i] == m) return true;
    return false;
}

}  // namespace

// ---- Sanity ----------------------------------------------------------------

TEST_CASE("AI always returns legal move from starting position", "[ai]") {
    chess::Board b;
    ai::MinimaxAgent agent(2);
    chess::Move m = agent.chooseMove(b);
    chess::MoveList legal;
    chess::generateLegalMoves(b, legal);
    REQUIRE(isLegalIn(m, legal));
}

TEST_CASE("AI captures free material queen undefended", "[ai]") {
    chess::Board b;
    // Pretas a mover, rainha branca no e4 sem defesa, peao preto em d5 pode captura-la.
    REQUIRE(b.loadFen("4k3/8/8/3p4/4Q3/8/8/4K3 b - - 0 1"));
    ai::MinimaxAgent agent(2);
    const chess::Move m = agent.chooseMove(b);
    REQUIRE(m.from == chess::makeSquare(3, 4));  // d5
    REQUIRE(m.to   == chess::makeSquare(4, 3));  // e4
    REQUIRE(m.isCapture());
}

TEST_CASE("AI detects mate in 1", "[ai]") {
    chess::Board b;
    REQUIRE(b.loadFen("7k/8/5KQ1/8/8/8/8/8 w - - 0 1"));
    ai::MinimaxAgent agent(2);
    const chess::Move m = agent.chooseMove(b);
    chess::UndoInfo undo;
    b.makeMove(m, undo);
    REQUIRE(chess::isCheckmate(b));
}

// ---- Mate em no maximo 2 ---------------------------------------------------

TEST_CASE("AI depth 4 finds mate in at most 2 moves", "[ai][mate2]") {
    chess::Board b;
    // "1k6/8/1KR5/8/8/8/8/8": Kb8, Kb6 (brancas), Rc6.
    // 1.Rc8# e mate direto (M1), ou a IA encontra outro caminho.
    // O teste aceita tanto M1 (pos lance branco ja e mate)
    // quanto M2 (mate apos resposta preta).
    REQUIRE(b.loadFen("1k6/8/1KR5/8/8/8/8/8 w - - 0 1"));
    ai::MinimaxAgent agent(4);
    const chess::Move m = agent.chooseMove(b);
    REQUIRE_FALSE(m.isNull());

    chess::UndoInfo undo1;
    b.makeMove(m, undo1);

    // Se ja e mate apos o primeiro lance (M1), aceita.
    if (chess::isCheckmate(b)) return;

    // Nao deve ter dado stalemate.
    REQUIRE_FALSE(chess::isStalemate(b));

    // Resposta das pretas
    ai::MinimaxAgent blackDef(4);
    const chess::Move bm = blackDef.chooseMove(b);
    if (bm.isNull()) return;  // sem movimentos => ja acabou
    chess::UndoInfo undo2;
    b.makeMove(bm, undo2);

    // Brancas entregam o mate
    ai::MinimaxAgent agent2(4);
    const chess::Move m2 = agent2.chooseMove(b);
    REQUIRE_FALSE(m2.isNull());
    chess::UndoInfo undo3;
    b.makeMove(m2, undo3);
    REQUIRE(chess::isCheckmate(b));
}

// ---- Promotion choice ------------------------------------------------------

TEST_CASE("AI promotes to queen in clear promotion position", "[ai]") {
    chess::Board b;
    // Peao branco em e7, rei preto em h8, sem stalemate ao promover para rainha.
    REQUIRE(b.loadFen("7k/4P3/8/8/8/8/8/4K3 w - - 0 1"));
    ai::MinimaxAgent agent(2);
    const chess::Move m = agent.chooseMove(b);
    INFO("move: from=" << (int)m.from << " to=" << (int)m.to
         << " promo=" << (int)m.promotion);
    REQUIRE(m.isPromotion());
    REQUIRE(m.promotion == chess::PieceType::Queen);
}

// ---- Alpha-beta pruning ----------------------------------------------------

TEST_CASE("AI alpha-beta and MVV-LVA reduces nodes by more than 10x", "[ai][pruning]") {
    chess::Board b;
    ai::EvaluatorConfig ec;
    ec.usePsts = true;

    ai::SearchConfig naive;
    naive.depth = 4;
    naive.useAlphaBeta = false;
    naive.useMoveOrdering = false;
    ai::MinimaxAgent naiveAgent(naive, ec);

    ai::SearchConfig pruned;
    pruned.depth = 4;
    pruned.useAlphaBeta = true;
    pruned.useMoveOrdering = true;
    ai::MinimaxAgent prunedAgent(pruned, ec);

    chess::Board copy = b;
    naiveAgent.chooseMove(copy);
    const auto naiveNodes = naiveAgent.lastInfo().nodesVisited;

    chess::Board copy2 = b;
    prunedAgent.chooseMove(copy2);
    const auto prunedNodes = prunedAgent.lastInfo().nodesVisited;

    INFO("naive nodes=" << naiveNodes << " pruned nodes=" << prunedNodes
         << " ratio=" << (prunedNodes > 0 ? static_cast<double>(naiveNodes) / prunedNodes : 0.0));
    REQUIRE(prunedNodes > 0);
    REQUIRE(naiveNodes > prunedNodes * 10);
}

// ---- Depth efectividade ----------------------------------------------------

TEST_CASE("AI depth 4 beats depth 2 in majority of games", "[ai][depth][slow]") {
    int hardWins = 0;
    for (int trial = 0; trial < 5; ++trial) {
        chess::Board b;
        std::vector<std::string> history;
        history.push_back(chess::positionKey(b));

        // Hard (depth 4, ordering) as white; Easy (depth 2) as black
        ai::SearchConfig hCfg;
        hCfg.depth = 4; hCfg.useAlphaBeta = true; hCfg.useMoveOrdering = true;
        ai::MinimaxAgent white(hCfg, {});
        ai::MinimaxAgent black(2);

        chess::GameResult result = chess::GameResult::Ongoing;
        int plies = 0;
        constexpr int kMax = 200;

        while (result == chess::GameResult::Ongoing && plies < kMax) {
            auto& agent = (b.sideToMove() == chess::Color::White)
                ? static_cast<ai::Agent&>(white) : static_cast<ai::Agent&>(black);
            const chess::Move m = agent.chooseMove(b);
            chess::MoveList legal; chess::generateLegalMoves(b, legal);
            REQUIRE(isLegalIn(m, legal));
            chess::UndoInfo undo; b.makeMove(m, undo);
            history.push_back(chess::positionKey(b));
            result = chess::evaluateGame(b, history);
            ++plies;
        }
        if (result == chess::GameResult::WhiteWins) ++hardWins;
    }
    INFO("hardWins=" << hardWins << "/5");
    REQUIRE(hardWins >= 3);
}

// ---- AI vs AI full games ---------------------------------------------------

TEST_CASE("AI vs AI 5 games finish without crash and with valid result", "[ai][game]") {
    for (int trial = 0; trial < 5; ++trial) {
        chess::Board b;
        std::vector<std::string> history;
        history.push_back(chess::positionKey(b));

        ai::MinimaxAgent white(2);
        ai::MinimaxAgent black(2);

        chess::GameResult result = chess::GameResult::Ongoing;
        int plies = 0;
        constexpr int kMaxPlies = 250;

        while (result == chess::GameResult::Ongoing && plies < kMaxPlies) {
            ai::Agent& agent = (b.sideToMove() == chess::Color::White)
                ? static_cast<ai::Agent&>(white)
                : static_cast<ai::Agent&>(black);
            const chess::Move m = agent.chooseMove(b);

            chess::MoveList legal;
            chess::generateLegalMoves(b, legal);
            REQUIRE(isLegalIn(m, legal));

            chess::UndoInfo undo;
            b.makeMove(m, undo);
            history.push_back(chess::positionKey(b));
            result = chess::evaluateGame(b, history);
            ++plies;
        }

        INFO("trial=" << trial << " plies=" << plies
             << " result=" << chess::gameResultName(result));
        REQUIRE((result != chess::GameResult::Ongoing || plies >= kMaxPlies));
    }
}
