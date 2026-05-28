#include "chess/Board.h"
#include "chess/MoveGenerator.h"
#include "chess/Notation.h"
#include "chess/Rules.h"

#include <catch2/catch_test_macros.hpp>

using namespace chess3d::chess;

// ---- Mate / stalemate ------------------------------------------------------

TEST_CASE("Rules fool's mate", "[rules][mate]") {
    Board b;
    // 1.f3 e5 2.g4 Qh4#
    REQUIRE(b.loadFen("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3"));
    REQUIRE(isCheckmate(b));
    REQUIRE(evaluatePosition(b) == GameResult::BlackWins);
}

TEST_CASE("Rules stalemate classic", "[rules][stalemate]") {
    Board b;
    REQUIRE(b.loadFen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1"));
    REQUIRE_FALSE(b.inCheck(Color::Black));
    REQUIRE(isStalemate(b));
    REQUIRE(evaluatePosition(b) == GameResult::DrawStalemate);
}

// ---- Material insuficiente -------------------------------------------------

TEST_CASE("Rules K vs K insufficient material", "[rules][material]") {
    Board b;
    REQUIRE(b.loadFen("8/8/4k3/8/8/4K3/8/8 w - - 0 1"));
    REQUIRE(isInsufficientMaterial(b));
}

TEST_CASE("Rules K+B vs K insufficient material", "[rules][material]") {
    Board b;
    REQUIRE(b.loadFen("8/8/4k3/8/8/4K3/8/3B4 w - - 0 1"));
    REQUIRE(isInsufficientMaterial(b));
}

TEST_CASE("Rules K+N vs K insufficient material", "[rules][material]") {
    Board b;
    REQUIRE(b.loadFen("8/8/4k3/8/8/4K3/8/3N4 w - - 0 1"));
    REQUIRE(isInsufficientMaterial(b));
}

TEST_CASE("Rules K+R vs K sufficient material", "[rules][material]") {
    Board b;
    REQUIRE(b.loadFen("8/8/4k3/8/8/4K3/8/3R4 w - - 0 1"));
    REQUIRE_FALSE(isInsufficientMaterial(b));
}

// ---- 50 movimentos ---------------------------------------------------------

TEST_CASE("Rules fifty move rule fires at halfmoveClock 100", "[rules][50move]") {
    Board b;
    REQUIRE(b.loadFen("4k3/8/8/8/8/8/8/4K3 w - - 100 50"));
    REQUIRE(isFiftyMoveRule(b));
    REQUIRE(evaluatePosition(b) == GameResult::DrawFiftyMoveRule);
}

TEST_CASE("Rules fifty move rule not fired at halfmoveClock 99", "[rules][50move]") {
    Board b;
    REQUIRE(b.loadFen("4k3/8/8/8/8/8/8/4K3 w - - 99 50"));
    REQUIRE_FALSE(isFiftyMoveRule(b));
}

// ---- Repeticao tripla ------------------------------------------------------

TEST_CASE("Rules threefold repetition via history", "[rules][repetition]") {
    Board b;
    std::vector<std::string> hist;
    hist.push_back(positionKey(b));
    hist.push_back(positionKey(b));  // duas ocorrencias passadas
    // A posicao atual e a terceira => empate por repeticao
    REQUIRE(evaluateGame(b, hist) == GameResult::DrawThreefoldRepetition);
}

TEST_CASE("Rules Nf3-Ng1 threefold repetition via real sequence", "[rules][repetition]") {
    Board b;
    std::vector<std::string> hist;
    hist.push_back(positionKey(b));  // pos0

    // 1.Nf3 Nf6  2.Ng1 Ng8  3.Nf3 Nf6  -> pos repetida 3x
    const std::vector<std::string> uciSeq = {
        "g1f3", "g8f6",
        "f3g1", "f6g8",
        "g1f3", "g8f6",
    };
    for (const auto& uci : uciSeq) {
        const int fromFile = uci[0] - 'a', fromRank = uci[1] - '1';
        const int toFile   = uci[2] - 'a', toRank   = uci[3] - '1';
        const Square from = makeSquare(fromFile, fromRank);
        const Square to   = makeSquare(toFile,   toRank);
        MoveList legal; generateLegalMoves(b, legal);
        chess3d::chess::Move chosen{};
        for (std::size_t i = 0; i < legal.size(); ++i)
            if (legal[i].from == from && legal[i].to == to) { chosen = legal[i]; break; }
        REQUIRE_FALSE(chosen.isNull());
        UndoInfo undo; b.makeMove(chosen, undo);
        hist.push_back(positionKey(b));
    }
    REQUIRE(evaluateGame(b, hist) == GameResult::DrawThreefoldRepetition);
}

// ---- Roque -----------------------------------------------------------------

TEST_CASE("Rules castling KS and QS available for white", "[rules][castling]") {
    Board b;
    REQUIRE(b.loadFen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
    MoveList legal; generateLegalMoves(b, legal);
    bool foundKS = false, foundQS = false;
    for (std::size_t i = 0; i < legal.size(); ++i) {
        if (legal[i].flag == MoveFlag::CastleKingside)  foundKS = true;
        if (legal[i].flag == MoveFlag::CastleQueenside) foundQS = true;
    }
    REQUIRE(foundKS);
    REQUIRE(foundQS);
}

TEST_CASE("Rules castling KS and QS available for black", "[rules][castling]") {
    Board b;
    REQUIRE(b.loadFen("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1"));
    MoveList legal; generateLegalMoves(b, legal);
    bool foundKS = false, foundQS = false;
    for (std::size_t i = 0; i < legal.size(); ++i) {
        if (legal[i].flag == MoveFlag::CastleKingside)  foundKS = true;
        if (legal[i].flag == MoveFlag::CastleQueenside) foundQS = true;
    }
    REQUIRE(foundKS);
    REQUIRE(foundQS);
}

TEST_CASE("Rules castling blocked when king is in check", "[rules][castling]") {
    Board b;
    // Rei branco em e1 em xeque pela torre preta em e8 -- sem roque permitido
    REQUIRE(b.loadFen("4r3/8/8/8/8/8/8/R3K2R w KQ - 0 1"));
    REQUIRE(b.inCheck(Color::White));
    MoveList legal; generateLegalMoves(b, legal);
    for (std::size_t i = 0; i < legal.size(); ++i) {
        REQUIRE_FALSE(legal[i].flag == MoveFlag::CastleKingside);
        REQUIRE_FALSE(legal[i].flag == MoveFlag::CastleQueenside);
    }
}

TEST_CASE("Rules castling blocked when rights lost", "[rules][castling]") {
    Board b;
    REQUIRE(b.loadFen("r3k2r/8/8/8/8/8/8/R3K2R w - - 0 1"));
    MoveList legal; generateLegalMoves(b, legal);
    for (std::size_t i = 0; i < legal.size(); ++i) {
        REQUIRE_FALSE(legal[i].flag == MoveFlag::CastleKingside);
        REQUIRE_FALSE(legal[i].flag == MoveFlag::CastleQueenside);
    }
}

TEST_CASE("Rules castling blocked when intermediate square attacked", "[rules][castling]") {
    Board b;
    // Torre preta em f8 cobre f1, impedindo roque kingside das brancas
    REQUIRE(b.loadFen("5r2/8/4k3/8/8/8/8/R3K2R w KQ - 0 1"));
    MoveList legal; generateLegalMoves(b, legal);
    bool foundKS = false;
    for (std::size_t i = 0; i < legal.size(); ++i)
        if (legal[i].flag == MoveFlag::CastleKingside) foundKS = true;
    REQUIRE_FALSE(foundKS);
}

// ---- En passant ------------------------------------------------------------

TEST_CASE("Rules en passant available after double pawn push", "[rules][ep]") {
    Board b;
    // Brancas em e5, pretas avancaram f7-f5 => ep em f6
    REQUIRE(b.loadFen("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3"));
    MoveList legal; generateLegalMoves(b, legal);
    bool foundEp = false;
    for (std::size_t i = 0; i < legal.size(); ++i)
        if (legal[i].flag == MoveFlag::EnPassant) foundEp = true;
    REQUIRE(foundEp);
}

TEST_CASE("Rules en passant not available on next move", "[rules][ep]") {
    Board b;
    // Sem quadrado de ep ('-')
    REQUIRE(b.loadFen("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 4"));
    MoveList legal; generateLegalMoves(b, legal);
    for (std::size_t i = 0; i < legal.size(); ++i)
        REQUIRE_FALSE(legal[i].flag == MoveFlag::EnPassant);
}

// ---- Promocao --------------------------------------------------------------

TEST_CASE("Rules promotion generates exactly 4 moves Q R B N", "[rules][promotion]") {
    Board b;
    // King on h8 so pawn on e7 can freely advance to e8
    REQUIRE(b.loadFen("7k/4P3/8/8/8/8/8/4K3 w - - 0 1"));
    MoveList legal; generateLegalMoves(b, legal);
    int promoCount = 0;
    bool hasQ = false, hasR = false, hasB = false, hasN = false;
    for (std::size_t i = 0; i < legal.size(); ++i) {
        if (!legal[i].isPromotion()) continue;
        ++promoCount;
        if (legal[i].promotion == PieceType::Queen)  hasQ = true;
        if (legal[i].promotion == PieceType::Rook)   hasR = true;
        if (legal[i].promotion == PieceType::Bishop) hasB = true;
        if (legal[i].promotion == PieceType::Knight) hasN = true;
    }
    REQUIRE(promoCount == 4);
    REQUIRE(hasQ); REQUIRE(hasR); REQUIRE(hasB); REQUIRE(hasN);
}

TEST_CASE("Rules promotion with capture generates 4 extra moves", "[rules][promotion]") {
    Board b;
    // Peao branco em e7 pode capturar em d8 (torre preta) ou avancar para e8
    // Rei preto em h8 para nao bloquear e8
    REQUIRE(b.loadFen("3r3k/4P3/8/8/8/8/8/4K3 w - - 0 1"));
    MoveList legal; generateLegalMoves(b, legal);
    int promoCapture = 0, promoQuiet = 0;
    for (std::size_t i = 0; i < legal.size(); ++i) {
        if (legal[i].flag == MoveFlag::PromotionCapture) ++promoCapture;
        if (legal[i].flag == MoveFlag::Promotion)        ++promoQuiet;
    }
    REQUIRE(promoQuiet   == 4);
    REQUIRE(promoCapture == 4);
}

TEST_CASE("Rules SAN promotion includes piece suffix", "[rules][promotion]") {
    Board b;
    REQUIRE(b.loadFen("7k/4P3/8/8/8/8/8/4K3 w - - 0 1"));
    MoveList legal; generateLegalMoves(b, legal);
    for (std::size_t i = 0; i < legal.size(); ++i) {
        if (legal[i].isPromotion() && legal[i].promotion == PieceType::Queen) {
            const std::string san = moveToSan(legal[i], b);
            // SAN deve conter '=' (ex: e8=Q ou e8=Q+)
            REQUIRE(san.find('=') != std::string::npos);
            REQUIRE(san.find('Q') != std::string::npos);
            break;
        }
    }
}
