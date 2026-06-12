#include "LanConnection.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstring>
#include <initializer_list>

// ─── Platform abstraction ─────────────────────────────────────────────────────

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>

namespace {
using SockT   = SOCKET;
constexpr SockT kInvalidSock = INVALID_SOCKET;
inline bool sockValid(SockT s)  { return s != INVALID_SOCKET; }
inline void sockClose(SockT s)  { shutdown(s, SD_BOTH); closesocket(s); }
inline int  sockRecv(SockT s, void* buf, int len)
    { return recv(s, reinterpret_cast<char*>(buf), len, 0); }
inline int  sockSend(SockT s, const void* buf, int len)
    { return send(s, reinterpret_cast<const char*>(buf), len, 0); }
inline int  sockError() { return WSAGetLastError(); }
inline bool sockSetNonBlocking(SockT s, bool nb) {
    u_long mode = nb ? 1 : 0;
    return ioctlsocket(s, FIONBIO, &mode) == 0;
}
// 1 = pronto, 0 = timeout, -1 = erro.
inline int sockPollIn(SockT s, int timeoutMs) {
    WSAPOLLFD p{ s, POLLRDNORM, 0 };
    const int n = WSAPoll(&p, 1, timeoutMs);
    if (n < 0) return -1;
    if (n == 0) return 0;
    if (p.revents & (POLLERR | POLLNVAL)) return -1;
    return 1;
}
inline int sockPollOut(SockT s, int timeoutMs) {
    WSAPOLLFD p{ s, POLLWRNORM, 0 };
    const int n = WSAPoll(&p, 1, timeoutMs);
    if (n < 0) return -1;
    if (n == 0) return 0;
    if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) return -1;
    return 1;
}
inline bool sockConnectInProgress() { return WSAGetLastError() == WSAEWOULDBLOCK; }
inline int  sockPendingError(SockT s) {
    int err = 0; int len = sizeof(err);
    getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&err), &len);
    return err;
}

struct WsaGuard {
    WsaGuard()  { WSADATA d{}; WSAStartup(MAKEWORD(2,2), &d); }
    ~WsaGuard() { WSACleanup(); }
};
static WsaGuard g_wsa;
}  // namespace

#else  // POSIX
#  include <arpa/inet.h>
#  include <csignal>
#  include <errno.h>
#  include <fcntl.h>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
#  include <poll.h>
#  include <sys/socket.h>
#  include <sys/types.h>
#  include <unistd.h>

namespace {
using SockT   = int;
constexpr SockT kInvalidSock = -1;
inline bool sockValid(SockT s)  { return s >= 0; }
inline void sockClose(SockT s)  { ::shutdown(s, SHUT_RDWR); ::close(s); }
inline int  sockRecv(SockT s, void* buf, int len)
    { return static_cast<int>(::recv(s, buf, static_cast<std::size_t>(len), 0)); }
inline int  sockSend(SockT s, const void* buf, int len)
    { return static_cast<int>(::send(s, buf, static_cast<std::size_t>(len), 0)); }
inline int  sockError() { return errno; }
inline bool sockSetNonBlocking(SockT s, bool nb) {
    const int fl = fcntl(s, F_GETFL, 0);
    if (fl < 0) return false;
    return fcntl(s, F_SETFL, nb ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK)) == 0;
}
// 1 = pronto, 0 = timeout, -1 = erro.
inline int sockPollIn(SockT s, int timeoutMs) {
    pollfd p{ s, POLLIN, 0 };
    const int n = ::poll(&p, 1, timeoutMs);
    if (n < 0) return (errno == EINTR) ? 0 : -1;
    if (n == 0) return 0;
    if (p.revents & (POLLERR | POLLNVAL)) return -1;
    return 1;
}
inline int sockPollOut(SockT s, int timeoutMs) {
    pollfd p{ s, POLLOUT, 0 };
    const int n = ::poll(&p, 1, timeoutMs);
    if (n < 0) return (errno == EINTR) ? 0 : -1;
    if (n == 0) return 0;
    if (p.revents & (POLLERR | POLLHUP | POLLNVAL)) return -1;
    return 1;
}
inline bool sockConnectInProgress() { return errno == EINPROGRESS; }
inline int  sockPendingError(SockT s) {
    int err = 0; socklen_t len = sizeof(err);
    getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len);
    return err;
}

struct SigPipeGuard {
    SigPipeGuard() { signal(SIGPIPE, SIG_IGN); }
};
static SigPipeGuard g_sigpipe;
}  // namespace

#endif  // _WIN32

// ─── Helpers ─────────────────────────────────────────────────────────────────

