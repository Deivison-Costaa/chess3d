# Chess3D

Jogo de xadrez 3D em C++ com **OpenGL 4.6 Core** (DSA + KHR_debug) e agente
Minimax — projeto da cadeira de Programação com Agentes (UFPB/CIn).

Veja [`PROJECT_PROMPT.md`](PROJECT_PROMPT.md) para o documento de design completo.

## Build

**Requisito de GPU:** OpenGL 4.6 (NVIDIA GTX 600+/RTX, AMD GCN 1.2+, Intel UHD 620+, Mesa 23+ no Linux).

### Windows (MinGW + MSYS2 + vcpkg)

```powershell
# Na MSYS2 MinGW64 shell (ou ajuste os caminhos conforme seu ambiente):
cmake -B build -S . -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe `
  -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe `
  -DCMAKE_MAKE_PROGRAM=C:/vcpkg/downloads/tools/ninja-1.13.2-windows/ninja.exe

cmake --build build
.\build\bin\chess3d.exe
```

### Linux (Ubuntu/Debian)

```bash
# Dependências do sistema:
sudo apt install build-essential ninja-build cmake pkg-config \
                 libgl-dev libx11-dev libxrandr-dev libxinerama-dev \
                 libxcursor-dev libxi-dev

# vcpkg (se ainda não tiver):
git clone https://github.com/microsoft/vcpkg ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh

# Configurar e compilar:
cmake -B build -S . -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build
./build/bin/chess3d
```

> Engines UCI externos (Stockfish/Lc0/Berserk) precisam de binários Linux em
> `assets/engines/`. Se ausentes, o jogo roda normalmente só com o Minimax interno.
> Alternativamente: `sudo apt install stockfish` e adicione o path ao catálogo.

## Como rodar

Após o build, use os scripts na raiz do projeto:

**Windows:**
```bat
run.bat
```

**Linux:**
```bash
chmod +x run.sh
./run.sh
```

No Windows o script adiciona `C:\msys64\mingw64\bin` ao PATH automaticamente,
necessário para resolver as DLLs do MinGW (`libstdc++`, `libgcc`, `libwinpthread`).
Qualquer argumento extra é repassado ao executável (ex: `run.bat --auto --max-plies 100`).

## Testes

```bash
cmake --build build --target chess3d_tests
ctest --test-dir build --output-on-failure
```

## Créditos

- Modelo 3D: **Jaximus** ([CGTrader](https://www.cgtrader.com/designers/jaximus))
  — Royalty Free License.
- Tabelas posicionais: Tomasz Michniewski, *Simplified Evaluation Function*,
  chessprogramming.org.
