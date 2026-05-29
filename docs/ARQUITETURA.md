# Chess3D — Documentação técnica

Jogo de xadrez **3D** em C++ com **OpenGL 4.6 Core** e um **agente inteligente**
(Minimax com poda alfa-beta) em três níveis, mais integração com engines UCI externas
(Stockfish, Berserk). Projeto da cadeira de **Programação com Agentes** (UFPB/CIn).

| | |
|---|---|
| **Repositório** | https://github.com/Deivison-Costaa/chess3d |
| **Downloads (binários prontos)** | https://github.com/Deivison-Costaa/chess3d/releases/tag/v0.1.0 |
| **Design original** | [`PROJECT_PROMPT.md`](../PROJECT_PROMPT.md) |
| **Apresentação (slides)** | [`docs/APRESENTACAO.md`](APRESENTACAO.md) |

| Menu | Em jogo |
|---|---|
| ![Menu principal](img/menu.png) | ![Tabuleiro 3D](img/jogo.png) |

---

## 1. Visão geral

O projeto tem dois eixos:

1. **Agente racional clássico** — Minimax/negamax com poda α-β, função de avaliação
   heurística (material + tabelas posicionais + mobilidade) e níveis por profundidade.
   Como bônus, agentes UCI externos (Stockfish 18, Berserk 14) plugados via subprocesso.
2. **Apresentação visual** — renderização 3D moderna (OpenGL 4.6 com Direct State Access),
   câmera orbital, materiais PBR, animações de movimento (cavalo em arco, captura com fade,
   queda do rei no xeque-mate), HUD/menus em ImGui.

Além do single-player, há **multiplayer LAN** (TCP), **hotseat** (2 jogadores no mesmo PC)
e um **modo headless** (sem janela) usado para testes automatizados e partidas IA×IA.

---

## 2. Stack de tecnologias

| Tecnologia | Função | Onde aparece |
|---|---|---|
| **C++20** | Linguagem | todo o `src/` |
| **OpenGL 4.6 Core** (DSA + `KHR_debug`) | API gráfica | `src/render/`, `src/app/Window` |
| **GLFW 3** | Janela, contexto GL e input | `src/app/Window`, `src/input/` |
| **glad** | Loader de funções OpenGL | `src/app/Window` |
| **glm** | Matemática (vec/mat/quat) | `src/render/`, `src/core/`, `src/anim/` |
| **tinygltf** | Carregar modelos glTF 2.0 (`.glb`) | `src/render/GltfLoader` |
| **stb_image** | Decodificar texturas (JPG/PNG) | `src/render/GltfLoader`, `src/app/Application` |
| **Dear ImGui** | UI imediata (menu, HUD, debug) | `src/ui/`, `src/app/Window` |
| **spdlog** | Logging | todo o projeto |
| **Catch2 v3** | Testes unitários | `tests/` |
| **CMake 3.20+ + Ninja** | Build system | `CMakeLists.txt`, `CMakePresets.json` |
| **vcpkg** (modo manifest) | Gerenciador de dependências | `vcpkg.json` |
| **MinGW-w64 / MSYS2** (Win) · **GCC** (Linux) | Toolchain C++ | CI + presets |
| **Stockfish 18 / Berserk 14** (UCI) | Engines de xadrez externas | `src/ai/UciEngineAgent`, subprocesso |
| **Sockets TCP** (Winsock / POSIX) | Multiplayer LAN | `src/net/` |
| **GitHub Actions** | CI/CD (testes + releases) | `.github/workflows/` |

---

## 3. Arquitetura em camadas

