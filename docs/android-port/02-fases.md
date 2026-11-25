# Fases do port (uma fase = uma conversa)

Convenção: "**Levar**" = arquivos que devem ser anexados/colados na conversa (além de `README.md`,
`03-decisoes.md` e a última entrada do `LOG.md`, que **sempre** vão). Caminhos relativos à raiz do repo.
Se uma fase crescer demais, divida em `P<n>a`, `P<n>b` e registre no quadro.

## Grafo de dependências
```
P0 → P1 → P2 → P3 ─┬→ P4 ──────────┐
                   ├→ P5 → P6 ─┬→ P7 ─┬→ P9 ─┐
                   └→ P10      └→ P8 ─┘      ├→ P11 → P12 → P13
                                             │
                          (P4,P10 podem ir em paralelo a P5–P9, em outra conta)
```
Paralelizáveis entre contas: **P4** (áudio) e **P10** (entrada) após P3; **P7** (GLES) e **P8** (Vulkan) após P6.
Regra para trabalho paralelo: cada conta em sua branch `android/p<n>-<tema>`; só `README.md` (quadro) e `LOG.md` geram conflito — resolver manualmente.

---

## P0 — Fundação e decisões
**Objetivo:** fechar as decisões que moldam tudo.
**Tarefas:** confirmar D-001..D-006 (`03-decisoes.md`); criar branch `android/main`; commitar esta pasta; criar `LOG.md`.
**Levar:** `README.md` do repo, `docs/ANALYSIS.md`, esta pasta.
**Pronto quando:** todas as decisões D-001..D-006 estão `DECIDIDA`.

## P1 — Build Android (NDK/CMake/Gradle/CI)
**Objetivo:** projeto Android que compila as bibliotecas e um `.so` vazio do engine.
**Tarefas:**
1. Separar o `CMakeLists.txt`: manter o ramo MSVC e criar ramo `ANDROID` (toolchain NDK) sem quebrar o Windows.
2. Módulo Gradle `android/` (AGP + CMake externo), `minSdk`/`targetSdk` conforme D-004; **ABI única `arm64-v8a`** (D-001).
3. Compilar para Android: Lua, libpng, minizip, zlib, (SDL/SDL_mixer/SDL_ttf/libresample/SDL_gfx conforme D-002/D-003).
4. Remover GLEW e o import-lib `.def` do caminho Android (`cmake/MugenSdk.cmake`, `cmake/ThirdParty.cmake`).
5. Workflow de CI para Android (`.github/workflows/android.yml`) gerando APK debug como artefato.
**Levar:** `CMakeLists.txt`, `CMakePresets.json`, `cmake/*`, `third_party/README.md`, `.github/workflows/build.yml`, `tools/msvc_env.bat`.
6. App de diagnóstico mínimo que registra as informações do aparelho de referência em `03-decisoes.md` (D-010).
**Pronto quando:** `./gradlew assembleDebug` no CI gera APK arm64 que roda no SM-A065M; Windows continua verde.

## P2 — Portabilidade do código (64 bits, Win32, convenções)
**Objetivo:** todos os `src/*.c` compilam com clang/NDK (ainda sem rodar).
**Tarefas:**
1. Criar `src/platform/platform.h`: `MUGEN_STDCALL`, `MUGEN_INT64`, `MUGEN_INLINE`, macros de export; substituir `__stdcall/__int64/__inline/__declspec`.
2. `MUGEN_ASSERT_SIZE` condicional ao alvo (D-001); listar structs com ponteiros/`DWORD`-like e corrigir casts ponteiro↔int.
3. Isolar `windows.h`: substituir por `platform/` nos 10 arquivos (ver `01-analise-do-repo.md` §2).
4. `Trunc64` explícito e portável (R2); flags `-ffp-contract=off`, sem `-ffast-math`.
5. Stubs temporários para o que depende de SDL/GL, para fechar a compilação.
**Levar:** `src/mugen.h`, `src/symbols.h`, `src/crt.h`, `src/ftol.h`, `src/game.h`, `src/char.h`, `src/player.h`, `src/sctrl.h`, `src/error.c`, `src/matchload.c`, `src/timer.c`, `src/system.c` (trechos Win32), mais a lista de erros de compilação gerada pelo CI da P1.
**Pronto quando:** compila `src/*.c` para `arm64-v8a` sem erros; build x86 Windows idêntico em comportamento (log).

