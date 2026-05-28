#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>

namespace chess3d::net {

// Wrapper TCP bloqueante cross-platform (Win32 Winsock / POSIX BSD sockets).
// Suporta um único peer (1:1). Thread reader interna elimina bloqueio no recv.
//
// Protocolo de framing: cada mensagem é precedida de 4 bytes big-endian
// indicando o tamanho do payload em bytes, seguidos pelo payload (sem \0).
class LanConnection {
public:
    LanConnection();
    ~LanConnection();

    LanConnection(const LanConnection&) = delete;
    LanConnection& operator=(const LanConnection&) = delete;

    // Abre um socket de escuta na porta e aguarda 1 cliente numa thread.
    // Retorna imediatamente; use isConnected() para saber quando o cliente chegou.
    // Chame em modo host: antes de mostrar o Lobby.
    bool startListening(uint16_t port);

    // Conecta ao host:port de forma bloqueante (sem thread).
    // Deve ser chamado de uma thread separada se não quiser congelar a UI.
    bool connectTo(const std::string& host, uint16_t port);

    // Envia mensagem (thread-safe). Retorna false se não conectado.
    bool sendMessage(const std::string& payload);

    // Drena UMA mensagem da fila interna (não-bloqueante).
    // Retorna nullopt se não houver mensagem disponível.
    std::optional<std::string> pollIncoming();

    bool isConnected() const { return connected_.load(); }

    // Fecha a conexão e espera a thread reader terminar.
    void close();

    // IP local no qual startListening fez bind (para mostrar ao usuário).
    std::string localIp() const;

private:
    void readerLoop();
    bool sendRaw(const void* data, int len);
    bool recvExact(void* buf, int len);

    std::thread acceptThread_;
    std::thread readerThread_;

    std::atomic<bool> connected_{false};
    std::atomic<bool> stop_{false};

    mutable std::mutex queueMutex_;
    std::queue<std::string> incoming_;
    std::condition_variable queueCv_;

    std::mutex sendMutex_;

    // Sockets mantidos como intptr_t para evitar vazar headers de plataforma.
    // Win32: SOCKET (UINT_PTR); POSIX: int (-1 = inválido).
    intptr_t listenSock_{-1};
    intptr_t peerSock_{-1};

    uint16_t boundPort_{0};
};

}  // namespace chess3d::net