```
                         ┌─────────────────────────────┐
                         │           main.cpp          │  ponto de entrada / CLI
                         └──────────────┬──────────────┘
                ┌───────────────────────┴───────────────────────┐
                │            src/app  (orquestração)             │
                │  Application · Window · GameSession · Headless  │
                └───┬───────────┬───────────┬───────────┬────────┘
       render/anim  │     ui    │   input   │    net     │  ai
   ┌────────────────┴┐ ┌────────┴─┐ ┌───────┴──┐ ┌───────┴───┐ ┌┴───────────────┐
   │ Shader · Mesh   │ │  GameUi   │ │ Input    │ │ LanConn.  │ │ Agent           │
   │ Camera · Picker │ │ (ImGui)   │ │ Handler  │ │ Protocol  │ │ MinimaxAgent    │
   │ GltfLoader      │ └───────────┘ └──────────┘ │ Remote-   │ │ Evaluator/PSTs  │
   │ Animator/Easing │                            │ Agent     │ │ UciEngineAgent  │
   └────────┬────────┘                            └───────────┘ │ EngineCatalog   │
            │                                                    └───────┬─────────┘
            └──────────────────────┬─────────────────────────────────────┘
                       ┌───────────┴───────────┐
                       │    src/chess  (engine pura, sem GL/UI)        │
                       │ Board · MoveGenerator · Rules · Move · Notation│
                       └──────────────────────────────────────────────┘
   src/core    → utilidades de coordenadas (BoardCoords)
   src/platform→ resolução de assets + auto-extração no Windows
```

A **engine de xadrez** (`src/chess`) é independente de gráficos/UI — por isso é testável de
forma isolada (perft, regras) e reutilizável pelo modo headless.

---

## 4. Módulo a módulo

### `src/chess/` — Engine de xadrez (lógica pura)
Representação **mailbox 8×8** (escolha didática, não bitboards).
- **`Types.h`** — `PieceType`, `Color`, `Piece`, `Square`; direitos de roque como bitmask.
- **`Board.{h,cpp}`** — `makeMove`/`unmakeMove` simétricos via `UndoInfo`; trata roque,
  en passant, promoção e perda de direitos de roque; `isSquareAttacked`; FEN.
- **`MoveGenerator.{h,cpp}`** — geração pseudo-legal + filtro de legalidade (make/unmake e
  teste de auto-xeque); roque valida casas vazias/atacadas; inclui `perft`/`perftDivide`.
- **`Rules.{h,cpp}`** — xeque-mate, afogamento, material insuficiente, 50 lances, repetição
  tripla; enum `GameResult` com todos os desfechos.
- **`Notation.{h,cpp}`** — SAN com desambiguação e sufixos `+`/`#`.

**Validação:** perft batendo com a tabela de referência (20 / 400 / 8902 / 197281;
Kiwipete 48 / 2039 / 97862; etc.).

### `src/ai/` — Agentes inteligentes
- **`Agent.h`** — interface abstrata (`chooseMove`) + `SearchInfo` (depth/nós/eval/tempo/PV).
- **`Evaluator.{h,cpp}` + `PieceSquareTables.h`** — material clássico + tabelas posicionais
  (Michniewski / *Simplified Evaluation*) espelhadas para o lado preto + mobilidade.
- **`MinimaxAgent.{h,cpp}`** — **negamax com poda α-β**, ordenação de lances **MVV-LVA**,
  **iterative deepening** com limite de tempo e **quiescence search**. Score de mate
  descontado pela profundidade (prefere mates curtos).
- **`DifficultyLevels.h`** — Fácil (depth 2, só material), Médio (depth 4, +PST +ordenação),
  Difícil (depth 6, +mobilidade +quiescence + 3 s de iterative deepening); `AgentSpec`
  para escolher engine por lado.
- **`EngineCatalog.{h,cpp}`** — detecta engines UCI em `assets/engines/` (Stockfish/Berserk)
  e habilita as opções correspondentes na UI.
- **`UciEngineAgent.{h,cpp}`** — cliente UCI genérico que conversa com a engine externa por
  **pipes anônimos** (`CreateProcessW` no Windows, `fork`+`execvp` no POSIX) e uma thread
  leitora; handshake `uci`/`isready`, `position fen` + `go movetime`.
- **`RemoteAgent.{h,cpp}`** — implementa `Agent` recebendo os lances do oponente via LAN.

**Métrica de poda (depth 4, posição inicial):** sem poda ≈ **206 603** nós; com α-β + MVV-LVA
≈ **5 425** nós (**~38×** de redução).

### `src/render/` — Renderização (OpenGL 4.6 Core, DSA)
- **`Shader.{h,cpp}`** — compila/linka programas; usa `glProgramUniform*` (DSA); cache de
  uniforms; `bindUniformBlock` para UBOs.
- **`Mesh.{h,cpp}`** — VAO/VBO/EBO via `glCreate*` + `glNamedBufferStorage` +
  `glVertexArrayAttrib*` (Direct State Access).
