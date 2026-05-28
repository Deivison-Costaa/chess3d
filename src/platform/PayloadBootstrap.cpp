#include "PayloadBootstrap.h"

#if defined(CHESS3D_PACKAGED) && defined(_WIN32)

#include "AssetPaths.h"

#include <spdlog/spdlog.h>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
// Garante que RT_RCDATA/MAKEINTRESOURCE resolvam p/ as variantes wide (LPCWSTR),
// compatíveis com FindResourceW (no MinGW o default é ANSI).
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef CHESS3D_VERSION
#define CHESS3D_VERSION "dev"
#endif

namespace fs = std::filesystem;

namespace chess3d::platform {
namespace {

// %LOCALAPPDATA%\chess3d\<versão>  (cai pra %TEMP% se a env não existir)
fs::path cacheRoot() {
    wchar_t buf[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    fs::path base = (n > 0 && n < MAX_PATH) ? fs::path(buf) : fs::temp_directory_path();
    return base / L"chess3d" / CHESS3D_VERSION;
}

// Extrai um .zip usando o tar.exe (bsdtar) nativo do Windows 10 1803+/11.
bool extractWithTar(const fs::path& zip, const fs::path& dest) {
    std::wstring cmd = L"tar.exe -xf \"" + zip.wstring() + L"\" -C \"" + dest.wstring() + L"\"";
    std::vector<wchar_t> mut(cmd.begin(), cmd.end());
    mut.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    if (!CreateProcessW(nullptr, mut.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code == 0;
}

}  // namespace

void bootstrapExtractPayload() {
    const fs::path cache  = cacheRoot();
    const fs::path assets = cache / "assets";
    const fs::path ready  = cache / ".ready";

    std::error_code ec;
    if (fs::exists(ready, ec)) {  // já extraído nesta versão
        setAssetRootOverride(assets);
        return;
    }

    // Localiza o recurso RCDATA embutido no .exe.
    HRSRC res = FindResourceW(nullptr, L"CHESS3D_PAYLOAD", RT_RCDATA);
    if (!res) {
        spdlog::error("bootstrap: recurso CHESS3D_PAYLOAD não encontrado");
        return;
    }
    HGLOBAL handle = LoadResource(nullptr, res);
    const void* data = handle ? LockResource(handle) : nullptr;
    const DWORD size = SizeofResource(nullptr, res);
    if (!data || size == 0) {
        spdlog::error("bootstrap: payload embutido vazio");
        return;
    }

    fs::create_directories(cache, ec);

    // Grava o zip embutido em disco (tar.exe precisa de arquivo seekable).
    const fs::path tmpZip = cache / "_payload.zip";
    {
        std::ofstream out(tmpZip, std::ios::binary | std::ios::trunc);
        if (!out) {
            spdlog::error("bootstrap: não consegui escrever {}", tmpZip.string());
            return;
        }
        out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    }

    fs::remove_all(assets, ec);  // remove extração parcial anterior, se houver
    if (!extractWithTar(tmpZip, cache)) {
        spdlog::error("bootstrap: falha ao extrair payload com tar.exe");
        return;
    }
    fs::remove(tmpZip, ec);

    { std::ofstream(ready) << CHESS3D_VERSION; }  // marca conclusão

    spdlog::info("bootstrap: assets extraídos em {}", assets.string());
    setAssetRootOverride(assets);
}

}  // namespace chess3d::platform

#else  // não empacotado ou não-Windows: no-op

namespace chess3d::platform {
void bootstrapExtractPayload() {}
}  // namespace chess3d::platform

#endif
