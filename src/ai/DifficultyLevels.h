#pragma once

#include <cstdint>

namespace chess3d::ai {

enum class Difficulty : std::uint8_t {
    Easy,    // depth 2, só material
    Medium,  // depth 4, material + PSTs
    Hard,    // depth 6, material + PSTs + mobilidade + ordering
    Master,  // Stockfish UCI (mantido p/ compat com factory antigo)
};

struct DifficultyConfig {
    int depth = 2;
    bool usePsts = false;
    bool useMobility = false;
    bool useMoveOrdering = false;
    bool useQuiescence = false;
    int timeLimitMs = 0;  // 0 = sem limite (apenas depth fixo)
};

constexpr DifficultyConfig configFor(Difficulty d) {
    switch (d) {
        case Difficulty::Easy:
            return {2, false, false, false, false, 0};
        case Difficulty::Medium:
            return {4, true,  false, true,  false, 0};
        case Difficulty::Hard:
            return {6, true,  true,  true,  true,  3000};
        case Difficulty::Master:
            return {0, false, false, false, false, 1000};
    }
    return {2, false, false, false, false, 0};
}

// Especificação genérica de qualquer agente (Minimax interno OU UCI externo).
// Usado pela UI pra deixar o usuário escolher livremente cada lado.
struct AgentSpec {
    enum class Engine : std::uint8_t {
        MinimaxEasy,
        MinimaxMedium,
        MinimaxHard,
        Stockfish,
        Berserk,
    };
    Engine engine = Engine::MinimaxMedium;
    int moveTimeMs = 1000;  // só usado em engines UCI (Stockfish/Berserk)
};

inline const char* engineLabel(AgentSpec::Engine e) {
    switch (e) {
        case AgentSpec::Engine::MinimaxEasy:   return "Minimax Facil";
        case AgentSpec::Engine::MinimaxMedium: return "Minimax Medio";
        case AgentSpec::Engine::MinimaxHard:   return "Minimax Dificil";
        case AgentSpec::Engine::Stockfish:     return "Stockfish 18";
        case AgentSpec::Engine::Berserk:       return "Berserk 14";
    }
    return "?";
}

inline bool isUciEngine(AgentSpec::Engine e) {
    return e == AgentSpec::Engine::Stockfish
        || e == AgentSpec::Engine::Berserk;
}

}  // namespace chess3d::ai