- **`Camera.{h,cpp}`** — câmera **orbital** (target/distância/yaw/pitch) com presets.
- **`GltfLoader.{h,cpp}`** — carrega o `.glb` via tinygltf (cena completa: tabuleiro modular
  + peças + mesa/tapete/relógio), extrai imagens via stb_image.
- **`Picker.{h,cpp}`** — picking 3D por **ray-cilindro** (unproject do cursor → raio →
  interseção com cilindros das peças), preciso mesmo com peças altas.

Contexto: OpenGL 4.6 Core, UBO `CameraBlock` em binding 0, MSAA 4×, Blinn-Phong com normal
maps e texturas PBR externas (ambientCG, CC0); `glDebugMessageCallback` (KHR_debug) em Debug.

### `src/anim/` — Animação
- **`Easing.h`** — funções de easing (cubic, quad, back, bounce…).
- **`Animator.{h,cpp}`** — sistema de **tweens** sobre peças visuais (posição/rotação/escala/
  alpha): slide reto, **cavalo em arco parabólico**, captura com fade, roque em sequência,
  promoção, **queda do rei** no xeque-mate (`easeOutBounce`).

### `src/app/` — Aplicação e orquestração
- **`Application.{h,cpp}`** — costura tudo; **máquina de estados** (Menu / Lobby / Jogando /
  Fim de jogo); dispara IA na vez certa; trata cliques, promoção modal, undo, relógio.
- **`Window.{h,cpp}`** — RAII de GLFW + bootstrap do glad + integração ImGui; instala o
  debug callback do GL.
- **`GameSession.{h,cpp}`** — estado de jogo **puro** (sem render/UI), reaproveitado nos testes.
- **`HeadlessRunner.{h,cpp}`** — modo sem janela com flags de CLI (`--auto`, `--mode`,
  `--white`, `--black`, `--moves`, `--print-result`…) para testes e partidas automatizadas.

### `src/ui/` — Interface (Dear ImGui)
- **`GameUi.{h,cpp}`** — menu principal (modos, cor, engine por lado, controle de tempo),
  HUD (relógios, capturas, histórico), diálogo de **promoção**, **lobby** LAN, tela de fim de
  jogo e **painel de debug da IA** (tecla F3: profundidade/nós/tempo/eval/PV).

### `src/input/` — Entrada
- **`InputHandler.{h,cpp}`** — *thunks* estáticos de GLFW → métodos; **encaminha** eventos ao
  ImGui antes (respeita `WantCaptureMouse/Keyboard`); distingue clique de arraste por
  tolerância de pixels/tempo; teclas de câmera/dificuldade/debug.

### `src/net/` — Multiplayer LAN
- **`LanConnection.{h,cpp}`** — conexão **TCP 1:1** cross-platform (Winsock/POSIX), thread
  leitora, *framing* de 4 bytes big-endian.
- **`Protocol.{h,cpp}`** — mensagens de texto `HELLO`/`ROLE`/`START`/`MOVE`/`TC`/`BYE`.

### `src/core/` — Utilidades
- **`BoardCoords.h`** — conversão casa↔mundo (`squareToWorld`/`worldToSquare`).

### `src/platform/` — Plataforma e distribuição
- **`AssetPaths.{h,cpp}`** — resolve o diretório de `assets/` em runtime: override do bootstrap
  → env `CHESS3D_ASSETS_DIR` → `$APPDIR` (AppImage) → ao lado do executável → caminho de build
  (dev). Permite o mesmo binário rodar da árvore de código, de um ZIP portátil ou do AppImage.
- **`PayloadBootstrap.{h,cpp}`** — só no `.exe` empacotado do Windows: extrai o payload
  embutido (assets + engines) para `%LOCALAPPDATA%\chess3d` na 1ª execução.

---

## 5. Build e distribuição

### Build de desenvolvimento
`CMake + Ninja + vcpkg` (manifest, `vcpkg.json`). Presets em `CMakePresets.json`
(`windows-mingw`, `linux-gcc`). A engine é compilada como `chess3d_lib` (estática),
reaproveitada pelo executável `chess3d` e pelos testes `chess3d_tests`.

