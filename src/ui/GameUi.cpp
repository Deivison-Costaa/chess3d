#include "GameUi.h"

#include "chess/Notation.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace chess3d::ui {

namespace {

const char* colorLabel(chess::Color c) {
    return c == chess::Color::White ? "Brancas" : "Pretas";
}

void centerNextWindow(float width, float height) {
    const ImVec2 vp = ImGui::GetMainViewport()->Size;
    ImGui::SetNextWindowPos(ImVec2(vp.x * 0.5f - width * 0.5f, vp.y * 0.5f - height * 0.5f),
                            ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Appearing);
}

const char* resultText(chess::GameResult r) {
    switch (r) {
        case chess::GameResult::WhiteWins: return "Vitoria das Brancas!";
        case chess::GameResult::BlackWins: return "Vitoria das Pretas!";
        case chess::GameResult::DrawStalemate:           return "Empate por afogamento";
        case chess::GameResult::DrawFiftyMoveRule:       return "Empate pela regra dos 50 lances";
        case chess::GameResult::DrawInsufficientMaterial:return "Empate por material insuficiente";
        case chess::GameResult::DrawThreefoldRepetition: return "Empate por repeticao tripla";
        case chess::GameResult::WhiteWinsOnTime: return "Brancas vencem por tempo!";
        case chess::GameResult::BlackWinsOnTime: return "Pretas vencem por tempo!";
        case chess::GameResult::Ongoing: return "Em andamento";
    }
    return "?";
}

void formatClock(int ms, char* buf, std::size_t cap) {
    if (ms < 0) ms = 0;
    const int totalSec = ms / 1000;
    const int mm = totalSec / 60;
    const int ss = totalSec % 60;
    // Quando resta menos de 10s, mostra décimos para tensão visual.
    if (totalSec < 10) {
        const int tenths = (ms % 1000) / 100;
        std::snprintf(buf, cap, "%d.%d", ss, tenths);
    } else {
        std::snprintf(buf, cap, "%d:%02d", mm, ss);
    }
}

}  // namespace

void GameUi::renderEngineCombo(const char* idLabel, ai::AgentSpec& spec) {
    const std::string comboId = std::string("Engine##") + idLabel;
    if (ImGui::BeginCombo(comboId.c_str(), ai::engineLabel(spec.engine))) {
        using E = ai::AgentSpec::Engine;
        // Os Minimax internos estão sempre disponíveis.
        const E builtins[] = { E::MinimaxEasy, E::MinimaxMedium, E::MinimaxHard };
        for (E e : builtins) {
            if (ImGui::Selectable(ai::engineLabel(e), spec.engine == e)) spec.engine = e;
        }
        // UCI externos: só listamos se o binário foi detectado.
        if (catalog_.stockfish
            && ImGui::Selectable(ai::engineLabel(E::Stockfish), spec.engine == E::Stockfish)) {
            spec.engine = E::Stockfish;
        }
        if (catalog_.berserk
            && ImGui::Selectable(ai::engineLabel(E::Berserk), spec.engine == E::Berserk)) {
            spec.engine = E::Berserk;
        }
        ImGui::EndCombo();
    }
    if (ai::isUciEngine(spec.engine)) {
        const std::string sliderId = std::string("Tempo (ms)##") + idLabel;
        ImGui::SliderInt(sliderId.c_str(), &spec.moveTimeMs, 250, 5000);
    }
}

