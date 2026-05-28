#include "AssetPaths.h"

#include <cstdlib>
#include <optional>
#include <system_error>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace chess3d::platform {

namespace {

std::optional<fs::path> g_override;

bool dirExists(const fs::path& p) {
    if (p.empty()) return false;
    std::error_code ec;
    return fs::is_directory(p, ec);
}

// Diretório de assets, em ordem de prioridade — o primeiro que EXISTE vence.
fs::path resolve() {
    // 1. Override explícito (bootstrap auto-extraível do Windows).
    if (g_override && dirExists(*g_override)) return *g_override;

    // 2. Variável de ambiente (escape hatch p/ devs / scripts).
    if (const char* env = std::getenv("CHESS3D_ASSETS_DIR")) {
        fs::path p(env);
        if (dirExists(p)) return p;
    }

    // 3. AppImage: $APPDIR/usr/share/chess3d/assets.
    if (const char* appdir = std::getenv("APPDIR")) {
        fs::path p = fs::path(appdir) / "usr" / "share" / "chess3d" / "assets";
        if (dirExists(p)) return p;
    }

    // 4. Relativo ao executável (ZIP portátil / layout usr/bin do AppImage).
    const fs::path exeDir = executableDir();
    if (!exeDir.empty()) {
        const fs::path candidates[] = {
            exeDir / ".." / "share" / "chess3d" / "assets",
            exeDir / "assets",
        };
        for (const fs::path& cand : candidates) {
            std::error_code ec;
            const fs::path canon = fs::weakly_canonical(cand, ec);
            if (!ec && dirExists(canon)) return canon;
            if (dirExists(cand)) return cand;
        }
    }

    // 5. Caminho de build embutido em tempo de compilação (só existe na máquina de dev).
#ifdef CHESS3D_ASSETS_DIR
    {
        fs::path p(CHESS3D_ASSETS_DIR);
        if (dirExists(p)) return p;
    }
#endif

    // 6. Último recurso: ./assets relativo ao CWD.
    return fs::path("assets");
}

}  // namespace

fs::path executableDir() {
#if defined(_WIN32)
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(),
                                           static_cast<DWORD>(buf.size()));
        if (n == 0) return {};
        if (n < buf.size()) {
            buf.resize(n);
            break;
        }
        buf.resize(buf.size() * 2);  // buffer pequeno; dobra e tenta de novo
    }
    return fs::path(buf).parent_path();
#elif defined(__linux__)
    std::error_code ec;
    const fs::path self = fs::read_symlink("/proc/self/exe", ec);
    if (ec) return {};
    return self.parent_path();
#else
    return {};
#endif
}

void setAssetRootOverride(fs::path root) {
    g_override = std::move(root);
}

const fs::path& assetRoot() {
    static const fs::path resolved = resolve();
    return resolved;
}

fs::path assetPath(const fs::path& rel) {
    return assetRoot() / rel;
}

}  // namespace chess3d::platform