### Empacotamento auto-contido
- **Windows — `.exe` único auto-extraível:** link **estático** (`x64-mingw-static`, sem DLLs
  do MinGW); os assets + engines são zipados em build-time e embutidos como recurso **RCDATA**
  (windres); na 1ª execução o `PayloadBootstrap` extrai via `tar.exe` nativo do Windows 10/11.
- **Linux — AppImage:** `linuxdeploy` empacota as libs dependentes (libGL/X11 vêm do host) e
  `appimagetool` gera o `.AppImage`; ver `packaging/linux/build_appimage.sh`.
- **Engines:** `cmake/FetchEngines.cmake` + `packaging/engines.manifest` (fonte única de
  versões/URLs) baixam Stockfish/Berserk das releases oficiais no momento do empacotamento
  (no Linux, o Berserk é compilado do código). Licenças GPLv3 incluídas no bundle.

### CI/CD (`.github/workflows/`)
- **`ci.yml`** — build + testes (Catch2 rápido + perft lento) em Windows e Linux + job de
  *interop* LAN entre dois processos.
- **`release.yml`** — em tags `v*`, builda os dois artefatos e publica a **Release** com o
  `.exe` e o `.AppImage` anexados.

---

## 6. Testes e validação

- **Catch2 (rápidos):** regras (roque, en passant, promoção, repetição, 50 lances), IA
  (sempre lance legal, mate em 1/2, profundidade), LAN (framing/loopback), protocolo,
  `GameSession`, factory de agentes.
- **perft (lentos):** contagem exata de nós contra a tabela de referência (starting, Kiwipete,
  posições 3/4).
- **Smoke headless:** `chess3d --auto --max-plies 150 --print-result` deve imprimir `RESULT …`.
- **Interop LAN:** scripts em `tests/scripts/` sobem host+cliente e comparam o resultado.

---

## 7. Como o projeto foi construído com o Claude Code

O projeto foi desenvolvido em parceria com o **Claude Code** (agente de IA da Anthropic) — o
que conversa diretamente com o tema da cadeira: *programar **com** agentes*. O fluxo foi
**incremental por fases**, com validação objetiva ao fim de cada uma:

| Fase | Entrega | Validação |
|---|---|---|
| 0–1 | Ambiente (vcpkg/CMake) + esqueleto + janela GLFW | abre contexto OpenGL 4.6 |
| 2–3 | Shader/Mesh/Câmera (DSA) + loader glTF | cena renderiza |
| 4 | **Engine de xadrez** | **perft** bate com a referência |
| 5–6 | Integração lógica↔render (picking) + animações | humano×humano jogável |
| 7–8 | IA Minimax → **α-β + PSTs + 3 níveis** | poda **~38×**; testes de IA verdes |
| 9–10 | UI (ImGui) + polish (PBR, relógio, quiescence) | 60 FPS, UX |
| 11–12 | Port Linux + **LAN/hotseat** + suíte de testes/CI | interop LAN + CI verde |
| 13 | **Distribuição auto-contida** (Win `.exe` + AppImage) | binários rodam sem instalar |

Princípios que guiaram o trabalho do agente:
- **Validação mensurável** a cada passo (perft, métrica de poda, smoke tests) em vez de
  "parece que funciona".
- **Separação de responsabilidades** (engine pura testável, render isolado, plataforma à parte).
- **Decisões registradas** — desvios do plano original anotados (ex.: OpenGL 3.3 → 4.6 com DSA;
  Ninja no lugar de Make; troca de asset para eliminar *aliasing*).
- Na fase de distribuição, dois problemas **de infraestrutura de CI** foram diagnosticados e
  corrigidos: tokens do cache `x-gha` do vcpkg e bump do vcpkg para resolver um 404 do 7-Zip.

---

## 8. Links

- **Releases (baixar e jogar):** https://github.com/Deivison-Costaa/chess3d/releases/tag/v0.1.0
- **Repositório:** https://github.com/Deivison-Costaa/chess3d
- **Slides de apresentação:** [`docs/APRESENTACAO.md`](APRESENTACAO.md)
- **Documento de design original:** [`PROJECT_PROMPT.md`](../PROJECT_PROMPT.md)
- **Como compilar / rodar:** [`README.md`](../README.md)
