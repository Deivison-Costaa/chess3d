#include "GameUi.h"

#include "chess/Notation.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace chess3d::ui {

namespace {

const char* difficultyLabel(ai::Difficulty d) {
    switch (d) {
        case ai::Difficulty::Easy:   return "Facil (depth 2)";
        case ai::Difficulty::Medium: return "Medio (depth 4)";
        case ai::Difficulty::Hard:   return "Dificil (depth 6)";
        case ai::Difficulty::Master: return "Mestre (Stockfish)";
    }
    return "?";
}

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
        case chess::GameResult::Ongoing: return "Em andamento";
    }
    return "?";
}

}  // namespace

void GameUi::renderMainMenu() {
    centerNextWindow(420.0f, 360.0f);
    ImGui::Begin("Chess3D", nullptr,
                 ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove);

    ImGui::PushFont(nullptr);
    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(ImVec4(0.95f, 0.85f, 0.45f, 1.0f), "Chess3D");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopFont();
    ImGui::TextDisabled("Xadrez 3D com agente Minimax");
    ImGui::Separator();
    ImGui::Spacing();

    // Cor do jogador
    if (ImGui::BeginCombo("Cor do jogador", colorLabel(setup_.humanColor))) {
        if (ImGui::Selectable("Brancas", setup_.humanColor == chess::Color::White)) {
            setup_.humanColor = chess::Color::White;
        }
        if (ImGui::Selectable("Pretas",  setup_.humanColor == chess::Color::Black)) {
            setup_.humanColor = chess::Color::Black;
        }
        ImGui::EndCombo();
    }

    // Dificuldade
    if (ImGui::BeginCombo("Dificuldade", difficultyLabel(setup_.difficulty))) {
        for (auto d : {ai::Difficulty::Easy, ai::Difficulty::Medium, ai::Difficulty::Hard}) {
            if (ImGui::Selectable(difficultyLabel(d), setup_.difficulty == d)) {
                setup_.difficulty = d;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Checkbox("Animar lances da IA", &setup_.animateAi);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Nova Partida", ImVec2(180, 36))) {
        if (onStartGame_) onStartGame_(setup_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Sair", ImVec2(120, 36))) {
        if (onExit_) onExit_();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Controles em jogo:");
    ImGui::TextDisabled("Click esquerdo: selecionar/mover peca");
    ImGui::TextDisabled("Arrastar: rotacionar camera");
    ImGui::TextDisabled("Direito: pan | Scroll: zoom");
    ImGui::TextDisabled("R/F: reset/top-down | 1/2: vista branco/preto");
    ImGui::TextDisabled("7/8/9: trocar dificuldade em jogo");

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
    if (ImGui::Button("Desfazer", ImVec2(100, 32))) {
        if (onUndo_) onUndo_();
    }
    ImGui::End();
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
