#include "EngineCatalog.h"

#include <spdlog/spdlog.h>

namespace chess3d::ai {

namespace {

std::filesystem::path assetsRoot() {
#ifdef CHESS3D_ASSETS_DIR
    return std::filesystem::path(CHESS3D_ASSETS_DIR);
#else
    return std::filesystem::path("assets");
#endif
}

}  // namespace

#ifdef _WIN32
constexpr const char* kExeExt = ".exe";
#else
constexpr const char* kExeExt = "";
#endif

EngineCatalog EngineCatalog::detect() {
    EngineCatalog c;
    const auto root = assetsRoot() / "engines";

    c.stockfishPath = root / (std::string("stockfish") + kExeExt);
    c.stockfish = std::filesystem::exists(c.stockfishPath);

    c.lc0ExePath     = root / "lc0" / (std::string("lc0") + kExeExt);
    c.lc0WeightsPath = root / "lc0" / "791556.pb.gz";
    c.lc0 = std::filesystem::exists(c.lc0ExePath)
         && std::filesystem::exists(c.lc0WeightsPath);

    c.berserkExePath = root / (std::string("berserk") + kExeExt);
    c.berserkNetPath = root / "berserk.nn";
    c.berserk = std::filesystem::exists(c.berserkExePath)
             && std::filesystem::exists(c.berserkNetPath);

    spdlog::info("EngineCatalog: stockfish={} lc0={} berserk={}",
                 c.stockfish, c.lc0, c.berserk);
    return c;
}

}  // namespace chess3d::ai
