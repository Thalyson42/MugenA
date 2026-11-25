# Análise do repo para o port Android

Base: commit `cc94cfe` ("Add 3D models to screen and storyboard backgrounds"), 131 `.c` + 127 `.h`, ~72k linhas, ~9 MB.
Fonte principal de contexto do engine: `docs/ANALYSIS.md` (mapa de módulos, quirks, validação).
*Tudo abaixo foi lido no código; itens marcados **(hipótese)** não foram verificados.*

## 1. Estado atual
- Alvo único: **Win32 x86 de 32 bits, MSVC** (`CMakeLists.txt` dá `FATAL_ERROR` fora disso). CI em `windows-2022`.
- Linguagem C11, um `.c` por módulo do `mugen.exe` original.
- Estático (código-fonte em `third_party/`): Lua 5.1.4, libpng 1.2.38, GLEW 1.5.4, minizip 0.15, cgltf, stb_image.
- **Dinâmico no original, sem código-fonte no repo** (só headers/`.def` em `third_party/sdk`): SDL 1.2, SDL_mixer,
  SDL_ttf, SDL_gfx (só `zoomSurface`), zlib 1.2.3, libresample, pthreads-win32, libmpg123 (via `LoadLibrary`).
  → Para Android é preciso obter/compilar equivalentes (ver D-002, D-003).

## 2. Mapa de impacto por área
| Área | Arquivos | Situação | Trabalho |
|---|---|---|---|
| Janela/contexto GL | `video.c`, `system.c`, `sdlmain.c`, `gfx.c`, `gameloop.c` | `SDL_SetVideoMode(SDL_OPENGL)`, `SDL_GL_SwapBuffers`, `SDL_WM_SetCaption` (SDL 1.2) | Alto |
| Renderer OpenGL | `glrender.c/.h`, `gldetect.c`, `prim.c`, `palette.c`, `gfx.h` | GL 2.x **fixed-function + GLSL**: `glBegin/glVertex2f`, `glMatrixMode`, `glPushAttrib`, GLEW, FBO via `*EXT`, textura 1D (`GlTex_Upload1D`) | **Muito alto** |
| Renderer software | `softrender.c`, `softblend.c`, `sprite.c`, `spritecodec.c` | RGB565, independente de GL | Baixo (portável) — **atalho para o 1º boot** |
| Seleção de renderer | `gfx.c` (`Gfx_SelectRenderer`, 4 tabelas), `game.h` (`RENDER_SYSTEM=0`, `RENDER_OPENGL_SCREEN=1`, `RENDER_OPENGL=2`), `globals.c` | Tabelas de ponteiros de função inicializadas em `globals.c` | Base da abstração (P6) |
| Shaders | `src/shaders/{model,shadow,env}.vert/.frag`, `env_*.frag`, shaders inline em `globals.c`/`glrender.c`, `cmake/EmbedShader.cmake` | GLSL desktop | GLES: reescrever dialeto; Vulkan: SPIR-V |
| Modelos 3D | `model.c` (3375 linhas), `bg3d.c`, cgltf, stb | Adição do unleashed, usa GL | P9 |
| API Win32 | `windows.h` em: `error.c gfx.c gldetect.c glrender.c matchload.c prim.c system.c timer.c video.c mugen.h` | Poucas chamadas: `Sleep`, `SendMessageA`, `SHGetFolderPathA`, `GetModuleFileNameA`, `DialogBoxParamA`, `timeGetTime`, `QueryPerformanceCounter`, `LoadLibrary/GetProcAddress` (mpg123) | Médio |
| Convenção de chamada | ~1659 ocorrências de `__stdcall/__declspec/__cdecl/__int64`… | Principalmente tipos de ponteiro GLEW/GL e `__int64` | Médio (mecânico) |
| Áudio | `sound.c`, `wav.c`, `bgm*.c`, `bgmplugins.c`, `bgmstream.c` | SDL_mixer 1.2 + thread pthread + plugins DLL | Médio |
| Entrada | `events.c`, `keyboard.c`, `pads.c`, `playerinput.c`, `keybind.c`, `title.c` (config de teclas) | SDL 1.2 teclado/joystick | Médio + toque novo |
| Arquivos | `file.c` (`File_Find` tira letra de drive, procura em `base_dir`, motif, `./`, `data/`), `stream.c`, `ini.c` | Assume Windows: caminhos com `\`, **sem diferenciar maiúsculas** | **Alto** (Android é case-sensitive) |
| Tempo | `timer.c` | `timeGetTime`/QPC | Baixo |
| Fontes | `font.c` + SDL_ttf | TrueType | Médio (compilar SDL_ttf/FreeType) |
| Recursos | `mugen.rc`, `tools/extract_icon.py` | Ícone/diálogo de erro Win32 | Substituir por equivalente Android |

## 3. Riscos técnicos (ordem de gravidade)
**R1 — Layout de structs de 32 bits.** 203 structs com `MUGEN_ASSERT_SIZE` (ex.: `GameVars` 0x13108, `Player` 0x2490).
Os tamanhos assumem ponteiros de 4 bytes. Em **arm64** (64 bits) as asserts falham e o layout muda.
Decisão D-001: build 32 bits (armeabi-v7a) vs. 64 bits com asserts condicionais. Afeta ABI, desempenho e distribuição.
Também procurar casts ponteiro↔`int`/`unsigned` e ponteiros guardados em campos de 4 bytes.

**R2 — Determinismo numérico.** O original roda x87 com precisão de 53 bits; o repo usa `/arch:IA32` e atribuições
para forçar arredondamentos. Em ARM: compilar com `-ffp-contract=off`, sem `-ffast-math`, e não deixar o compilador
manter intermediários em `float` onde o original guarda `double`. `Ftol` (`src/ftol.h`) já é portável (checagem de faixa).
**`Trunc64` não é**: `(int)(__int64)x` em ARM satura (`fcvtzs`), enquanto o `fistp` x86 devolve `0x8000000000000000`
(parte baixa = 0) para fora da faixa → precisa de implementação explícita. Validar com teste diferencial contra o build Windows.

**R3 — Renderer GL fixed-function → GLES.** GLES 2/3 não tem `glBegin/End`, pilha de matrizes, `glPushAttrib`,
textura 1D, GLEW, extensões `EXT` de FBO (no ES são core), e o GLSL precisa de `#version 100`/`300 es` + `precision`.
Vulkan é backend inteiramente novo (pipelines por variante de PalFX/blend, render targets, SPIR-V).
`GL_PROGRAM_COUNT = 9` variantes de shader (palfx × quad especial) são a lista mínima de pipelines.

