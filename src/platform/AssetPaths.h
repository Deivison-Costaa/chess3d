#pragma once

#include <filesystem>

namespace chess3d::platform {

// Diretório onde está o executável em execução (resolvido 1x).
// Win32: GetModuleFileNameW. Linux: /proc/self/exe. Vazio se indeterminável.
std::filesystem::path executableDir();

// Sobrescreve o root de assets. Usado pelo bootstrap auto-extraível do Windows.
// DEVE ser chamado antes da primeira chamada a assetRoot()/assetPath(),
// pois o root é resolvido e cacheado na primeira consulta.
void setAssetRootOverride(std::filesystem::path root);

// Root resolvido do diretório de assets (models/, textures/, shaders/, engines/).
// Ordem de resolução documentada em AssetPaths.cpp. Cacheado após a 1ª chamada.
const std::filesystem::path& assetRoot();

// assetRoot() / rel
std::filesystem::path assetPath(const std::filesystem::path& rel);

}  // namespace chess3d::platform
