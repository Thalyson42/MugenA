# Registro de decisões (ADR) e perguntas em aberto

Formato: `D-nnn` · estado (`ABERTA` / `PROPOSTA` / `DECIDIDA`) · contexto · opções · recomendação · decisão final.
Nunca apague uma decisão: se mudar, crie uma nova que a substitui e marque a antiga como `SUBSTITUÍDA por D-xxx`.

## D-001 · ABI e tamanho de ponteiro — DECIDIDA (2026-11-24)
**Decisão:** alvo primário **`arm64-v8a` (64 bits), execução nativa, sem emulação**. Não haverá build `armeabi-v7a` nem `x86_64`
(emulador) no escopo; o build x86 Windows continua só como referência de comportamento.
**Consequências:** `MUGEN_ASSERT_SIZE` fica condicional (só vale no build Windows 32 bits); os layouts em 64 bits mudam e precisam ser
revisados na P2 (casts ponteiro↔int, ponteiros em campos de 4 bytes, `Trunc64`). Testes diferenciais contra o Windows comparam
**comportamento** (`mugen.log`, saídas), não layout de memória.

## D-002 · Camada SDL no Android — ABERTA
**Contexto:** o engine usa SDL 1.2 em ~28 arquivos; SDL 1.2 não tem porte Android oficial.
**Opções:** (A) `sdl12-compat` sobre SDL2 (menos mudanças; **verificar** suporte a `SDL_OPENGL`/EGL no Android e a API de
`SDL_SetVideoMode`/superfície de 16 bpp usada pelo renderer software). (B) migrar o código para SDL2 direto (mais trabalho, mais controle,
necessário de qualquer forma para Vulkan via `SDL_Vulkan`). (C) camada própria de plataforma sobre NativeActivity/EGL.
**Recomendação provisória:** decidir na P3 após um spike de 1 dia com (A); se falhar, (B).
**Decisão final:** _pendente_

## D-003 · Bibliotecas sem fonte no repo — ABERTA
SDL_mixer, SDL_ttf (+FreeType), SDL_gfx (`zoomSurface`), zlib, libresample, libmpg123. Definir fonte/versão de cada uma
(preferir as de mesma versão do original ou compatíveis) e como entram no build (submódulo, FetchContent ou `third_party/`).
**Decisão final:** _pendente_
**Progresso na P1 (sem decidir):** entram no build Android, a partir de `third_party/`, Lua 5.1.4, libpng 1.2.38, minizip 0.15, cgltf e stb_image;
zlib vem do NDK (ver D-011). SDL/SDL_mixer/SDL_ttf/SDL_gfx/libresample/libmpg123 **não** entram na P1: dependem de D-002 e há um `TODO(P3)` em `cmake/MugenAndroid.cmake`.

## D-004 · Versões Android — PROPOSTA (verificar contra o aparelho de referência, D-010)
`minSdk` 24 (Vulkan 1.0 disponível; 26+ simplifica áudio/SAF) · `targetSdk` atual da Play · NDK LTS recente · AGP/Gradle estáveis.
**Valores usados na P1 (provisórios, ainda não rodados no CI nem no aparelho):** minSdk 24, compileSdk/targetSdk 35, NDK 27.2.12479018 (r27 LTS),
CMake 3.22.1 (do SDK), AGP 8.7.3, Gradle 8.9, Kotlin 2.0.21, JDK 17. **A revisar na P12:** o `targetSdk` exigido pela Play para publicar
(provavelmente mais alto que 35) e o NDK/AGP mais novos; a P1 só gera APK debug.
**Decisão final:** _pendente_

## D-005 · Onde ficam os dados do jogo — ABERTA
(A) Pasta escolhida via SAF e cópia para armazenamento interno do app (simples e rápido, duplica espaço).
(B) Acesso direto via SAF (sem cópia) — exige camada de I/O sobre descritores; difícil com `fopen`.
(C) `Android/data/<pkg>/files` (pasta específica do app; usuário copia os arquivos).
**Recomendação provisória:** C como primeiro passo (P3), A como UX final (P11).
**Decisão final:** _pendente_

