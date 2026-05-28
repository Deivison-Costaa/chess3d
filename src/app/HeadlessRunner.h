#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace chess3d {

struct HeadlessConfig {
    // mode: "ai-vs-ai", "human-vs-ai", "hotseat", "lan-host", "lan-client"
    std::string mode = "ai-vs-ai";
    // engine names: "human", "easy", "medium", "hard", "stockfish", "berserk"
    std::string white = "easy";
    std::string black = "easy";

    int maxPlies     = 400;
    std::string startFen;         // empty = standard start
    int timeMs       = 0;         // 0 = no clock
    int incMs        = 0;
    uint16_t lanPort = 5021;
    std::string lanHostIp = "127.0.0.1";
    std::string nick  = "bot";
    std::string savePgn;          // path to write PGN; empty = skip
    bool printResult = false;     // print "RESULT <name> <plies>" to stdout

    std::vector<std::string> moves;  // pre-applied UCI moves before AI loop
};

// Parses argv into a HeadlessConfig. Returns false on parse error (prints usage).
bool parseHeadlessCli(int argc, char** argv, HeadlessConfig& out);

// Runs the headless game loop. Returns 0 on success.
int runHeadless(const HeadlessConfig& cfg);

}  // namespace chess3d
