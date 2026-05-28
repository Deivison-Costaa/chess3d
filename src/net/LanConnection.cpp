#include "LanConnection.h"

#include <spdlog/spdlog.h>

#include <cstring>

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
#  include <netdb.h>
#  include <netinet/in.h>
#  include <netinet/tcp.h>
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

    listenSock_ = fromSock(ls);
    spdlog::info("LanConnection: escutando em porta {}", boundPort_);

    // Accept em background para não congelar a UI.
    acceptThread_ = std::thread([this, ls]() {
        sockaddr_in clientAddr{};
        socklen_t   clientLen = sizeof(clientAddr);
        SockT peer = ::accept(ls, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        sockClose(ls);  // fecha o listen socket após aceitar 1 cliente
        listenSock_ = fromSock(kInvalidSock);

        if (!sockValid(peer) || stop_.load()) {
            if (sockValid(peer)) sockClose(peer);
            return;
        }
        int noDelay = 1;
        setsockopt(peer, IPPROTO_TCP, TCP_NODELAY,
                   reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

        char ipBuf[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuf, sizeof(ipBuf));
        spdlog::info("LanConnection: cliente conectado de {}", ipBuf);

        peerSock_ = fromSock(peer);
        connected_.store(true);
        readerLoop();
    });
    return true;
}

bool LanConnection::connectTo(const std::string& host, uint16_t port) {
    SockT s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!sockValid(s)) {
        spdlog::error("LanConnection: socket() failed ({})", sockError());
        return false;
    }

    addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    const std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        spdlog::error("LanConnection: getaddrinfo({}) failed ({})", host, sockError());
        sockClose(s);
        return false;
    }

    if (::connect(s, res->ai_addr, static_cast<int>(res->ai_addrlen)) < 0) {
        spdlog::error("LanConnection: connect({}:{}) failed ({})", host, port, sockError());
        freeaddrinfo(res);
        sockClose(s);
        return false;
    }
    freeaddrinfo(res);

    int noDelay = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY,
               reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));

    spdlog::info("LanConnection: conectado a {}:{}", host, port);
    peerSock_ = fromSock(s);
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

    // Fecha sockets para desbloquear recv() e accept() nas threads.
    const SockT ls = toSock(listenSock_);
    if (sockValid(ls)) { sockClose(ls); listenSock_ = fromSock(kInvalidSock); }
    const SockT ps = toSock(peerSock_);
    if (sockValid(ps)) { sockClose(ps); peerSock_ = fromSock(kInvalidSock); }

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
    const SockT s = toSock(peerSock_);
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
    const SockT s = toSock(peerSock_);
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