## D-006 · Licenças e linkagem — ABERTA
Várias bibliotecas são LGPL (SDL 1.2/mixer, mpg123, pthreads-win32 só no Windows). Em Android tudo é `.so` ou estático:
definir linkagem dinâmica das LGPL e como publicar avisos/fontes no release. (Não é aconselhamento jurídico.)
**Decisão final:** _pendente_

## D-007 · Versão mínima de GLES — a abrir na P7
GLES 3.0 com fallback 2.0, ou só 3.0. Afeta textura 1D→2D, formatos de profundidade, instancing.

## D-008 · Compilação de shaders para Vulkan — a abrir na P8
glslang/shaderc no build vs. SPIR-V pré-compilado e versionado.

## D-009 · Política de nomes de branch e commits
`android/main` (integração) e `android/p<n>-<tema>` por fase. Commits no formato `android(p<n>): <resumo>`.

## D-010 · Dispositivo de referência e ordem dos renderers — DECIDIDA (2026-11-24)
**Dispositivo de referência:** Samsung Galaxy A06 5G (celular de entrada). Todo critério de desempenho e teste é medido nele primeiro.
Especificações a **confirmar no aparelho** (vindas de memória, não verificadas): SoC MediaTek da faixa Dimensity, GPU Mali, tela HD+,
4–6 GB de RAM. Na P1 rodar um app de diagnóstico e anotar aqui: versão do Android, `ro.product.cpu.abi`, GLES (`glGetString`),
versão do Vulkan e extensões, resolução e taxa de atualização da tela, RAM livre.
**Os 3 renderers serão mantidos:** (1) **software** primeiro (P5), (2) **GLES** (P7), (3) **Vulkan** (P8). O software permanece como
fallback permanente e como referência visual para comparar os outros.
**Implicação de desempenho:** o A06 é fraco; o renderer software deve ser otimizado desde a P5 (NEON, resolução interna do jogo
conservadora, escala feita pela GPU) e medir FPS/tempo de frame antes de seguir para a P6.

## D-011 · Estrutura do build Android e zlib — DECIDIDA (P1)
1. **Um só `CMakeLists.txt`, dois ramos.** Com `ANDROID` definido (toolchain do NDK), o top-level exige `ANDROID_ABI=arm64-v8a`, define
   `MUGEN_PLATFORM_ANDROID`, inclui `cmake/MugenAndroid.cmake` e dá `return()`; o resto do arquivo (alvo `mugen.exe`) só vale para o Windows.
   `MugenSdk.cmake` e `ThirdParty.cmake` continuam exclusivos do Windows. O módulo Gradle aponta para esse mesmo `CMakeLists.txt`.
   O idioma `RC` passou de `project(... C RC)` para `enable_language(RC)` dentro de `if(MSVC)` (necessário porque o NDK não tem `RC`).
2. **P1 compila só as bibliotecas e um `libmugen.so` vazio** (`android/app/src/main/cpp/mugen_android_stub.c`); `MUGEN_SOURCES` entra na P2/P3.
3. **zlib do sistema (NDK `libz`)** em vez do zlib 1.2.3 do original; o header 1.2.3 de `third_party/sdk/include` não entra no caminho Android.
   Risco baixo (API estável); revisar se algum comportamento de `inflate` divergir nos testes diferenciais.
4. **libpng:** `PNG_NO_ASSEMBLER_CODE`/`PNG_NO_MMX_CODE` como definições PUBLIC (o MMX do 1.2.x é só x86; PUBLIC para o `png_struct` do engine bater com o da lib).
5. **Flags:** `-ffp-contract=off` no alvo `mugen` (R2; nunca `-ffast-math`), `-Wl,-z,max-page-size=16384` (páginas de 16 KB), `ANDROID_STL=none` (só C).
