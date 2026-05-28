#pragma once

#include "Agent.h"
#include "net/LanConnection.h"

#include <atomic>
#include <mutex>
#include <queue>
#include <string>

namespace chess3d::ai {

// Implementação de Agent que espera pelo lance do jogador remoto via LanConnection.
//
// chooseMove() bloqueia (polling de ~10ms) até receber "MOVE <uci>".
// Chame cancel() para liberar o bloqueio (obrigatório antes de destruir a conexão).
//
// Mensagens de controle recebidas durante o polling (RESIGN, BYE, etc.) são
// armazenadas internamente. Application deve chamar pollControlEvent() a cada
// frame para tratá-las, antes de maybeTriggerAi().
class RemoteAgent : public Agent {
public:
    explicit RemoteAgent(net::LanConnection* conn, std::string nick = "adversário")
        : conn_(conn), nick_(std::move(nick)) {}

    chess::Move chooseMove(chess::Board& board) override;
    std::string name() const override { return nick_; }

    // Sinaliza que chooseMove deve retornar Move{} imediatamente.
    void cancel() { cancelled_.store(true); }

    // Drena UMA mensagem de controle (RESIGN, BYE, DRAW_OFFER…).
    // Retorna nullopt se não houver nenhuma.
    std::optional<std::string> pollControlEvent();

private:
    void enqueueControl(std::string msg);

    net::LanConnection* conn_;
    std::string nick_;
    std::atomic<bool> cancelled_{false};

    std::mutex ctrlMutex_;
    std::queue<std::string> controlEvents_;
};

}  // namespace chess3d::ai
