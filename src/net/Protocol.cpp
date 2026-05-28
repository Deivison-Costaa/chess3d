#include "Protocol.h"

#include <sstream>
#include <string_view>

namespace chess3d::net::proto {

std::string hello(const std::string& nick) {
    return "HELLO chess3d/1 " + nick;
}

std::string role(chess::Color color) {
    return std::string("ROLE ") + (color == chess::Color::White ? "WHITE" : "BLACK");
}

std::string start(const std::string& fen) {
    return "START " + fen;
}

std::string tc(int whiteMs, int blackMs) {
    return "TC " + std::to_string(whiteMs) + " " + std::to_string(blackMs);
}

std::string move(const std::string& uci, int whiteMs, int blackMs) {
    return "MOVE " + uci + " " + std::to_string(whiteMs) + " " + std::to_string(blackMs);
}

// ─── Helpers ─────────────────────────────────────────────────────────────────

static bool startsWith(const std::string& s, std::string_view prefix) {
    return s.size() >= prefix.size()
        && s.compare(0, prefix.size(), prefix.data(), prefix.size()) == 0;
}

// ─── Parsers ─────────────────────────────────────────────────────────────────

std::optional<std::string> parseHelloNick(const std::string& msg) {
    // "HELLO chess3d/1 <nick>"
    constexpr std::string_view kPrefix = "HELLO chess3d/1 ";
    if (!startsWith(msg, kPrefix)) return std::nullopt;
    return msg.substr(kPrefix.size());
}

std::optional<chess::Color> parseRole(const std::string& msg) {
    // "ROLE WHITE" or "ROLE BLACK"
    if (!startsWith(msg, "ROLE ")) return std::nullopt;
    const std::string colorStr = msg.substr(5);
    if (colorStr == "WHITE") return chess::Color::White;
    if (colorStr == "BLACK") return chess::Color::Black;
    return std::nullopt;
}

std::optional<std::string> parseStartFen(const std::string& msg) {
    // "START <fen>"
    if (!startsWith(msg, "START ")) return std::nullopt;
    return msg.substr(6);
}

std::optional<std::pair<int, int>> parseTc(const std::string& msg) {
    // "TC <whiteMs> <blackMs>"
    if (!startsWith(msg, "TC ")) return std::nullopt;
    std::istringstream ss(msg.substr(3));
    int w = -1, b = -1;
    if (!(ss >> w >> b)) return std::nullopt;
    return std::make_pair(w, b);
}

std::optional<ParsedMove> parseMove(const std::string& msg) {
    // "MOVE <uci> [<whiteMs> <blackMs>]"
    if (!startsWith(msg, "MOVE ")) return std::nullopt;
    std::istringstream ss(msg.substr(5));
    ParsedMove pm;
    if (!(ss >> pm.uci)) return std::nullopt;
    // clocks are optional
    ss >> pm.whiteMs >> pm.blackMs;
    return pm;
}

bool isControl(const std::string& msg) {
    return msg == "RESIGN" || msg == "BYE" || msg == "DRAW_OFFER";
}

}  // namespace chess3d::net::proto