namespace {

inline SockT toSock(intptr_t v)   { return static_cast<SockT>(v); }
inline intptr_t fromSock(SockT s) { return static_cast<intptr_t>(s); }

// Escreve 4 bytes big-endian para um buffer (sem incluir <arpa/inet.h> diretamente).
inline void writeBE32(uint8_t* dst, uint32_t v) {
    dst[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
    dst[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
    dst[2] = static_cast<uint8_t>((v >>  8) & 0xFF);
    dst[3] = static_cast<uint8_t>((v >>  0) & 0xFF);
}
inline uint32_t readBE32(const uint8_t* src) {
    return (static_cast<uint32_t>(src[0]) << 24)
         | (static_cast<uint32_t>(src[1]) << 16)
         | (static_cast<uint32_t>(src[2]) <<  8)
         | (static_cast<uint32_t>(src[3]) <<  0);
}

}  // namespace

// ─── LanConnection ───────────────────────────────────────────────────────────

namespace chess3d::net {

LanConnection::LanConnection() = default;

LanConnection::~LanConnection() {
    close();
}

bool LanConnection::startListening(uint16_t port) {
    SockT ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!sockValid(ls)) {
        spdlog::error("LanConnection: socket() failed ({})", sockError());
        return false;
    }

    // SO_REUSEADDR evita "address already in use" em reinicializações rápidas.
    int yes = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(ls, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        spdlog::error("LanConnection: bind() failed on port {} ({})", port, sockError());
        sockClose(ls);
        return false;
    }
    if (listen(ls, 1) < 0) {
        spdlog::error("LanConnection: listen() failed ({})", sockError());
        sockClose(ls);
        return false;
    }

    // Recupera a porta real (caso port==0 → OS escolhe).
    sockaddr_in boundAddr{};
    socklen_t   boundLen = sizeof(boundAddr);
    getsockname(ls, reinterpret_cast<sockaddr*>(&boundAddr), &boundLen);
    boundPort_ = ntohs(boundAddr.sin_port);

    // Accept não-bloqueante com poll: close() só precisa setar stop_ e a thread
    // sai em ≤~100ms — sem depender da semântica frágil de "fechar fd acorda accept".
    sockSetNonBlocking(ls, true);
    listenSock_.store(fromSock(ls));
    spdlog::info("LanConnection: escutando em porta {}", boundPort_);

    acceptThread_ = std::thread([this]() {
        SockT peer = kInvalidSock;
        sockaddr_in clientAddr{};
        while (!stop_.load()) {
            const SockT cur = toSock(listenSock_.load());
            if (!sockValid(cur)) break;
            const int r = sockPollIn(cur, 100);
            if (r < 0) break;
            if (r == 0) continue;
            socklen_t clientLen = sizeof(clientAddr);
            peer = ::accept(cur, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
            if (sockValid(peer)) break;
            if (stop_.load()) break;
            // EAGAIN (poll espúrio) ou erro transitório: tenta de novo.
        }

        // Fecha o listen socket apenas se ainda formos os donos (close() pode tê-lo levado).
        const SockT owned = toSock(listenSock_.exchange(fromSock(kInvalidSock)));
        if (sockValid(owned)) sockClose(owned);

        if (!sockValid(peer)) return;
        if (stop_.load()) { sockClose(peer); return; }

        // No Windows o socket aceito herda o modo não-bloqueante do listen.
        sockSetNonBlocking(peer, false);
        int noDelay = 1;
        setsockopt(peer, IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

        char ipBuf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuf, sizeof(ipBuf));
        spdlog::info("LanConnection: cliente conectado de {}", ipBuf);

        peerSock_.store(fromSock(peer));
        connected_.store(true);
        readerLoop();
    });
    return true;
}

bool LanConnection::connectTo(const std::string& host, uint16_t port, int timeoutMs) {
    SockT s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!sockValid(s)) {
        spdlog::error("LanConnection: socket() failed ({})", sockError());
        return false;
    }

    // AI_NUMERICHOST primeiro (caso de uso é IP literal — não bloqueia em DNS);
    // fallback com resolução completa se não for um IP.
    addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_NUMERICHOST;
    const std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        hints.ai_flags = 0;
        if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
            spdlog::error("LanConnection: getaddrinfo({}) failed ({})", host, sockError());
            sockClose(s);
            return false;
        }
    }

    // Connect não-bloqueante: publica o fd em connectSock_ ANTES, para close()
    // poder abortar de outra thread (fecha o fd → poll retorna erro).
    sockSetNonBlocking(s, true);
    connectSock_.store(fromSock(s));

    const int rc = ::connect(s, res->ai_addr, static_cast<int>(res->ai_addrlen));
    freeaddrinfo(res);

