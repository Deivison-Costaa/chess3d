# Chess3D

Jogo de xadrez 3D em C++ com **OpenGL 4.6 Core** (DSA + KHR_debug) e agente
Minimax — projeto da cadeira de Programação com Agentes (UFPB/CIn).

Veja [`PROJECT_PROMPT.md`](PROJECT_PROMPT.md) para o documento de design completo.

## Build (Windows + MinGW + vcpkg)

Pré-requisitos:

- MinGW-w64 (g++ 13+ recomendado, MSYS2 funciona)
- CMake 3.20+
- vcpkg em `C:\vcpkg` (com `VCPKG_DEFAULT_TRIPLET=x64-mingw-dynamic`)
- GPU com OpenGL 4.6 (qualquer NVIDIA GTX 600+/RTX, AMD GCN 1.2+, Intel UHD 620+)

```powershell
cmake -B build -S . -G "MinGW Makefiles" `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic

cmake --build build --config Release
.\build\bin\chess3d.exe
```

## Testes

```powershell
ctest --test-dir build --output-on-failure
```

## Créditos

- Modelo 3D: **Jaximus** ([CGTrader](https://www.cgtrader.com/designers/jaximus))
  — Royalty Free License.
- Tabelas posicionais: Tomasz Michniewski, *Simplified Evaluation Function*,
  chessprogramming.org.
