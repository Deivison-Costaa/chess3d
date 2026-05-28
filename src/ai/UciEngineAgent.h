#pragma once

#include "Agent.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace chess3d::ai {

// Cliente UCI genérico — funciona com Stockfish, Berserk, ou qualquer engine
// que fale o protocolo UCI. Comunica via pipes anônimos (Win32 ou POSIX) + uma
// thread leitora para evitar deadlock quando o engine não responde.
class UciEngineAgent : public Agent {
public:
    // setOptions: pares (name, value) enviados como `setoption name <k> value <v>`
    // no handshake (ex: {{"EvalFile", ".../berserk.nn"}} pra Berserk).
    // displayNameOverride: se vazio, usa o `id name` reportado pelo engine.
    explicit UciEngineAgent(std::filesystem::path enginePath,
                            std::string displayNameOverride = {},
                            std::vector<std::pair<std::string, std::string>> setOptions = {},
                            int moveTimeMs = 1000);
    ~UciEngineAgent() override;

    UciEngineAgent(const UciEngineAgent&) = delete;
    UciEngineAgent& operator=(const UciEngineAgent&) = delete;

    bool ok() const { return ok_; }
    std::string name() const override { return name_; }

    chess::Move chooseMove(chess::Board& board) override;
    SearchInfo lastInfo() const override { return info_; }

private:
    bool spawn(const std::filesystem::path& enginePath);
    void shutdown();

    void readerLoop();
    void writeLine(const std::string& s);
    // Espera linha que satisfaça `predicate(line)`. Retorna a linha ou "" no timeout.
    template <typename Pred>
    std::string waitForLine(Pred pred, std::chrono::milliseconds timeout);

    void parseInfoLine(const std::string& line);
    chess::Move uciToMove(const std::string& uci, chess::Board& board) const;

    // Process/fd handles: void* mascara HANDLE (Win32) ou pid_t/int (POSIX).
    void* hProcess_  = nullptr;
    void* hChildStdoutRd_ = nullptr;
    void* hChildStdinWr_  = nullptr;

    std::thread reader_;
    std::atomic<bool> stop_{false};

    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::deque<std::string> lines_;

    int moveTimeMs_;
    bool ok_ = false;
    std::string name_;
    SearchInfo info_;
};

}  // namespace chess3d::ai
