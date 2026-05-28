#include "EngineCatalog.h"

#include "platform/AssetPaths.h"

#include <spdlog/spdlog.h>

namespace chess3d::ai {

namespace {

std::filesystem::path assetsRoot() {
    return platform::assetRoot();
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

    c.berserkExePath = root / (std::string("berserk") + kExeExt);
    c.berserkNetPath = root / "berserk.nn";
    c.berserk = std::filesystem::exists(c.berserkExePath)
             && std::filesystem::exists(c.berserkNetPath);

    spdlog::info("EngineCatalog: stockfish={} berserk={}",
                 c.stockfish, c.berserk);
    return c;
}

}  // namespace chess3d::ai