**R4 — Perda de contexto/superfície no Android.** Pausar/retomar destrói o contexto EGL ou o swapchain Vulkan.
O engine mantém texturas e render targets (`GlTex`, `GlVbuf`) em cache → é preciso recriar tudo no resume.

**R5 — Sistema de arquivos.** Conteúdo M.U.G.E.N usa maiúsculas/minúsculas inconsistentes e `\`. Precisa de resolução
de caminho insensível a caixa + normalização de `\` (cache de diretório). Dados ficam em armazenamento escopado:
SAF ou diretório do app (ver D-005).

**R6 — SDL 1.2 não tem porte Android oficial.** Opções em D-002 (sdl12-compat sobre SDL2 **(hipótese, confirmada só por leitura de código na P3a, ver D-002: funciona com
`SDL_OPENGL` no Android — verificar)**, ou migrar direto para SDL2 mexendo em ~28 arquivos que usam SDL).

**R7 — Desempenho.** Engine de 2D/CPU pesada em celulares (software renderer RGB565, resampler, IA). Medir cedo (P5).

**R8 — Bibliotecas sem fonte no repo.** `libresample`, `SDL_gfx` (zoomSurface), `libmpg123`, `SDL_ttf` precisam de fonte
e licença compatível (LGPL em vários → atenção a linkagem estática no APK; ver D-006).

## 4. Atalhos e pontos a favor
- O **renderer software** (`RENDER_SYSTEM`) é independente de GL: dá para chegar à tela de título no Android antes de
  qualquer GLES/Vulkan (marco M1, P5).
- O engine já tem **tabelas de função por renderer** (`Gfx_SelectRenderer`): base natural da abstração (P6).
- Poucas chamadas Win32 distintas; o que existe é concentrado em ~10 arquivos.
- O CI e os testes diferenciais do Windows continuam como referência de regressão.
