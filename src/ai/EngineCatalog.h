#pragma once

#include <filesystem>

namespace chess3d::ai {

// Quais engines UCI externos estão presentes em assets/engines/.
// Detectados no startup pra UI mostrar só opções viáveis.
struct EngineCatalog {
    bool stockfish = false;
    bool berserk   = false;

    std::filesystem::path stockfishPath;
    std::filesystem::path berserkExePath;
    std::filesystem::path berserkNetPath;

    static EngineCatalog detect();
};

}  // namespace chess3d::ai
