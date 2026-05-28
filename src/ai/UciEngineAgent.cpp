#include "UciEngineAgent.h"

#include "chess/MoveGenerator.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <sstream>

// ─── Platform-specific pipe / process I/O ────────────────────────────────────

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace chess3d::ai {
namespace {
inline HANDLE asHandle(void* p) { return reinterpret_cast<HANDLE>(p); }
inline void*  toVoid(HANDLE h)  { return reinterpret_cast<void*>(h); }
}  // namespace

bool UciEngineAgent::spawn(const std::filesystem::path& enginePath) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE childStdoutRd = nullptr, childStdoutWr = nullptr;
    HANDLE childStdinRd  = nullptr, childStdinWr  = nullptr;

    if (!CreatePipe(&childStdoutRd, &childStdoutWr, &sa, 0)) return false;
    if (!SetHandleInformation(childStdoutRd, HANDLE_FLAG_INHERIT, 0)) return false;
    if (!CreatePipe(&childStdinRd, &childStdinWr, &sa, 0)) return false;
    if (!SetHandleInformation(childStdinWr, HANDLE_FLAG_INHERIT, 0)) return false;

    STARTUPINFOW si{};
    si.cb          = sizeof(si);
    si.hStdError   = childStdoutWr;
    si.hStdOutput  = childStdoutWr;
    si.hStdInput   = childStdinRd;
    si.dwFlags     = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::wstring cmd = enginePath.wstring();
    std::vector<wchar_t> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back(L'\0');

    std::wstring cwd    = enginePath.parent_path().wstring();
    const wchar_t* cwdPtr = cwd.empty() ? nullptr : cwd.c_str();

    const BOOL ok = CreateProcessW(
        nullptr, mutableCmd.data(), nullptr, nullptr,
        TRUE, CREATE_NO_WINDOW, nullptr, cwdPtr, &si, &pi);
    if (!ok) {
        CloseHandle(childStdoutRd); CloseHandle(childStdoutWr);
        CloseHandle(childStdinRd);  CloseHandle(childStdinWr);
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(childStdoutWr);
    CloseHandle(childStdinRd);

    hProcess_       = toVoid(pi.hProcess);
    hChildStdoutRd_ = toVoid(childStdoutRd);
    hChildStdinWr_  = toVoid(childStdinWr);

    reader_ = std::thread(&UciEngineAgent::readerLoop, this);
    return true;
}

void UciEngineAgent::shutdown() {
    if (!hProcess_) return;
    try { writeLine("quit"); } catch (...) {}
    WaitForSingleObject(asHandle(hProcess_), 1500);
    TerminateProcess(asHandle(hProcess_), 0);
    stop_ = true;
    if (asHandle(hChildStdinWr_))  CloseHandle(asHandle(hChildStdinWr_));
    if (asHandle(hChildStdoutRd_)) CloseHandle(asHandle(hChildStdoutRd_));
    if (reader_.joinable()) reader_.join();
    CloseHandle(asHandle(hProcess_));
    hProcess_ = hChildStdoutRd_ = hChildStdinWr_ = nullptr;
}

void UciEngineAgent::writeLine(const std::string& s) {
    if (!hChildStdinWr_) return;
    const std::string line = s + "\n";
    DWORD written = 0;
    WriteFile(asHandle(hChildStdinWr_), line.data(),
              static_cast<DWORD>(line.size()), &written, nullptr);
}

void UciEngineAgent::readerLoop() {
    std::string buf;
    char chunk[512];
    DWORD got = 0;
    while (!stop_) {
        if (!ReadFile(asHandle(hChildStdoutRd_), chunk, sizeof(chunk), &got, nullptr)
            || got == 0) {
            break;
        }
        buf.append(chunk, got);
        std::size_t pos = 0;
        while (true) {
            const std::size_t nl = buf.find('\n', pos);
            if (nl == std::string::npos) break;
            std::string line = buf.substr(pos, nl - pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            {
                std::lock_guard<std::mutex> lk(queueMutex_);
                lines_.push_back(std::move(line));
            }
            queueCv_.notify_all();
            pos = nl + 1;
        }
        if (pos > 0) buf.erase(0, pos);
    }
}

}  // namespace chess3d::ai

#else  // POSIX (Linux, macOS)

#include <csignal>
#include <fcntl.h>
#include <mutex>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace chess3d::ai {
namespace {
inline pid_t asPid(void* p) { return static_cast<pid_t>(reinterpret_cast<intptr_t>(p)); }
inline void* toVoidPid(pid_t p) { return reinterpret_cast<void*>(static_cast<intptr_t>(p)); }
inline int   asFd(void* p) { return static_cast<int>(reinterpret_cast<intptr_t>(p)); }
inline void* toVoidFd(int fd) { return reinterpret_cast<void*>(static_cast<intptr_t>(fd)); }
}  // namespace

bool UciEngineAgent::spawn(const std::filesystem::path& enginePath) {
    // Ignore SIGPIPE in parent so a closed engine stdin gives EPIPE, not SIGKILL.
    static std::once_flag sigOnce;
    std::call_once(sigOnce, [] { signal(SIGPIPE, SIG_IGN); });

    int stdoutPipe[2], stdinPipe[2];
    if (pipe(stdoutPipe) < 0) return false;
    if (pipe(stdinPipe)  < 0) { close(stdoutPipe[0]); close(stdoutPipe[1]); return false; }

    // Mark parent-side fds close-on-exec so they don't leak into grandchildren.
    fcntl(stdoutPipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(stdinPipe[1],  F_SETFD, FD_CLOEXEC);

    const pid_t pid = fork();
    if (pid < 0) {
        close(stdoutPipe[0]); close(stdoutPipe[1]);
        close(stdinPipe[0]);  close(stdinPipe[1]);
        return false;
    }

    if (pid == 0) {
        // Child: rewire stdio and exec the engine.
        dup2(stdinPipe[0],  STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stdoutPipe[1], STDERR_FILENO);  // merge stderr so we see engine errors in our log
        close(stdinPipe[0]); close(stdinPipe[1]);
        close(stdoutPipe[0]); close(stdoutPipe[1]);

        // CWD = engine directory so relative weight paths work (e.g., Lc0).
        const auto dir = enginePath.parent_path();
        if (!dir.empty()) chdir(dir.string().c_str());

        const std::string exe = enginePath.string();
        char* const argv[] = { const_cast<char*>(exe.c_str()), nullptr };
        execvp(exe.c_str(), argv);
        _exit(127);  // exec failed
    }

    // Parent: close child-side fds.
    close(stdoutPipe[1]);
    close(stdinPipe[0]);

    hProcess_       = toVoidPid(pid);
    hChildStdoutRd_ = toVoidFd(stdoutPipe[0]);
    hChildStdinWr_  = toVoidFd(stdinPipe[1]);

    reader_ = std::thread(&UciEngineAgent::readerLoop, this);
    return true;
}

void UciEngineAgent::shutdown() {
    if (!hProcess_) return;
    try { writeLine("quit"); } catch (...) {}

    // Give engine up to ~1.5s to exit gracefully.
    const pid_t pid = asPid(hProcess_);
    for (int i = 0; i < 15; ++i) {
        if (waitpid(pid, nullptr, WNOHANG) == pid) goto cleaned;
        usleep(100'000);  // 100ms
    }
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);

cleaned:
    stop_ = true;
    if (hChildStdinWr_)  close(asFd(hChildStdinWr_));
    if (hChildStdoutRd_) close(asFd(hChildStdoutRd_));  // unblocks reader
    if (reader_.joinable()) reader_.join();
    hProcess_ = hChildStdoutRd_ = hChildStdinWr_ = nullptr;
}

void UciEngineAgent::writeLine(const std::string& s) {
    if (!hChildStdinWr_) return;
    const std::string line = s + "\n";
    const int fd = asFd(hChildStdinWr_);
    std::size_t written = 0;
    while (written < line.size()) {
        const ssize_t n = write(fd, line.data() + written, line.size() - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            return;
        }
        written += static_cast<std::size_t>(n);
    }
}

void UciEngineAgent::readerLoop() {
    const int fd = asFd(hChildStdoutRd_);
    std::string buf;
    char chunk[512];
    while (!stop_) {
        ssize_t got;
        do { got = read(fd, chunk, sizeof(chunk)); } while (got < 0 && errno == EINTR);
        if (got <= 0) break;
        buf.append(chunk, static_cast<std::size_t>(got));
        std::size_t pos = 0;
        while (true) {
            const std::size_t nl = buf.find('\n', pos);
            if (nl == std::string::npos) break;
            std::string line = buf.substr(pos, nl - pos);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            {
                std::lock_guard<std::mutex> lk(queueMutex_);
                lines_.push_back(std::move(line));
            }
            queueCv_.notify_all();
            pos = nl + 1;
        }
        if (pos > 0) buf.erase(0, pos);
    }
}

}  // namespace chess3d::ai

#endif  // _WIN32

// ─── Common code (platform-independent) ──────────────────────────────────────

namespace chess3d::ai {

UciEngineAgent::UciEngineAgent(std::filesystem::path enginePath,
                               std::string displayNameOverride,
                               std::vector<std::pair<std::string, std::string>> setOptions,
                               int moveTimeMs)
    : moveTimeMs_(moveTimeMs),
      name_(displayNameOverride.empty() ? std::string("UCI Engine") : displayNameOverride) {
    if (!spawn(enginePath)) {
        spdlog::error("UciEngineAgent: failed to spawn {}", enginePath.string());
        return;
    }

    writeLine("uci");
    const std::string uciOk = waitForLine(
        [](const std::string& l) { return l == "uciok"; },
        std::chrono::milliseconds(5000));
    if (uciOk != "uciok") {
        spdlog::error("UciEngineAgent: uciok não recebido de {}", enginePath.string());
        return;
    }
    if (displayNameOverride.empty()) {
        std::unique_lock<std::mutex> lk(queueMutex_);
        for (const auto& l : lines_) {
            if (l.rfind("id name ", 0) == 0) { name_ = l.substr(8); break; }
        }
    }
    {
        std::lock_guard<std::mutex> lk(queueMutex_);
        lines_.clear();
    }

    for (const auto& [k, v] : setOptions) {
        writeLine("setoption name " + k + " value " + v);
    }

    writeLine("isready");
    const std::string ready = waitForLine(
        [](const std::string& l) { return l == "readyok"; },
        std::chrono::milliseconds(10000));
    if (ready != "readyok") {
        spdlog::error("UciEngineAgent: readyok não recebido de {}", name_);
        return;
    }

    ok_ = true;
    spdlog::info("UciEngineAgent: pronto ({}, movetime={}ms)", name_, moveTimeMs_);
}

UciEngineAgent::~UciEngineAgent() {
    shutdown();
}

template <typename Pred>
std::string UciEngineAgent::waitForLine(Pred pred, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::unique_lock<std::mutex> lk(queueMutex_);
    for (;;) {
        for (auto it = lines_.begin(); it != lines_.end(); ++it) {
            if (pred(*it)) {
                std::string match = std::move(*it);
                lines_.erase(it);
                return match;
            }
        }
        if (std::chrono::steady_clock::now() >= deadline) return {};
        queueCv_.wait_until(lk, deadline);
    }
}

void UciEngineAgent::parseInfoLine(const std::string& line) {
    std::istringstream is(line);
    std::string tok;
    is >> tok;  // "info"
    if (tok != "info") return;
    while (is >> tok) {
        if (tok == "depth") { is >> info_.depthReached; }
        else if (tok == "nodes") { is >> info_.nodesVisited; }
        else if (tok == "score") {
            std::string kind; is >> kind;
            if (kind == "cp") { is >> info_.evaluation; }
            else if (kind == "mate") {
                int n; is >> n;
                info_.evaluation = (n > 0 ? 100000 - n : -100000 - n);
            }
        } else if (tok == "time") {
            long ms; is >> ms;
            info_.elapsed = std::chrono::microseconds(ms * 1000);
        } else if (tok == "pv") {
            break;
        }
    }
}

chess::Move UciEngineAgent::uciToMove(const std::string& uci, chess::Board& board) const {
    if (uci.size() < 4) return {};
    const int fromFile = uci[0] - 'a';
    const int fromRank = uci[1] - '1';
    const int toFile   = uci[2] - 'a';
    const int toRank   = uci[3] - '1';
    if (fromFile < 0 || fromFile > 7 || fromRank < 0 || fromRank > 7 ||
        toFile   < 0 || toFile   > 7 || toRank   < 0 || toRank   > 7) {
        return {};
    }
    const chess::Square from = chess::makeSquare(fromFile, fromRank);
    const chess::Square to   = chess::makeSquare(toFile,   toRank);
    chess::PieceType promo = chess::PieceType::None;
    if (uci.size() >= 5) {
        switch (uci[4]) {
            case 'q': promo = chess::PieceType::Queen;  break;
            case 'r': promo = chess::PieceType::Rook;   break;
            case 'b': promo = chess::PieceType::Bishop; break;
            case 'n': promo = chess::PieceType::Knight; break;
            default: break;
        }
    }
    chess::MoveList legal;
    chess::generateLegalMoves(board, legal);
    for (std::size_t i = 0; i < legal.size(); ++i) {
        const auto& m = legal[i];
        if (m.from == from && m.to == to && m.promotion == promo) return m;
    }
    return {};
}

chess::Move UciEngineAgent::chooseMove(chess::Board& board) {
    info_ = SearchInfo{};
    if (!ok_) return {};

    {
        std::lock_guard<std::mutex> lk(queueMutex_);
        lines_.clear();
    }

    writeLine("position fen " + board.toFen());
    writeLine("go movetime " + std::to_string(moveTimeMs_));

    const auto t0       = std::chrono::steady_clock::now();
    const auto deadline = t0 + std::chrono::milliseconds(moveTimeMs_ + 5000);
    std::string bestmove;
    while (std::chrono::steady_clock::now() < deadline && bestmove.empty()) {
        std::unique_lock<std::mutex> lk(queueMutex_);
        if (lines_.empty()) {
            queueCv_.wait_until(lk, deadline);
            continue;
        }
        std::string line = std::move(lines_.front());
        lines_.pop_front();
        lk.unlock();
        if (line.rfind("info ", 0) == 0) {
            parseInfoLine(line);
        } else if (line.rfind("bestmove ", 0) == 0) {
            bestmove = line.substr(9);
        }
    }
    info_.elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0);
    if (bestmove.empty()) {
        spdlog::warn("UciEngineAgent({}): bestmove timeout", name_);
        return {};
    }
    const std::size_t sp  = bestmove.find(' ');
    const std::string uci = (sp == std::string::npos) ? bestmove : bestmove.substr(0, sp);
    return uciToMove(uci, board);
}

}  // namespace chess3d::ai