    bool ok = (rc == 0);
    if (!ok && sockConnectInProgress()) {
        const auto deadline = std::chrono::steady_clock::now()
                            + std::chrono::milliseconds(timeoutMs);
        while (!stop_.load()) {
            if (std::chrono::steady_clock::now() >= deadline) break;
            const int r = sockPollOut(s, 100);  // fatias de 100ms re-checam stop_
            if (r < 0) break;
            if (r == 0) continue;
            ok = (sockPendingError(s) == 0);
            break;
        }
    }

    // Handoff de posse: se close() já levou o fd, ele já o fechou — não fechar de novo.
    if (connectSock_.exchange(fromSock(kInvalidSock)) == fromSock(kInvalidSock)) {
        spdlog::info("LanConnection: connect a {}:{} abortado", host, port);
        return false;
    }
    if (!ok || stop_.load()) {
        spdlog::error("LanConnection: connect({}:{}) failed ({})", host, port, sockError());
        sockClose(s);
        return false;
    }

    sockSetNonBlocking(s, false);  // readerLoop usa recv bloqueante
    int noDelay = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
               reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

    spdlog::info("LanConnection: conectado a {}:{}", host, port);
    peerSock_.store(fromSock(s));
    connected_.store(true);

    readerThread_ = std::thread(&LanConnection::readerLoop, this);
    return true;
}

bool LanConnection::sendMessage(const std::string& payload) {
    if (!connected_.load()) return false;
    std::lock_guard<std::mutex> lk(sendMutex_);
    uint8_t header[4];
    writeBE32(header, static_cast<uint32_t>(payload.size()));
    return sendRaw(header, 4) && sendRaw(payload.data(), static_cast<int>(payload.size()));
}

std::optional<std::string> LanConnection::pollIncoming() {
    std::lock_guard<std::mutex> lk(queueMutex_);
    if (incoming_.empty()) return std::nullopt;
    std::string msg = std::move(incoming_.front());
    incoming_.pop();
    return msg;
}

void LanConnection::close() {
    stop_.store(true);
    connected_.store(false);

    // Toma posse e fecha cada socket (exchange garante fechamento único mesmo
    // com as threads internas correndo em paralelo). Fechar o peer desbloqueia
    // o recv() do readerLoop; connect/accept saem pelo stop_ via poll.
    for (std::atomic<intptr_t>* sock : { &connectSock_, &listenSock_, &peerSock_ }) {
        const SockT s = toSock(sock->exchange(fromSock(kInvalidSock)));
        if (sockValid(s)) sockClose(s);
    }

    if (acceptThread_.joinable()) acceptThread_.join();
    if (readerThread_.joinable()) readerThread_.join();
}

std::string LanConnection::localIp() const {
    char buf[INET_ADDRSTRLEN] = "127.0.0.1";
    // Resolução simples via connect UDP (não envia pacotes, só seta a rota).
    SockT tmp = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockValid(tmp)) {
        sockaddr_in remote{};
        remote.sin_family      = AF_INET;
        remote.sin_addr.s_addr = inet_addr("8.8.8.8");
        remote.sin_port        = htons(53);
        if (::connect(tmp, reinterpret_cast<sockaddr*>(&remote), sizeof(remote)) == 0) {
            sockaddr_in local{};
            socklen_t   len = sizeof(local);
            getsockname(tmp, reinterpret_cast<sockaddr*>(&local), &len);
            inet_ntop(AF_INET, &local.sin_addr, buf, sizeof(buf));
        }
        sockClose(tmp);
    }
    return buf;
}

bool LanConnection::sendRaw(const void* data, int len) {
    const SockT s = toSock(peerSock_.load());
    if (!sockValid(s)) return false;
    int sent = 0;
    while (sent < len) {
        const int n = sockSend(s, reinterpret_cast<const char*>(data) + sent, len - sent);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

bool LanConnection::recvExact(void* buf, int len) {
    const SockT s = toSock(peerSock_.load());
    if (!sockValid(s)) return false;
    int received = 0;
    while (received < len) {
        const int n = sockRecv(s, reinterpret_cast<char*>(buf) + received, len - received);
        if (n <= 0) return false;
        received += n;
    }
    return true;
}

void LanConnection::readerLoop() {
    while (!stop_.load()) {
        uint8_t header[4] = {};
        if (!recvExact(header, 4)) break;
        const uint32_t payloadLen = readBE32(header);
        if (payloadLen > 1024 * 1024) break;  // sanidade: rejeita frames gigantes
        if (payloadLen == 0) continue;        // frame vazio (keepalive): ignorar

        std::string payload(payloadLen, '\0');
        if (!recvExact(payload.data(), static_cast<int>(payloadLen))) break;

        {
            std::lock_guard<std::mutex> lk(queueMutex_);
            incoming_.push(std::move(payload));
        }
        queueCv_.notify_all();
    }
    connected_.store(false);
    spdlog::info("LanConnection: reader encerrou (peer desconectado ou erro)");
}

}  // namespace chess3d::net
