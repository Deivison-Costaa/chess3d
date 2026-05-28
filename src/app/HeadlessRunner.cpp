#include "HeadlessRunner.h"

#include "GameSession.h"
#include "ai/DifficultyLevels.h"
#include "chess/Notation.h"
#include "chess/Rules.h"
#include "ui/GameUi.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <string_view>
#include <thread>

namespace chess3d {

namespace {

ai::AgentSpec agentSpecFromName(const std::string& name) {
    static const std::map<std::string, ai::AgentSpec::Engine> kMap = {
        {"easy",      ai::AgentSpec::Engine::MinimaxEasy},
        {"medium",    ai::AgentSpec::Engine::MinimaxMedium},
        {"hard",      ai::AgentSpec::Engine::MinimaxHard},
        {"stockfish", ai::AgentSpec::Engine::Stockfish},
        {"lc0",       ai::AgentSpec::Engine::Lc0},
        {"berserk",   ai::AgentSpec::Engine::Berserk},
    };
    ai::AgentSpec spec;
    auto it = kMap.find(name);
    if (it != kMap.end()) spec.engine = it->second;
    return spec;
}

ui::GameSetup buildSetup(const HeadlessConfig& cfg) {
    ui::GameSetup setup;

    if (cfg.mode == "ai-vs-ai") {
        setup.mode = ui::GameMode::AiVsAi;
    } else if (cfg.mode == "hotseat") {
        setup.mode = ui::GameMode::Hotseat;
    } else if (cfg.mode == "lan-host") {
        setup.mode = ui::GameMode::LanHost;
    } else if (cfg.mode == "lan-client") {
        setup.mode = ui::GameMode::LanClient;
    } else {
        setup.mode = ui::GameMode::HumanVsAi;
    }

    setup.whiteAgent = agentSpecFromName(cfg.white);
    setup.blackAgent = agentSpecFromName(cfg.black);
    setup.humanColor = chess::Color::White;  // default; overridden for LAN client by handshake
    setup.lanPort    = static_cast<int>(cfg.lanPort);
    setup.lanHost    = cfg.lanHostIp;
    std::strncpy(setup.lanNick, cfg.nick.c_str(), sizeof(setup.lanNick) - 1);
    setup.lanNick[sizeof(setup.lanNick) - 1] = '\0';

    if (cfg.timeMs > 0) {
        setup.timeControl.initialMs   = cfg.timeMs;
        setup.timeControl.incrementMs = cfg.incMs;
    }

    return setup;
}

void savePgn(const std::string& path, const GameSession& session,
             const HeadlessConfig& cfg) {
    std::ofstream f(path);
    if (!f.is_open()) {
        spdlog::warn("HeadlessRunner: cannot write PGN to '{}'", path);
        return;
    }
    // Minimal PGN headers
    f << "[Event \"HeadlessGame\"]\n";
    f << "[Site \"localhost\"]\n";
    f << "[White \"" << cfg.white << "\"]\n";
    f << "[Black \"" << cfg.black << "\"]\n";
    const auto r = session.result();
    const char* resultStr = "*";
    if (r == chess::GameResult::WhiteWins || r == chess::GameResult::WhiteWinsOnTime)
        resultStr = "1-0";
    else if (r == chess::GameResult::BlackWins || r == chess::GameResult::BlackWinsOnTime)
        resultStr = "0-1";
    else if (r != chess::GameResult::Ongoing)
        resultStr = "1/2-1/2";
    f << "[Result \"" << resultStr << "\"]\n\n";

    // Moves
    const auto& played = session.played();
    for (std::size_t i = 0; i < played.size(); ++i) {
        if (i % 2 == 0) f << (i / 2 + 1) << ". ";
        f << played[i].san << " ";
    }
    f << resultStr << "\n";
}

}  // namespace

// ─── CLI parser ──────────────────────────────────────────────────────────────

bool parseHeadlessCli(int argc, char** argv, HeadlessConfig& out) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        auto next = [&]() -> std::string {
            if (i + 1 < argc) return argv[++i];
            return {};
        };

        if (arg == "--headless" || arg == "--auto") {
            // --auto implies headless ai-vs-ai; already set if mode not overridden
            if (arg == "--auto" && out.mode.empty()) out.mode = "ai-vs-ai";
        } else if (arg == "--mode")         { out.mode       = next(); }
        else if (arg == "--white")          { out.white      = next(); }
        else if (arg == "--black")          { out.black      = next(); }
        else if (arg == "--max-plies")      { out.maxPlies   = std::stoi(next()); }
        else if (arg == "--start-fen")      { out.startFen   = next(); }
        else if (arg == "--time")           { out.timeMs     = std::stoi(next()); }
        else if (arg == "--inc")            { out.incMs      = std::stoi(next()); }
        else if (arg == "--lan-port")       { out.lanPort    = static_cast<uint16_t>(std::stoi(next())); }
        else if (arg == "--lan-host-ip")    { out.lanHostIp  = next(); }
        else if (arg == "--nick")           { out.nick       = next(); }
        else if (arg == "--save-pgn")       { out.savePgn    = next(); }
        else if (arg == "--print-result")   { out.printResult = true; }
        else if (arg == "--moves") {
            // Comma-separated UCI moves
            std::string mlist = next();
            std::string mv;
            for (char c : mlist) {
                if (c == ',') { if (!mv.empty()) { out.moves.push_back(mv); mv.clear(); } }
                else mv += c;
            }
            if (!mv.empty()) out.moves.push_back(mv);
        }
    }
    if (out.mode.empty()) out.mode = "ai-vs-ai";
    return true;
}

// ─── Runner ──────────────────────────────────────────────────────────────────

int runHeadless(const HeadlessConfig& cfg) {
    spdlog::info("HeadlessRunner: mode={} white={} black={} maxPlies={}",
                 cfg.mode, cfg.white, cfg.black, cfg.maxPlies);

    const ui::GameSetup setup = buildSetup(cfg);
    GameSession session;
    if (!session.start(setup)) {
        spdlog::error("HeadlessRunner: failed to start game session");
        return 1;
    }

    // Apply pre-loaded moves (from --moves flag)
    for (const auto& uci : cfg.moves) {
        if (session.isOver()) break;
        if (!session.submitMove(uci)) {
            spdlog::error("HeadlessRunner: illegal pre-loaded move '{}'", uci);
            return 1;
        }
        session.tick(0);  // let AI respond if needed
    }

    // Main game loop
    int plies = static_cast<int>(session.played().size());
    while (!session.isOver() && plies < cfg.maxPlies) {
        session.tick(0);
        const int newPlies = static_cast<int>(session.played().size());
        if (newPlies == plies) {
            // No progress (human turn in headless mode without moves input)
            break;
        }
        plies = newPlies;
    }

    const auto result = session.result();
    const char* resultName = chess::gameResultName(result);

    if (cfg.printResult) {
        std::printf("RESULT %s %d\n", resultName, plies);
        std::fflush(stdout);
    }
    spdlog::info("HeadlessRunner: {} after {} plies", resultName, plies);

    if (!cfg.savePgn.empty()) {
        savePgn(cfg.savePgn, session, cfg);
    }

    // In LAN mode, give the TCP stack time to flush the last move to the peer
    // before the connection destructor fires and aborts the socket.
    if (session.lanMode()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    return 0;
}

}  // namespace chess3d