void GameUi::renderMainMenu() {
    centerNextWindow(560.0f, 560.0f);
    ImGui::Begin("Chess3D", nullptr,
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove);

    ImGui::PushFont(nullptr);
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.45f, 1.0f), "Chess3D");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopFont();
    ImGui::TextDisabled("Xadrez 3D com agentes inteligentes");
    ImGui::Separator();
    ImGui::Spacing();

    // ── Toggle de modo ───────────────────────────────────────────────────
    const ImVec4 active(0.95f, 0.78f, 0.30f, 1.0f);
    const ImVec4 inactive(0.30f, 0.32f, 0.38f, 1.0f);
    auto modeButton = [&](const char* label, GameMode m, ImVec2 sz) {
        const bool isOn = (setup_.mode == m);
        ImGui::PushStyleColor(ImGuiCol_Button,       isOn ? active   : inactive);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(active.x * 0.85f, active.y * 0.85f, active.z * 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
        ImGui::PushStyleColor(ImGuiCol_Text,
            isOn ? ImVec4(0.08f, 0.08f, 0.10f, 1.0f) : ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
        if (ImGui::Button(label, sz)) setup_.mode = m;
        ImGui::PopStyleColor(4);
    };
    // Linha 1: modos contra IA
    modeButton("Humano vs IA", GameMode::HumanVsAi, ImVec2(246, 48));
    ImGui::SameLine();
    modeButton("IA vs IA",     GameMode::AiVsAi,    ImVec2(246, 48));
    // Linha 2: modos multijogador
    modeButton("Hotseat",      GameMode::Hotseat,   ImVec2(160, 44));
    ImGui::SameLine();
    modeButton("LAN - Hospedar", GameMode::LanHost,  ImVec2(178, 44));
    ImGui::SameLine();
    modeButton("LAN - Conectar", GameMode::LanClient, ImVec2(152, 44));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (setup_.mode == GameMode::HumanVsAi) {
        if (ImGui::BeginCombo("Sua cor", colorLabel(setup_.humanColor))) {
            if (ImGui::Selectable("Brancas", setup_.humanColor == chess::Color::White))
                setup_.humanColor = chess::Color::White;
            if (ImGui::Selectable("Pretas",  setup_.humanColor == chess::Color::Black))
                setup_.humanColor = chess::Color::Black;
            ImGui::EndCombo();
        }
        ai::AgentSpec& aiSpec = (setup_.humanColor == chess::Color::White)
                              ? setup_.blackAgent : setup_.whiteAgent;
        ImGui::Text("Adversário:");
        renderEngineCombo("opponent", aiSpec);

    } else if (setup_.mode == GameMode::AiVsAi) {
        const float cardW = 230.0f;
        ImGui::BeginChild("##white", ImVec2(cardW, 110), true);
            ImGui::TextColored(ImVec4(0.95f, 0.92f, 0.85f, 1.0f), "Lado Branco");
            ImGui::Separator();
            renderEngineCombo("w", setup_.whiteAgent);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##black", ImVec2(cardW, 110), true);
            ImGui::TextColored(ImVec4(0.45f, 0.45f, 0.50f, 1.0f), "Lado Preto");
            ImGui::Separator();
            renderEngineCombo("b", setup_.blackAgent);
        ImGui::EndChild();
        ImGui::TextDisabled("Pause/velocidade aparecem em jogo (HUD inferior).");

    } else if (setup_.mode == GameMode::Hotseat) {
        ImGui::TextColored(ImVec4(0.70f, 0.95f, 0.70f, 1.0f),
                           "Dois jogadores no mesmo computador.");
        ImGui::TextDisabled("Cada um faz sua jogada na mesma tela. Sem rede.");

    } else if (setup_.mode == GameMode::LanHost) {
        ImGui::TextColored(ImVec4(0.70f, 0.85f, 1.0f, 1.0f), "Hospedar partida na rede local");
        ImGui::InputText("Seu apelido##h", setup_.lanNick, sizeof(setup_.lanNick));
        ImGui::InputInt("Porta",          &setup_.lanPort);
        setup_.lanPort = (setup_.lanPort < 1024) ? 1024
                       : (setup_.lanPort > 65535) ? 65535 : setup_.lanPort;
        if (ImGui::BeginCombo("Sua cor##h", colorLabel(setup_.humanColor))) {
            if (ImGui::Selectable("Brancas", setup_.humanColor == chess::Color::White))
                setup_.humanColor = chess::Color::White;
            if (ImGui::Selectable("Pretas",  setup_.humanColor == chess::Color::Black))
                setup_.humanColor = chess::Color::Black;
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("Passe o IP da sua maquina para o cliente.");

    } else if (setup_.mode == GameMode::LanClient) {
        ImGui::TextColored(ImVec4(0.70f, 0.85f, 1.0f, 1.0f), "Conectar a uma partida na LAN");
        ImGui::InputText("Seu apelido##c", setup_.lanNick, sizeof(setup_.lanNick));
        static char hostBuf[64];
        // Inicializa o buffer com o valor atual apenas uma vez.
        if (hostBuf[0] == '\0' && !setup_.lanHost.empty())
            std::snprintf(hostBuf, sizeof(hostBuf), "%s", setup_.lanHost.c_str());
        if (ImGui::InputText("IP do host", hostBuf, sizeof(hostBuf)))
            setup_.lanHost = hostBuf;
        ImGui::InputInt("Porta##c", &setup_.lanPort);
        setup_.lanPort = (setup_.lanPort < 1024) ? 1024
                       : (setup_.lanPort > 65535) ? 65535 : setup_.lanPort;
        ImGui::TextDisabled("Peça o IP da máquina host (aparece no Lobby).");
    }

    // Relógio (só em modos que têm IA ou Hotseat, não em LAN — host decide)
    if (setup_.mode == GameMode::HumanVsAi || setup_.mode == GameMode::AiVsAi ||
        setup_.mode == GameMode::Hotseat) {
        ImGui::Spacing();
        ImGui::Checkbox("Animar lances", &setup_.animateAi);
        if (ImGui::BeginCombo("Modo de tempo", setup_.timeControl.label)) {
            const TimeControl options[] = { kTimeUnlimited, kTimeBlitz5, kTimeRapid10 };
            for (const auto& opt : options) {
                const bool selected = (setup_.timeControl.initialMs == opt.initialMs);
                if (ImGui::Selectable(opt.label, selected)) setup_.timeControl = opt;
            }
            ImGui::EndCombo();
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const char* startLabel = (setup_.mode == GameMode::LanHost) ? "Hospedar"
                           : (setup_.mode == GameMode::LanClient) ? "Conectar"
                           : "Nova Partida";
    if (ImGui::Button(startLabel, ImVec2(200, 36))) {
        if (onStartGame_) onStartGame_(setup_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Sair", ImVec2(120, 36))) {
        if (onExit_) onExit_();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Controles em jogo:");
    ImGui::TextDisabled("Click esquerdo: selecionar/mover peca");
    ImGui::TextDisabled("Arrastar: rotacionar camera | Direito: pan | Scroll: zoom");
    ImGui::TextDisabled("R/F: reset/top-down | 1/2: vista branco/preto");

    ImGui::End();
}

void GameUi::renderLobby(const LobbyData& data) {
    centerNextWindow(420.0f, 220.0f);
    ImGui::Begin("Aguardando jogador...", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::TextColored(ImVec4(0.70f, 0.85f, 1.0f, 1.0f), "Partida aberta na rede local.");
    ImGui::Spacing();
    ImGui::Text("Seu IP:  %s", data.localIp.c_str());
    ImGui::Text("Porta:   %d", data.port);
    ImGui::Spacing();
    ImGui::TextDisabled("Passe o IP e a porta acima para o outro jogador.");
    ImGui::Separator();
    ImGui::Spacing();
    // Spinner simples usando pontos rotacionando.
    const float t = static_cast<float>(ImGui::GetTime());
    const char* dots[] = { ".  ", ".. ", "..." };
    ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.45f, 1.0f),
                       "Aguardando conexão%s", dots[static_cast<int>(t * 2) % 3]);
    ImGui::Spacing();
    if (ImGui::Button("Cancelar", ImVec2(140, 32))) {
        if (onCancelLobby_) onCancelLobby_();
    }
    ImGui::End();
}

void GameUi::renderHud(const HudData& data) {
    const ImVec2 vp = ImGui::GetMainViewport()->Size;

    // ── Painel superior esquerdo: turno + estado da IA ─────────────────
    ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGui::Begin("##turn", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::Text("Lance %d", data.fullmove);
    ImGui::Text("Vez: %s", colorLabel(data.sideToMove));
    if (data.inCheck) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.2f, 1.0f), "XEQUE!");
    }
    if (data.aiThinking) {
        ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "Pensando...");
    }
    // Relógios (somente se modo temporizado)
    if (data.whiteTimeMs >= 0 || data.blackTimeMs >= 0) {
        ImGui::Separator();
        char wBuf[16], bBuf[16];
        formatClock(data.whiteTimeMs, wBuf, sizeof(wBuf));
        formatClock(data.blackTimeMs, bBuf, sizeof(bBuf));
        const bool wActive = (data.sideToMove == chess::Color::White);
        const ImVec4 hot(1.00f, 0.90f, 0.40f, 1.0f);   // ativo
        const ImVec4 low(0.95f, 0.30f, 0.30f, 1.0f);   // < 10s
        const ImVec4 idle(0.75f, 0.75f, 0.75f, 1.0f);
        auto pick = [&](int ms, bool active) -> ImVec4 {
            if (ms < 10000) return low;
            return active ? hot : idle;
        };
        ImGui::TextColored(pick(data.whiteTimeMs, wActive),
                           "Brancas  %s%s", wBuf, wActive ? " <" : "");
        ImGui::TextColored(pick(data.blackTimeMs, !wActive),
                           "Pretas   %s%s", bBuf, !wActive ? " <" : "");
    }
    ImGui::End();

    // ── Painel superior direito: capturas ──────────────────────────────
    constexpr float capW = 260.0f;
    ImGui::SetNextWindowPos(ImVec2(vp.x - capW - 12.0f, 12.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(capW, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGui::Begin("##captures", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::Text("Capturas");
    ImGui::Separator();
    auto drawCaps = [](const char* label, const std::vector<chess::Piece>& caps) {
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        for (const auto& p : caps) {
            ImGui::SameLine(0.0f, 3.0f);
            const ImVec4 color = (p.color == chess::Color::White)
                ? ImVec4(0.95f, 0.92f, 0.78f, 1.0f)
                : ImVec4(0.40f, 0.40f, 0.45f, 1.0f);
            ImGui::TextColored(color, "%s", chess::pieceGlyph(p));
        }
    };
    drawCaps("Brancas tem:", data.capturedByWhite);
    drawCaps("Pretas tem:",  data.capturedByBlack);
    ImGui::End();

    // ── Painel inferior direito: historico SAN ─────────────────────────
    constexpr float histW = 240.0f;
    constexpr float histH = 280.0f;
    ImGui::SetNextWindowPos(ImVec2(vp.x - histW - 12.0f, vp.y - histH - 12.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(histW, histH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGui::Begin("Historico", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
    for (std::size_t i = 0; i < data.sanHistory.size(); i += 2) {
        ImGui::Text("%2zu. %-7s %s", i/2 + 1, data.sanHistory[i].c_str(),
                    (i + 1 < data.sanHistory.size()) ? data.sanHistory[i+1].c_str() : "");
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f) {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::End();

    // ── Painel inferior esquerdo: botoes ───────────────────────────────
    constexpr float btnW = 220.0f;
    constexpr float btnH = 80.0f;
    ImGui::SetNextWindowPos(ImVec2(12.0f, vp.y - btnH - 12.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(btnW, btnH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGui::Begin("##buttons", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoFocusOnAppearing);
    if (ImGui::Button("Menu", ImVec2(90, 32))) {
        if (onBackToMenu_) onBackToMenu_();
    }
    ImGui::SameLine();
    // Desfazer: oculto em IA vs IA e em LAN (exigiria handshake com o oponente).
    if (!data.aiVsAi && !data.lanMode && ImGui::Button("Desfazer", ImVec2(100, 32))) {
        if (onUndo_) onUndo_();
    }
    ImGui::End();

    // ── Painel inferior central (só em IA vs IA): pause + velocidade ────
    if (data.aiVsAi) {
        constexpr float pbW = 360.0f, pbH = 70.0f;
        ImGui::SetNextWindowPos(ImVec2((vp.x - pbW) * 0.5f, vp.y - pbH - 12.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(pbW, pbH), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.75f);
        ImGui::Begin("##playback", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoFocusOnAppearing);
        const char* playLabel = data.paused ? "Retomar" : "Pausar";
        if (ImGui::Button(playLabel, ImVec2(85, 32))) {
            if (onPause_) onPause_(!data.paused);
        }
        ImGui::SameLine();
        const float speeds[] = {0.5f, 1.0f, 2.0f, 4.0f};
        for (float s : speeds) {
            char label[8];
            std::snprintf(label, sizeof(label), "%gx", s);
            const bool on = (std::abs(data.speedMultiplier - s) < 0.01f);
            if (on) ImGui::PushStyleColor(ImGuiCol_Button,
                                          ImVec4(0.95f, 0.78f, 0.30f, 1.0f));
            if (ImGui::Button(label, ImVec2(45, 32))) {
                if (onSpeed_) onSpeed_(s);
            }
            if (on) ImGui::PopStyleColor();
            ImGui::SameLine();
        }
        // Mostra quem joga de cada lado.
        ImGui::NewLine();
        ImGui::TextDisabled("%s (B) vs %s (P)",
                            data.whiteAgentName.c_str(),
                            data.blackAgentName.c_str());
        ImGui::End();
    }
}

void GameUi::renderEndGame(chess::GameResult result, int totalPlies) {
    centerNextWindow(420.0f, 220.0f);
    ImGui::Begin("Fim de jogo", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove);

    ImGui::SetWindowFontScale(1.4f);
    ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.45f, 1.0f), "%s", resultText(result));
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();
    ImGui::Text("Total de meio-lances: %d", totalPlies);
    ImGui::Spacing();
    if (ImGui::Button("Nova partida", ImVec2(160, 36))) {
        if (onNewGame_) onNewGame_();
    }
    ImGui::SameLine();
    if (ImGui::Button("Menu principal", ImVec2(160, 36))) {
        if (onBackToMenu_) onBackToMenu_();
    }
    ImGui::End();
}

void GameUi::renderDebugPanel(const ai::SearchInfo& info, const std::string& agentName) {
    if (!showDebug_) return;
    ImGui::SetNextWindowPos(ImVec2(280.0f, 12.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 380.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowBgAlpha(0.85f);
    ImGui::Begin("Debug (F3)", &showDebug_,
                 ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::TextDisabled("Teclas: F4=view G=degamma L=normal T=PBR P=diag");
    ImGui::Separator();
    ImGui::Text("Agente: %s", agentName.c_str());
    ImGui::Separator();
    ImGui::Text("Depth alcancado: %d", info.depthReached);
    ImGui::Text("Nos explorados: %llu", static_cast<unsigned long long>(info.nodesVisited));
    ImGui::Text("Tempo: %.2f ms", info.elapsed.count() / 1000.0);
    ImGui::Text("Avaliacao: %d cp", info.evaluation);
    if (info.nodesVisited > 0 && info.elapsed.count() > 0) {
        const double knps = static_cast<double>(info.nodesVisited)
                          / (info.elapsed.count() / 1e6) / 1000.0;
        ImGui::Text("Throughput: %.0f kN/s", knps);
    }
    ImGui::Separator();
    ImGui::Text("Linha principal:");
    for (const auto& m : info.principalVariation) {
        ImGui::BulletText("%s", chess::moveToUci(m).c_str());
    }
    ImGui::Separator();
    ImGui::Text("Top candidatos:");
    int rank = 1;
    for (const auto& c : info.topCandidates) {
        ImGui::Text("%d. %s   eval=%+d cp", rank++,
                    chess::moveToUci(c.move).c_str(), c.score);
    }
    ImGui::End();
}

void GameUi::renderPromotionDialog(const PromotionRequest& req) {
    centerNextWindow(360.0f, 200.0f);
    ImGui::Begin("Promocao", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove);
    ImGui::Text("Para qual peca promover?");
    ImGui::Spacing();
    constexpr ImVec2 sz(70, 50);
    if (ImGui::Button("Rainha", sz)) { if (onPromotion_) onPromotion_(chess::PieceType::Queen);  }
    ImGui::SameLine();
    if (ImGui::Button("Torre",  sz)) { if (onPromotion_) onPromotion_(chess::PieceType::Rook);   }
    ImGui::SameLine();
    if (ImGui::Button("Bispo",  sz)) { if (onPromotion_) onPromotion_(chess::PieceType::Bishop); }
    ImGui::SameLine();
    if (ImGui::Button("Cavalo", sz)) { if (onPromotion_) onPromotion_(chess::PieceType::Knight); }
    ImGui::Spacing();
    ImGui::TextDisabled(req.isCapture ? "(promocao por captura)" : "(promocao por avanco)");
    ImGui::End();
}

}  // namespace chess3d::ui
