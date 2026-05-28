#pragma once

namespace chess3d::platform {

// Build empacotado do Windows (.exe auto-extraível): na 1ª execução extrai o
// payload embutido (assets + engines) para %LOCALAPPDATA%\chess3d\<versão> e
// aponta o asset root pra lá. Em qualquer outra configuração é no-op.
//
// Deve ser chamado bem no início de main(), antes de qualquer uso de assets.
void bootstrapExtractPayload();

}  // namespace chess3d::platform