## P3 — Camada de plataforma (janela, eventos, tempo, arquivos)
**Objetivo:** o engine inicia no Android até criar janela/superfície e ler arquivos.
**Tarefas:**
1. Implementar D-002 (sdl12-compat ou SDL2): `video.c`, `sdlmain.c`, `system.c` (título/foco), `events.c`.
2. `timer.c` com `clock_gettime`; `Sleep` → `nanosleep`.
3. `file.c`: normalizar `\`, resolução de caminho **insensível a caixa** com cache de diretório; diretório de dados (D-005).
4. `log.c`/`error.c`: log via `__android_log_print` + arquivo; diálogo de erro (substituto de `DialogBoxParamA`).
5. Ciclo de vida: callbacks de pause/resume/terminate chegando ao loop (`gameloop.c`).
**Levar:** `src/video.c`, `src/system.c`, `src/sdlmain.c`, `src/main.c`, `src/events.c`, `src/frame.c`, `src/gameloop.c`, `src/timer.c`, `src/file.c`, `src/stream.c`, `src/ini.c`, `src/log.c`, `src/error.c`, `src/config.c`, `src/cmdline.c`, `src/platform/*`.
**Pronto quando:** app abre, loga `mugen.log` no diretório definido e encontra arquivos do jogo ignorando caixa.

## P4 — Áudio e BGM  *(paralelizável)*
**Objetivo:** efeitos sonoros e música tocando.
**Tarefas:** SDL_mixer no Android; `sound.c`/`wav.c`; thread BGM (`bgmthread.c`, pthread nativo); plugins: o
"mpg123" usa `LoadLibrary` → linkar estaticamente ou substituir (D-006); latência de buffer; pausa/retomada de áudio.
**Levar:** `src/sound.c`, `src/sound.h`, `src/wav.c`, `src/bgm.c`, `src/bgm.h`, `src/bgmthread.c`, `src/bgmplugins.c`, `src/bgmstream.c`, `third_party/sdk/include/libresample.h`, `third_party/sdk/def/SDL_mixer.def`.
**Pronto quando:** SFX e BGM tocam na tela de título e em luta, sem crash ao pausar/retomar.

## P5 — Primeiro boot com renderer software  **(Marco M1)**
**Objetivo:** tela de título e uma luta rodando no Android usando `RENDER_SYSTEM` (software, RGB565).
**Tarefas:** apresentar o framebuffer software como textura GLES simples (quad) ou `ANativeWindow`; escala/letterbox;
otimizar o caminho de software para o SM-A065M (NEON, resolução interna, escala pela GPU); medir FPS e tempo de frame **no aparelho**; corrigir crashes de 64 bits que aparecerem; teste diferencial de `mugen.log` contra Windows.
**Levar:** `src/video.c`, `src/gfx.c`, `src/gfx.h`, `src/frame.c`, `src/softrender.c`, `src/softblend.c`, `src/sprite.c`, `src/spritecodec.c`, `src/palette.c`, `src/prim.c`, `src/globals.c` (tabelas de renderer), `src/game.h`.
**Pronto quando:** título → select → luta VS jogáveis (mesmo sem toque, com teclado/gamepad); `mugen.log` equivalente ao do Windows nos cenários do README.

## P6 — Abstração de renderer
**Objetivo:** interface única (`src/render/renderer.h`) sobre as tabelas de `Gfx_SelectRenderer`, escondendo GL.
**Tarefas:** definir API mínima (criar/destruir textura e alvo, upload 2D/1D→2D, desenhar sprite com `DrawParams`+PalFX+blend,
clip rect, present, captura/readback, fade/barras); adaptar `glrender.c`, `prim.c`, `gfx.c`, `palette.c`; manter backend GL desktop funcionando no Windows.
**Levar:** `src/gfx.c`, `src/gfx.h`, `src/glrender.c`, `src/glrender.h`, `src/gldetect.c`, `src/prim.c`, `src/palette.c`, `src/palette.h`, `src/sprite.c`, `src/sprite.h`, `src/globals.c`, `src/game.h`, `src/symbols.h`.
**Pronto quando:** Windows (GL) e Android (software) rodam via interface; tabela de operações documentada em `docs/android-port/render-api.md`.

## P7 — Backend GLES  **(Marco M2)**
**Tarefas:** contexto EGL (ou via SDL); GLES 3.0 com fallback 2.0 (D-007 a abrir); eliminar `glBegin/End` (VBO/vertex arrays),
pilha de matrizes (uniforms), `glPushAttrib`, textura 1D → 2D Nx1; FBO core; portar os 9 programas (`GL_PROGRAM_COUNT`) para GLSL ES;
`glBlendEquation`; recriação de recursos após perda de contexto; formato de textura (`GlTex_Upload*`, `Gl_Premultiply`).
**Levar:** `src/glrender.c`, `src/glrender.h`, `src/gldetect.c`, `src/render/*`, `src/prim.c`, `src/shaders/*`, `cmake/EmbedShader.cmake`, trecho de shaders inline de `src/globals.c`.
**Pronto quando:** paridade visual com o GL desktop nos cenários de teste (capturas de tela comparadas); sem erros GL em `glGetError` no log.

## P8 — Backend Vulkan  **(Marco M3)**
**Tarefas:** instância/dispositivo/fila, swapchain (recriar em resize/resume), render pass/dynamic rendering, pipelines por variante
(palfx × quad especial × blend), descritores de textura/paleta, upload staging, render targets (`GlVbuf`), clip rect via scissor,
readback para screenshot; shaders em GLSL→SPIR-V (glslang/shaderc no build, D-008 a abrir); validação via layers em debug; detecção e fallback para GLES.
**Levar:** `src/render/*`, `src/glrender.c` (como referência de semântica), `src/shaders/*`, `src/prim.c`, `docs/android-port/render-api.md`.
**Pronto quando:** paridade com o GLES nas mesmas capturas; sem erros de validação; troca de backend por config.

## P9 — Modelos 3D e shaders de ambiente
**Tarefas:** portar `model.c`, `bg3d.c`, `model.*`, `shadow.*`, `env_*.frag` para os dois backends (depth, sombras, cubemap/panorama, filtro de ambiente);
cgltf/stb em ARM; limites de GLES (formatos de profundidade, extensões).
**Levar:** `src/model.c`, `src/bg3d.c`, `src/shaders/*`, `third_party/cgltf-1.14/`, `third_party/stb/`, `docs/android-port/render-api.md`.
**Pronto quando:** stages 3D de exemplo renderizam igual nos dois backends.

## P10 — Entrada e controles de toque  *(paralelizável)*
**Tarefas:** overlay de controles virtuais (D-pad/analógico + 6 botões + start), layout editável e salvo; mapear para o sistema de teclas
existente sem alterar `cmdinput.c`; gamepad (SDL joystick/GameController) e teclado Bluetooth; UI de configuração (`title.c`); latência de entrada.
**Levar:** `src/events.c`, `src/keyboard.c`, `src/pads.c`, `src/playerinput.c`, `src/keybind.c`, `src/title.c` (trechos de key config), `src/cmdinput.c` (somente leitura), `src/config.c`.
**Pronto quando:** partida completa jogável só por toque e por gamepad.

## P11 — App shell, dados do jogo e ciclo de vida
**Tarefas:** Activity/launcher Kotlin; seleção do diretório do M.U.G.E.N (SAF ou cópia — D-005); validação de dados; tela de configuração
(renderer GLES/Vulkan, resolução, escala); tratamento de rotação/fullscreen/notch; pause/resume/low-memory; `mugen.cfg` por plataforma.
**Levar:** `android/*` (módulo Gradle), `src/platform/*`, `src/config.c`, `src/system.c`, `docs/android-port/LOG.md`.
**Pronto quando:** instalação limpa → escolher pasta → jogar, sem terminal.

## P12 — QA, desempenho e release  **(Marco M4)**
**Tarefas:** matriz de dispositivos (GPU Adreno/Mali/PowerVR, Android versões), perfil de CPU/GPU, correção de vazamentos/ANRs,
assinatura de APK/AAB, CI de release, README de usuário, licenças de terceiros (LGPL, D-006).
**Levar:** `LOG.md`, resultados de teste, `android/*`, `.github/workflows/*`.
**Pronto quando:** APK/AAB assinado em release do GitHub com notas e checklist de testes aprovado.

## P13 — Alterações pós-port  *(reservada)*
Escopo a definir. Regras: marcar no código como mudança do unleashed (`/* MUGEN-UNLEASHED: ... */`), manter flag de
"modo fiel ao original", registrar em `03-decisoes.md`. Quando o escopo existir, quebre em `P13a`, `P13b`…
