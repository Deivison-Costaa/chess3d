#pragma once

#include "chess/Types.h"

#include <optional>
#include <string>
#include <utility>

namespace chess3d::net::proto {

// ─── Formatters ──────────────────────────────────────────────────────────────

std::string hello(const std::string& nick);
std::string role(chess::Color color);
std::string start(const std::string& fen);
std::string tc(int whiteMs, int blackMs);
std::string move(const std::string& uci, int whiteMs, int blackMs);

// ─── Parsers ─────────────────────────────────────────────────────────────────

// Returns the nick from "HELLO chess3d/1 <nick>", or nullopt.
std::optional<std::string> parseHelloNick(const std::string& msg);

// Returns the color from "ROLE WHITE|BLACK", or nullopt.
std::optional<chess::Color> parseRole(const std::string& msg);

// Returns the FEN from "START <fen>", or nullopt.
std::optional<std::string> parseStartFen(const std::string& msg);

// Returns {whiteMs, blackMs} from "TC <w> <b>", or nullopt.
std::optional<std::pair<int, int>> parseTc(const std::string& msg);

// Parsed MOVE message: UCI string plus optional clock values (-1 if absent).
struct ParsedMove {
    std::string uci;
    int whiteMs = -1;
    int blackMs = -1;
};
// Returns ParsedMove from "MOVE <uci> [<whiteMs> <blackMs>]", or nullopt.
std::optional<ParsedMove> parseMove(const std::string& msg);

// Returns true for RESIGN, BYE, DRAW_OFFER.
bool isControl(const std::string& msg);

}  // namespace chess3d::net::proto
