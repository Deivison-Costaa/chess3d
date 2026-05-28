#include "net/Protocol.h"
#include "chess/Types.h"

#include <catch2/catch_test_macros.hpp>

using namespace chess3d::net::proto;
using namespace chess3d::chess;

// ---- hello -----------------------------------------------------------------

TEST_CASE("Protocol hello format", "[protocol]") {
    REQUIRE(hello("alice")  == "HELLO chess3d/1 alice");
    REQUIRE(hello("bot123") == "HELLO chess3d/1 bot123");
    REQUIRE(hello("")       == "HELLO chess3d/1 ");
}

TEST_CASE("Protocol hello parse", "[protocol]") {
    auto n1 = parseHelloNick("HELLO chess3d/1 alice");
    REQUIRE(n1.has_value());
    REQUIRE(*n1 == "alice");

    auto n2 = parseHelloNick("HELLO chess3d/1 ");
    REQUIRE(n2.has_value());
    REQUIRE(*n2 == "");

    REQUIRE_FALSE(parseHelloNick("HELLO").has_value());
    REQUIRE_FALSE(parseHelloNick("WORLD chess3d/1 x").has_value());
    REQUIRE_FALSE(parseHelloNick("").has_value());
}

TEST_CASE("Protocol hello round-trip", "[protocol]") {
    const std::string nick = "jogador_1";
    auto parsed = parseHelloNick(hello(nick));
    REQUIRE(parsed.has_value());
    REQUIRE(*parsed == nick);
}

// ---- role ------------------------------------------------------------------

TEST_CASE("Protocol role format", "[protocol]") {
    REQUIRE(role(Color::White) == "ROLE WHITE");
    REQUIRE(role(Color::Black) == "ROLE BLACK");
}

TEST_CASE("Protocol role parse", "[protocol]") {
    auto c1 = parseRole("ROLE WHITE");
    REQUIRE(c1.has_value());
    REQUIRE(*c1 == Color::White);

    auto c2 = parseRole("ROLE BLACK");
    REQUIRE(c2.has_value());
    REQUIRE(*c2 == Color::Black);

    REQUIRE_FALSE(parseRole("ROLE BOTH").has_value());
    REQUIRE_FALSE(parseRole("ROLE").has_value());
    REQUIRE_FALSE(parseRole("").has_value());
    REQUIRE_FALSE(parseRole("role white").has_value());  // case sensitive
}

TEST_CASE("Protocol role round-trip", "[protocol]") {
    REQUIRE(*parseRole(role(Color::White)) == Color::White);
    REQUIRE(*parseRole(role(Color::Black)) == Color::Black);
}

// ---- start -----------------------------------------------------------------

TEST_CASE("Protocol start format and parse", "[protocol]") {
    const std::string fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    const std::string msg = start(fen);
    REQUIRE(msg == "START " + fen);

    auto parsed = parseStartFen(msg);
    REQUIRE(parsed.has_value());
    REQUIRE(*parsed == fen);

    REQUIRE_FALSE(parseStartFen("STARTX fen").has_value());
}

// ---- tc --------------------------------------------------------------------

TEST_CASE("Protocol tc format", "[protocol]") {
    REQUIRE(tc(-1, 0)          == "TC -1 0");
    REQUIRE(tc(300000, 300000) == "TC 300000 300000");
    REQUIRE(tc(0, 0)           == "TC 0 0");
}

TEST_CASE("Protocol tc parse", "[protocol]") {
    auto p1 = parseTc("TC -1 0");
    REQUIRE(p1.has_value());
    REQUIRE(p1->first  == -1);
    REQUIRE(p1->second ==  0);

    auto p2 = parseTc("TC 300000 300000");
    REQUIRE(p2.has_value());
    REQUIRE(p2->first  == 300000);
    REQUIRE(p2->second == 300000);

    REQUIRE_FALSE(parseTc("TC").has_value());
    REQUIRE_FALSE(parseTc("TC 300000").has_value());
    REQUIRE_FALSE(parseTc("XC 0 0").has_value());
}

TEST_CASE("Protocol tc round-trip", "[protocol]") {
    const int w = 300000, b = 299500;
    auto p = parseTc(tc(w, b));
    REQUIRE(p.has_value());
    REQUIRE(p->first  == w);
    REQUIRE(p->second == b);
}

// ---- move ------------------------------------------------------------------

TEST_CASE("Protocol move format", "[protocol]") {
    REQUIRE(move("e2e4", 5000, 4980) == "MOVE e2e4 5000 4980");
    REQUIRE(move("e7e8q", -1, -1)    == "MOVE e7e8q -1 -1");
}

TEST_CASE("Protocol move parse with clocks", "[protocol]") {
    auto p1 = parseMove("MOVE e2e4 5000 4980");
    REQUIRE(p1.has_value());
    REQUIRE(p1->uci     == "e2e4");
    REQUIRE(p1->whiteMs == 5000);
    REQUIRE(p1->blackMs == 4980);
}

TEST_CASE("Protocol move parse without clocks", "[protocol]") {
    auto p = parseMove("MOVE e7e5");
    REQUIRE(p.has_value());
    REQUIRE(p->uci     == "e7e5");
    REQUIRE(p->whiteMs == -1);
    REQUIRE(p->blackMs == -1);
}

TEST_CASE("Protocol move parse promotion UCI", "[protocol]") {
    auto p = parseMove("MOVE e7e8q 1000 2000");
    REQUIRE(p.has_value());
    REQUIRE(p->uci == "e7e8q");
}

TEST_CASE("Protocol move parse rejects malformed", "[protocol]") {
    REQUIRE_FALSE(parseMove("MOVE").has_value());
    REQUIRE_FALSE(parseMove("MOV e2e4").has_value());
    REQUIRE_FALSE(parseMove("").has_value());
}

TEST_CASE("Protocol move round-trip", "[protocol]") {
    const std::string uci = "d2d4";
    const int w = 30000, bk = 29900;
    auto p = parseMove(move(uci, w, bk));
    REQUIRE(p.has_value());
    REQUIRE(p->uci     == uci);
    REQUIRE(p->whiteMs == w);
    REQUIRE(p->blackMs == bk);
}

// ---- isControl -------------------------------------------------------------

TEST_CASE("Protocol isControl recognizes control messages", "[protocol]") {
    REQUIRE(isControl("RESIGN"));
    REQUIRE(isControl("BYE"));
    REQUIRE(isControl("DRAW_OFFER"));
}

TEST_CASE("Protocol isControl rejects non-control messages", "[protocol]") {
    REQUIRE_FALSE(isControl("MOVE e2e4"));
    REQUIRE_FALSE(isControl("HELLO chess3d/1 x"));
    REQUIRE_FALSE(isControl("ROLE WHITE"));
    REQUIRE_FALSE(isControl("START rnbq..."));
    REQUIRE_FALSE(isControl("TC -1 0"));
    REQUIRE_FALSE(isControl(""));
    REQUIRE_FALSE(isControl("resign"));  // case sensitive
}
