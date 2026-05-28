# PROJECT_PROMPT.md — Xadrez 3D em C++ com Agente Inteligente

> **Como usar este documento**: cole este arquivo inteiro na raiz de uma pasta vazia do seu projeto (ex: `C:\dev\chess3d\`) e abra o Claude Code apontando para essa pasta (`cd C:\dev\chess3d && claude`). Use o comando `/init` na primeira sessão para o Claude analisar este documento e gerar o `CLAUDE.md` complementar. Implementação por **fases** — não tente fazer tudo de uma vez.

---

## 1. Visão geral

### Objetivo

Construir um **jogo de xadrez 3D em C++** com renderização OpenGL e um **agente inteligente com múltiplos níveis de dificuldade**, para apresentação em cadeira de **Programação com Agentes** (UFPB/CIn).

A ênfase do projeto está em **dois eixos**:

1. **Agente racional clássico** — implementação didática de Minimax com poda alpha-beta, função de avaliação heurística e diferentes níveis de profundidade, justificando cada decisão de design como agente baseado em utilidade.
2. **Apresentação visual impactante** — animações suaves, câmera 3D orbital, materiais PBR, highlights de movimentos válidos, animação de captura, queda do rei no xeque-mate.

### Plataforma alvo

- **Sistema operacional**: Windows 10/11
- **Compilador**: MSVC (Visual Studio 2022 Community) ou MinGW-w64 com g++ 13+
- **Build system**: CMake 3.20+
- **Gerenciador de dependências**: vcpkg (modo manifest)
- **API gráfica**: OpenGL 3.3 Core Profile (compatibilidade máxima)
- **Linguagem**: C++17 (mínimo) ou C++20 (preferível, para `concepts` e `std::format`)

### Critério de sucesso

- [ ] Carrega o modelo glTF e renderiza tabuleiro + 32 peças
- [ ] Jogador humano joga com brancas usando mouse (picking 3D)
- [ ] Agente joga com pretas em 3 níveis (Fácil/Médio/Difícil)
- [ ] (Bônus) Nível Mestre via Stockfish UCI
- [ ] Movimentos animados suavemente, cavalo pula em arco, captura tem efeito visual
- [ ] Detecta xeque, xeque-mate, empate, roque, promoção, en passant
- [ ] Menu inicial e HUD funcionais (ImGui)
- [ ] Roda a 60 FPS em hardware modesto

---

## 2. Stack técnica

### Bibliotecas

Todas via **vcpkg** (modo manifest, ver `vcpkg.json` abaixo):

| Lib | Função | Justificativa |
|---|---|---|
| `glfw3` | Janela + input | Padrão, multiplataforma, ativo |
| `glad` | Loader OpenGL | Header gerado, sem runtime overhead |
| `glm` | Matemática (vec/mat/quat) | Sintaxe GLSL-like, header-only |
| `tinygltf` | Loader glTF 2.0 | Single-header, mais simples que Assimp, suficiente pra cargas estáticas |
| `stb` | Carregamento de texturas (`stb_image.h`) | Padrão de facto |
| `imgui` | UI imediata | Menu, HUD, debug — essencial pra apresentação |
| `spdlog` | Logging | Logs coloridos, útil pra depurar agente |
| *(opcional)* `catch2` | Testes unitários da engine | Crítico pra validar gerador de movimentos |

### Estrutura sugerida

```
chess3d/
├── CMakeLists.txt
├── vcpkg.json
├── PROJECT_PROMPT.md           ← este arquivo
├── CLAUDE.md                   ← gerado via /init
├── README.md
├── .gitignore
├── assets/
│   ├── models/
│   │   └── chessboard.glb      ← modelo glTF da Jaximus
│   ├── shaders/
│   │   ├── pbr.vert
│   │   ├── pbr.frag
│   │   ├── highlight.vert
│   │   ├── highlight.frag
│   │   └── shadow.{vert,frag}  (opcional)
│   └── fonts/
│       └── (fontes ImGui)
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── Application.{h,cpp}     ← loop principal, integra tudo
│   │   └── Window.{h,cpp}          ← wrapper GLFW
│   ├── core/
│   │   ├── Logger.h
│   │   └── Types.h                 ← typedefs (Square, Piece, Color, etc)
│   ├── chess/
│   │   ├── Board.{h,cpp}           ← representação 8x8
│   │   ├── Move.{h,cpp}            ← struct Move + flags (captura, roque, promoção)
│   │   ├── MoveGenerator.{h,cpp}   ← gera movimentos pseudo-legais e legais
│   │   ├── Rules.{h,cpp}           ← xeque, mate, empate, regras especiais
│   │   ├── GameState.{h,cpp}       ← histórico, turno, repetição 3x, regra 50 lances
│   │   └── Notation.{h,cpp}        ← FEN, SAN (algébrica), PGN básico
│   ├── ai/
│   │   ├── Agent.h                 ← interface abstrata
│   │   ├── MinimaxAgent.{h,cpp}    ← Minimax + alpha-beta
│   │   ├── Evaluator.{h,cpp}       ← função de avaliação
│   │   ├── OpeningBook.{h,cpp}     ← opcional: aberturas pré-computadas
│   │   ├── StockfishAgent.{h,cpp}  ← opcional: UCI wrapper
│   │   └── DifficultyLevels.h      ← enum + parâmetros
│   ├── render/
│   │   ├── Renderer.{h,cpp}        ← orquestrador
│   │   ├── Shader.{h,cpp}
│   │   ├── Mesh.{h,cpp}            ← VAO/VBO/EBO
│   │   ├── Texture.{h,cpp}
│   │   ├── Material.{h,cpp}        ← PBR maps
│   │   ├── Camera.{h,cpp}          ← orbital com mouse
│   │   ├── GltfLoader.{h,cpp}      ← extrai meshes nomeadas
│   │   └── Picker.{h,cpp}          ← ray casting mouse → casa
│   ├── anim/
│   │   ├── Animator.{h,cpp}        ← sistema de tweens
│   │   ├── Easing.h                ← funções de easing
│   │   └── MoveAnimation.{h,cpp}   ← trajetórias por tipo de peça
│   ├── ui/
│   │   ├── MainMenu.{h,cpp}
│   │   ├── HUD.{h,cpp}
│   │   └── DebugPanel.{h,cpp}      ← visualizar busca da IA em tempo real
│   └── input/
│       └── InputHandler.{h,cpp}
└── tests/
    ├── test_board.cpp
    ├── test_movegen.cpp           ← perft é OBRIGATÓRIO (ver §7)
    └── test_evaluator.cpp
```

### Conteúdo do `vcpkg.json`

```json
{
  "name": "chess3d",
  "version-string": "0.1.0",
  "dependencies": [
    "glfw3",
    "glad",
    "glm",
    "tinygltf",
    "stb",
    "imgui",
    "spdlog",
    {
      "name": "imgui",
      "features": ["glfw-binding", "opengl3-binding"]
    },
    "catch2"
  ]
}
```

### Notas sobre o build no Windows

- Use **vcpkg manifest mode**: na raiz do projeto, `vcpkg install` lê o `vcpkg.json` e baixa tudo na pasta `vcpkg_installed/`.
- No CMake, integrar via `-DCMAKE_TOOLCHAIN_FILE=<caminho_vcpkg>/scripts/buildsystems/vcpkg.cmake`.
- No PowerShell:
  ```powershell
  cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake
  cmake --build build --config Release
  ```

---

## 3. O modelo 3D

### Origem

[Chessboard with Black and White Pieces — Jaximus (CGTrader)](https://www.cgtrader.com/free-3d-models/sports/game/chessboard-with-black-and-white-pieces) — licença Royalty Free (no AI). **Creditar autor no README**.

### Características confirmadas

- Formato: glTF 2.0 (`.gltf` + `.bin` + texturas), 14.3 MB total
- 16 peças pretas + 16 peças brancas + tabuleiro
- **Meshes separadas com nomes lógicos**: `pawn_white`, `bishop_black`, `rook_white`, `king_black`, etc.
- Texturas PBR 2K: Base Color, Roughness, Normal, Metalness
- Topologia limpa (quads, sem n-gons, manifold)
- UVs sem overlap
- **Sem rigging, sem animação** → animação 100% programática

### Estratégia de carregamento

1. Carregar o `.gltf` com tinygltf
2. Iterar pelos `nodes`/`meshes` e identificar por nome (`pawn_white`, etc.)
3. **Armazenar uma malha por tipo de peça** (peão branco, peão preto, etc. — 13 malhas: 6 tipos × 2 cores + tabuleiro) e **instanciar** ao renderizar (instancing por uniform de matriz model, ou loop simples — 32 peças não precisam de instancing GPU)
4. Texturas: carregar 1 set PBR e reaproveitar (a cor da peça pode vir da textura ou de um tint via uniform)
5. **Normalizar escala**: descobrir o bounding box do tabuleiro e escalar tudo para que o tabuleiro fique em `[-4, +4]` em X e Z, com casa de tamanho 1.0 em coordenadas de mundo. Isso facilita o mapeamento `(file, rank) → world_pos`.

### Mapeamento lógico → 3D

```
Casas: file = 0..7 (a..h), rank = 0..7 (1..8)

world_x = (file - 3.5) * SQUARE_SIZE
world_z = (rank - 3.5) * SQUARE_SIZE  // ou negativo, dependendo da orientação
world_y = ALTURA_TABULEIRO            // peças apoiadas em cima
```

---

## 4. Engine de xadrez (módulo `chess/`)

### Representação do tabuleiro

Use **mailbox 8×8** simples (não bitboards). Justificativa: didática, suficiente pra profundidades 2–6, e fácil de explicar na apresentação.

```cpp
enum class PieceType : uint8_t { None, Pawn, Knight, Bishop, Rook, Queen, King };
enum class Color : uint8_t { White, Black };

struct Piece {
    PieceType type = PieceType::None;
    Color color = Color::White;
    bool hasMoved = false;  // para roque e peão duplo
};

class Board {
    Piece squares[64];  // index = rank * 8 + file
    Color sideToMove;
    int enPassantSquare = -1;  // -1 se nenhum
    bool castlingRights[4];    // KQkq
    int halfmoveClock = 0;     // regra dos 50 lances
    int fullmoveNumber = 1;
    // ...
};
```

### Geração de movimentos

Implementar gerador **pseudo-legal** + filtro de legalidade (descartar movimentos que deixam próprio rei em xeque):

- Movimentos de peão: avanço simples, avanço duplo, captura diagonal, en passant, promoção
- Cavalo: 8 deltas fixos `{±1, ±2}`
- Bispo/Torre/Rainha: sliding pieces, iterar direções até bloquear
- Rei: 8 deltas + roque (curto e longo, com todas as restrições)

### Validação de regras especiais

- **Roque**: rei não pode estar em xeque, não pode passar por casa atacada, casas entre rei e torre vazias, nenhuma das peças moveu
- **En passant**: somente no lance imediatamente seguinte ao avanço duplo
- **Promoção**: peão chega à última fileira → escolher Q/R/B/N (UI modal)
- **Xeque-mate**: rei em xeque e sem movimentos legais
- **Empate**: stalemate, 50 lances sem captura/peão, repetição tripla, material insuficiente

### Notação

- **FEN** para serializar/deserializar posição (útil pra debug e Stockfish)
- **SAN** (Standard Algebraic Notation) para exibir histórico ("Nf3", "O-O", "Qxe5+")

---

## 5. Agente inteligente (módulo `ai/`)

### Interface

```cpp
class Agent {
public:
    virtual ~Agent() = default;
    virtual Move chooseMove(const Board& board) = 0;
    virtual std::string name() const = 0;
};
```

### Implementação: `MinimaxAgent`

**Algoritmo**: Minimax com poda alpha-beta, busca em profundidade fixa por nível.

```cpp
int alphaBeta(Board& board, int depth, int alpha, int beta, bool maximizing) {
    if (depth == 0 || board.isGameOver())
        return evaluate(board);

    auto moves = generateLegalMoves(board);
    orderMoves(moves, board);  // MVV-LVA: capturas valiosas primeiro

    if (maximizing) {
        int maxEval = -INF;
        for (auto& m : moves) {
            board.makeMove(m);
            int eval = alphaBeta(board, depth - 1, alpha, beta, false);
            board.unmakeMove(m);
            maxEval = std::max(maxEval, eval);
            alpha = std::max(alpha, eval);
            if (beta <= alpha) break;  // poda beta
        }
        return maxEval;
    } else {
        // espelho para minimizing
    }
}
```

### Função de avaliação (Evaluator)

Combinação ponderada de:

1. **Material** (peso dominante)
   - Peão: 100, Cavalo: 320, Bispo: 330, Torre: 500, Rainha: 900, Rei: 20000
2. **Tabelas posicionais (Piece-Square Tables / PSTs)**
   - Valor extra por casa ocupada — incentiva centralização, peões avançados, cavalos em casas fortes
   - Use as tabelas clássicas de [Tomasz Michniewski (Simplified Evaluation Function)](https://www.chessprogramming.org/Simplified_Evaluation_Function)
3. **Mobilidade** (opcional, nível Difícil): `num_movimentos_legais_brancas - num_movimentos_legais_pretas`
4. **Segurança do rei** (opcional): penalidade se o rei está exposto sem peões à frente
5. **Estrutura de peões** (opcional): penalizar peões dobrados, isolados; bonificar passados

> **Para a apresentação acadêmica**, documente isso como **função de utilidade** do agente: ele é um *utility-based agent* no sentido de Russell & Norvig.

### Níveis de dificuldade

| Nível | Profundidade | Heurísticas | Estimativa força |
|---|---|---|---|
| **Fácil** | 2 | Só material | ~800 ELO |
| **Médio** | 4 | Material + PST | ~1400 ELO |
| **Difícil** | 6 | Material + PST + mobilidade + ordenação MVV-LVA | ~1800 ELO |
| **Mestre** *(opcional)* | — | Stockfish UCI, depth 12 ou 1s/lance | >2500 ELO |

**Tempo limite por lance** (importante pra apresentação não travar):
- Implementar `time-limited iterative deepening` no Difícil: começa em depth 1, aumenta, para quando atinge 3 segundos ou profundidade alvo. Retorna a melhor jogada da última iteração completa.

### Otimizações recomendadas (priorizadas)

1. **Move ordering MVV-LVA** (Most Valuable Victim, Least Valuable Attacker) — quase obrigatório, multiplica eficácia da poda
2. **Quiescence search** — em folhas, continua só com capturas até posição "quieta", evita horizon effect (peão prestes a virar rainha)
3. **Transposition table** com Zobrist hashing — opcional, mas se sobrar tempo é grande ganho
4. **Killer moves heuristic** — opcional, ajuda na ordenação

### Stockfish (nível Mestre, opcional)

Comunicação via **protocolo UCI** em subprocesso:

1. Baixar `stockfish.exe` de https://stockfishchess.org/download/
2. Spawn como subprocesso (`_popen` ou `CreateProcess`)
3. Enviar comandos UCI: `uci`, `isready`, `position fen <FEN>`, `go movetime 1000`
4. Parsear resposta `bestmove e2e4`
5. Encapsular em `StockfishAgent : public Agent`

---

## 6. Renderização (módulo `render/`)

### Pipeline

OpenGL 3.3 Core. Shader PBR simplificado:

- **Vertex shader**: transforma posição, normal, tangent; passa UVs
- **Fragment shader**: PBR com Cook-Torrance (GGX + Schlick + Smith), 1–2 luzes direcionais, ambient via cor constante ou IBL simples
- Para uma versão **mais simples e ainda bonita**: Blinn-Phong com mapa normal — economiza tempo e fica ótimo

### Câmera

**Câmera orbital** ao redor do centro do tabuleiro:
- Mouse drag: rotação (yaw + pitch, com clamp em pitch)
- Scroll: zoom (distância radial)
- Botão direito drag: pan (opcional)
- Tecla `R`: reset view
- Tecla `F`: vista de cima (top-down)
- Tecla `1`/`2`: vista do branco / vista do preto

### Highlights de casas

Desenhar quads semi-transparentes acima do tabuleiro (Y ligeiramente acima da superfície):

- **Azul translúcido**: casa selecionada (peça escolhida)
- **Verde**: casas de destino válidas (movimento normal)
- **Vermelho**: casas de captura
- **Amarelo**: última jogada (origem + destino)
- **Pulsante laranja**: rei em xeque

### Picking 3D (mouse → casa)

Ray casting:
1. Converter mouse `(x, y)` em raio no espaço de mundo (usando matriz de projeção e view invertidas)
2. Intersectar com plano Y = altura do tabuleiro
3. Converter ponto de interseção em `(file, rank)`
4. Validar se está dentro do tabuleiro `[0..7]`

### Sombras (opcional, polish)

Shadow mapping básico com 1 luz direcional. Apenas se tempo permitir.

---

## 7. Sistema de animação (módulo `anim/`)

Esse é o **wow factor da apresentação**. Capricha aqui.

### Estrutura

```cpp
struct PieceTransform {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale = {1, 1, 1};
    float opacity = 1.0f;  // pra fade na captura
};

class Animator {
public:
    void animateMove(PieceID id, glm::vec3 from, glm::vec3 to, MoveType type, float duration);
    void animateCapture(PieceID id, float duration);
    void animatePromotion(PieceID id, PieceType newType, float duration);
    void animateKingFall(PieceID kingId);
    void update(float deltaTime);
    bool isAnimating() const;  // bloqueia input enquanto anima
private:
    std::vector<Tween> activeTweens;
};
```

### Trajetórias por tipo de peça

| Peça | Trajetória |
|---|---|
| Peão, Bispo, Rainha, Rei, Torre | Slide reto, ease-in-out cubic, ~0.4s |
| **Cavalo** | Arco parabólico (altura máxima = 0.8 * distância), ease-out, com pequena rotação no eixo Y (~30°) durante o pulo |
| Roque | Rei desliza, torre **simultaneamente** desliza com 100ms de delay — fica visualmente claro que é uma jogada só |

### Captura

Sequência:
1. Peça atacante chega à casa (~0.4s)
2. **Antes do impacto**: peça capturada sofre `scale 1.0 → 1.1 → 0.0` + `opacity 1 → 0` em 0.3s
3. Pequeno efeito: peça atacante "balança" levemente (rotação ±5° em Y, 0.1s)
4. (Opcional) Spawn de partículas: 20–30 quads pequenos com física simples, fade-out 0.5s

### Promoção

1. Peão chega à última fileira
2. UI modal pausa e pergunta peça (Q/R/B/N) — default rainha
3. Peão sobe levemente (0.2s)
4. Crossfade: peão diminui escala enquanto a nova peça aparece e cresce ao mesmo tempo (0.4s)

### Xeque-mate (queda do rei)

Quando detectado mate:
1. Pausa de 0.5s
2. Rei perdedor **tomba**: rotação 90° no eixo X (ou Z, dependendo da direção), com ease-out + bounce sutil ao final
3. Câmera faz **slow zoom-in** no rei caído (3s)
4. UI: "Mate em X lances. Vitória das brancas/pretas." com botão de nova partida

### Easing functions

```cpp
namespace Easing {
    float linear(float t) { return t; }
    float easeInOutCubic(float t) {
        return t < 0.5f ? 4*t*t*t : 1 - powf(-2*t+2, 3)/2;
    }
    float easeOutQuad(float t) { return 1 - (1-t)*(1-t); }
    float easeOutBounce(float t);  // implementação padrão
}
```

### Regra de ouro

**Bloquear input durante animações.** Use `Animator::isAnimating()` no loop principal: se true, não aceita cliques. Caso contrário, fica fácil quebrar a sincronia entre estado lógico e visual.

---

## 8. UI (módulo `ui/`) com ImGui

### Menu inicial

- Título estilizado
- Botão: **Nova Partida**
- Combo: cor do jogador humano (Brancas / Pretas / Aleatório)
- Combo: dificuldade (Fácil / Médio / Difícil / Mestre)
- Checkbox: animar movimento da IA (default on)
- Botão: **Carregar PGN** (opcional)
- Botão: **Sair**

### HUD durante a partida

- Canto superior esquerdo: turno atual, indicador "Pensando..." quando IA processa
- Canto superior direito: capturas (peças pretas capturadas | peças brancas capturadas)
- Canto inferior direito: **histórico de movimentos em SAN** com scroll
- Canto inferior esquerdo: botões `Desfazer | Render Debug | Menu`

### Painel de Debug (tecla F3)

Esse painel é **ouro para a apresentação acadêmica** — mostra que o agente é um sistema real, não caixa-preta:

- Profundidade atual da busca
- Nós explorados (com e sem poda)
- Tempo de busca
- **Linha principal (PV — Principal Variation)**: melhor sequência prevista
- Valor de avaliação da posição (em centipawns)
- Top 5 movimentos candidatos com seus valores
- (Opcional) Visualização da árvore de busca em modo simplificado

---

## 9. Fases de implementação (siga em ordem!)

Cada fase termina com um deliverable testável. **Não pule fases.**

### Fase 1 — Setup (½ dia)
- [ ] Criar repo, `.gitignore`, `CMakeLists.txt`, `vcpkg.json`
- [ ] Janela GLFW abre, fundo colorido limpa a tela
- [ ] Build no Release e Debug funciona via PowerShell

### Fase 2 — Hello Mesh (1 dia)
- [ ] Shader simples carrega
- [ ] Triângulo colorido aparece
- [ ] Câmera orbital com mouse funciona
- [ ] Cubo com iluminação Phong

### Fase 3 — Loader glTF (1–2 dias)
- [ ] `GltfLoader` carrega `chessboard.glb`
- [ ] Mapeia nomes de meshes (`pawn_white`, etc.) para `std::map<std::string, Mesh>`
- [ ] Texturas PBR carregam
- [ ] Cena renderiza tabuleiro + 32 peças na posição inicial (hardcoded por enquanto)

### Fase 4 — Engine de xadrez (2–3 dias) 🔴 CRÍTICO
- [ ] `Board`, `Piece`, `Move` definidos
- [ ] `MoveGenerator` gera movimentos legais para todas as peças
- [ ] **Teste perft obrigatório**: o `MoveGenerator` deve passar nos perft de profundidade 1–4 na posição inicial:
  - `perft(1) = 20`
  - `perft(2) = 400`
  - `perft(3) = 8902`
  - `perft(4) = 197281`
  - (referência: https://www.chessprogramming.org/Perft_Results)
- [ ] Detecta xeque, mate, empate, roque, en passant, promoção
- [ ] FEN parsing/serialização

### Fase 5 — Integração lógica ↔ render (1 dia)
- [ ] Função `worldToSquare(vec3) → Square` e `squareToWorld(Square) → vec3`
- [ ] Picker funciona: clique numa peça destaca ela
- [ ] Mostrar movimentos legais como highlights verdes/vermelhos
- [ ] Mover peça sem animação (teleporta) — ainda assim deve atualizar `Board`

### Fase 6 — Animação (1–2 dias)
- [ ] `Animator` com tweens lineares
- [ ] Easing functions
- [ ] Movimento normal animado
- [ ] **Cavalo pula em arco** ✨
- [ ] Captura com fade
- [ ] Roque com torre seguindo

### Fase 7 — IA básica (2 dias) 🔴 CRÍTICO
- [ ] `MinimaxAgent` com profundidade fixa = 2
- [ ] `Evaluator` apenas material
- [ ] Joga partida completa contra humano
- [ ] Detecta mate corretamente (não permite IA "alucinar")

### Fase 8 — Alpha-beta + níveis (1 dia)
- [ ] Adicionar poda alpha-beta
- [ ] PSTs implementadas
- [ ] Move ordering MVV-LVA
- [ ] Níveis Fácil/Médio/Difícil expostos no menu

### Fase 9 — UI completa (1 dia)
- [ ] Menu inicial ImGui
- [ ] HUD em jogo
- [ ] Histórico SAN
- [ ] Diálogo de promoção
- [ ] Tela final (mate/empate)

### Fase 10 — Polish (1–2 dias)
- [ ] Animação de mate (queda do rei + zoom)
- [ ] Sons (opcional): movimento, captura, xeque
- [ ] Painel de debug F3
- [ ] Iterative deepening + tempo limite no Difícil
- [ ] Quiescence search

### Fase 11 — Stockfish (opcional, 1 dia)
- [ ] Wrapper `StockfishAgent` via UCI
- [ ] Nível Mestre no menu

### Fase 12 — Apresentação (½ dia)
- [ ] Posições de demonstração pré-carregadas (FENs interessantes)
- [ ] Slides com arquitetura
- [ ] Script de demo: abertura → meio-jogo tático → final com mate

---

## 10. Argumentação para a cadeira de Agentes

Use **essa terminologia** ao apresentar, alinhando com Russell & Norvig:

| Conceito | Como aparece no projeto |
|---|---|
| **Ambiente** | Determinístico, totalmente observável, sequencial, estático, discreto, multi-agente competitivo |
| **PEAS** | Performance: vitórias/empates. Environment: tabuleiro 8×8. Actuators: movimentos. Sensors: estado do tabuleiro |
| **Tipo de agente** | Utility-based agent com busca adversarial |
| **Função de utilidade** | `Evaluator::evaluate()` — combina material + posição + mobilidade |
| **Algoritmo de decisão** | Minimax com poda alpha-beta + iterative deepening |
| **Trade-off** | Profundidade × tempo de resposta (justificativa dos níveis) |
| **Racionalidade limitada** | Profundidade finita + função heurística (não joga "perfeito", mas é racional dentro do orçamento computacional) |

**Slide-killer**: gravar uma partida onde o agente sacrifica material por mate em N — mostra que a busca enxerga além do material imediato. Esse é o exemplo concreto de *lookahead* + função de utilidade que professores adoram.

---

## 11. Riscos e mitigações

| Risco | Mitigação |
|---|---|
| Bug no gerador de movimentos passa despercebido | **Perft obrigatório na Fase 4** — não avance sem isso |
| IA muito lenta no Difícil | Iterative deepening com timeout; cap de profundidade |
| Picking 3D errado em ângulos extremos | Testes com câmera em várias posições; clamp do pitch |
| Modelo glTF não carrega corretamente | Testar antes no [glTF Viewer](https://gltf-viewer.donmccurdy.com/); cair para Assimp se tinygltf falhar |
| Animação dessincroniza estado lógico | Bloquear input durante animação + atualizar estado lógico **antes** de iniciar animação |
| Memória vaza com tweens | RAII; tweens em `std::vector` que se removem ao terminar |
| Não dá tempo de fazer Stockfish | Cortar — é opcional, e os 3 níveis Minimax já são suficientes pra cadeira |

---

## 12. Convenções de código

- **C++ moderno**: `std::unique_ptr`, `std::optional`, `enum class`, RAII estrito
- **Nomes**: classes `PascalCase`, métodos/variáveis `camelCase`, constantes `UPPER_SNAKE`
- **Headers**: include guards com `#pragma once`
- **Sem `using namespace std;`** em headers
- **Const correctness** em todo getter/método não-mutador
- **Logging**: `spdlog::info/warn/error/debug`, nunca `std::cout` em código de produção

---

## 13. Próximo passo após colar este documento

No Claude Code, após `claude` na pasta do projeto:

1. `/init` — gera CLAUDE.md inicial
2. "Leia o PROJECT_PROMPT.md inteiro e me confirme que entendeu o escopo. Não comece a implementar ainda. Liste só as fases que vai executar e me pergunte sobre qualquer ambiguidade."
3. Após confirmação: "Vamos começar pela Fase 1. Configure o `CMakeLists.txt`, `vcpkg.json` e crie a estrutura de pastas."
4. Não pule fases. Ao fim de cada fase, faça commit com mensagem clara (`feat(chess): perft passes depth 4`).

---

## 14. Créditos

- Modelo 3D: **Jaximus** ([CGTrader](https://www.cgtrader.com/designers/jaximus)) — Royalty Free License
- Tabelas posicionais: Tomasz Michniewski, *Simplified Evaluation Function*, chessprogramming.org
- Stockfish (opcional): GPLv3 — incluir aviso se distribuir binário junto

---

**Bons jogos e boa apresentação.** ♞
