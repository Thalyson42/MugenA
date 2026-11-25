# Registro de decisões (ADR) e perguntas em aberto

Formato: `D-nnn` · estado (`ABERTA` / `PROPOSTA` / `DECIDIDA`) · contexto · opções · recomendação · decisão final.
Nunca apague uma decisão: se mudar, crie uma nova que a substitui e marque a antiga como `SUBSTITUÍDA por D-xxx`.

## D-001 · ABI e tamanho de ponteiro — DECIDIDA (2026-11-24)
**Decisão:** alvo primário **`arm64-v8a` (64 bits), execução nativa, sem emulação**. Não haverá build `armeabi-v7a` nem `x86_64`
(emulador) no escopo; o build x86 Windows continua só como referência de comportamento.
**Consequências:** `MUGEN_ASSERT_SIZE` fica condicional (só vale no build Windows 32 bits); os layouts em 64 bits mudam e precisam ser
revisados na P2 (casts ponteiro↔int, ponteiros em campos de 4 bytes, `Trunc64`). Testes diferenciais contra o Windows comparam
**comportamento** (`mugen.log`, saídas), não layout de memória.

## D-002 · Camada SDL no Android — DECIDIDA (2026-11-25, condicional à verificação em execução na P3b)
**Contexto:** o engine usa SDL 1.2 em ~28 arquivos; SDL 1.2 não tem porte Android oficial.
**Opções:** (A) `sdl12-compat` sobre SDL2 (menos mudanças; **verificar** suporte a `SDL_OPENGL`/EGL no Android e a API de
`SDL_SetVideoMode`/superfície de 16 bpp usada pelo renderer software). (B) migrar o código para SDL2 direto (mais trabalho, mais controle,
necessário de qualquer forma para Vulkan via `SDL_Vulkan`). (C) camada própria de plataforma sobre NativeActivity/EGL.
**Recomendação provisória:** decidir na P3 após um spike de 1 dia com (A); se falhar, (B).
**Decisão final:** **(A) `sdl12-compat` sobre SDL2**, com (B) como plano de reserva. Spike estático da P3a (sem NDK nem aparelho; só leitura de código):
- Todas as **44** funções `SDL_*` que `src/*.c` chamam estão em `src/SDL12.exports` do `sdl12-compat` (commit `e5fdbd3`, 2026-11-20); a 45ª, `SDL_main`, é a convenção de entrada do app. `SDL_GetWMInfo`/`SDL_SetModuleHandle`/`SDL_VIDEODRIVER` só aparecem em trechos Win32.
- Foco/segundo plano chegam como `SDL_ACTIVEEVENT` (do `SDL_WINDOWEVENT_FOCUS_*`/`MINIMIZED`/`RESTORED`), que `Events_Poll` já trata (`g_onActivate`). O compat não define perfil de contexto GL, então no Android sai o contexto GLES padrão do SDL2 (coerente com D-007; GL de função fixa não funciona, o que só importa a partir da P7).
- `SDL_GL_SWAP_CONTROL` é traduzido pelo compat.
**Condições para a P3b** (o spike não prova execução): (1) o compat carrega o SDL2 por `dlopen("libSDL2-2.0.so.0")` (caminho `__unix__`); no APK a lib é `libSDL2.so` → **patch de uma linha** em `SDL20_LIBNAME` para `__ANDROID__` (código zlib, vendorizar em `third_party/`); (2) o SDL2 no Android exige o `SDLActivity` (Java) e `SDL_main` em `libmugen.so`; (3) `SDL_SetVideoMode(w,h,16,SDL_HWSURFACE)` tem de dar uma superfície RGB565 utilizável e a apresentação (textura do SDL2) não pode custar mais do que o orçamento de quadro no A065M; o escalonamento 640x480 → 720x1600 precisa de letterbox.
**Reverter para (B)** se (3) falhar, ou se o compat perder o contexto/superfície ao voltar do segundo plano de forma que não dê para contornar. SDL2: versão estável 2.x mais recente a fixar na P3b.

## D-003 · Bibliotecas sem fonte no repo — ABERTA
SDL_mixer, SDL_ttf (+FreeType), SDL_gfx (`zoomSurface`), zlib, libresample, libmpg123. Definir fonte/versão de cada uma
(preferir as de mesma versão do original ou compatíveis) e como entram no build (submódulo, FetchContent ou `third_party/`).
**Decisão final:** _pendente_
**Achado da P3a (sem decidir):** o que o engine chama fora do SDL: SDL_mixer 25 funções, entre elas `Mix_QuerySpecEx` (**não existe no SDL_mixer 1.2 padrão**: o original usa uma versão modificada; ver `sound.c`), SDL_ttf 8 (`TTF_RenderUTF8_*`, `TTF_SizeUTF8`...), SDL_gfx só `zoomSurface` (`font.c`), libresample 6, e nenhuma chamada `mpg123_` direta em `src/*.c`. O SDL_mixer 1.2 compila sobre o `sdl12-compat` (é só API SDL 1.2). Decidir na P3b/P4.
**Progresso na P1 (sem decidir):** entram no build Android, a partir de `third_party/`, Lua 5.1.4, libpng 1.2.38, minizip 0.15, cgltf e stb_image;
zlib vem do NDK (ver D-011). SDL/SDL_mixer/SDL_ttf/SDL_gfx/libresample/libmpg123 **não** entram na P1: dependem de D-002 e há um `TODO(P3)` em `cmake/MugenAndroid.cmake`.

## D-004 · Versões Android — PROPOSTA (verificar contra o aparelho de referência, D-010)
`minSdk` 24 (Vulkan 1.0 disponível; 26+ simplifica áudio/SAF) · `targetSdk` atual da Play · NDK LTS recente · AGP/Gradle estáveis.
**Valores usados na P1 (provisórios, ainda não rodados no CI nem no aparelho):** minSdk 24, compileSdk/targetSdk 35, NDK 27.2.12479018 (r27 LTS),
CMake 3.22.1 (do SDK), AGP 8.7.3, Gradle 8.9, Kotlin 2.0.21, JDK 17. **A revisar na P12:** o `targetSdk` exigido pela Play para publicar
(provavelmente mais alto que 35) e o NDK/AGP mais novos; a P1 só gera APK debug.
**Decisão final:** _pendente_

## D-005 · Onde ficam os dados do jogo — DECIDIDA (2026-11-25): C na P3, A na P11
(A) Pasta escolhida via SAF e cópia para armazenamento interno do app (simples e rápido, duplica espaço).
(B) Acesso direto via SAF (sem cópia) — exige camada de I/O sobre descritores; difícil com `fopen`.
(C) `Android/data/<pkg>/files` (pasta específica do app; usuário copia os arquivos).
**Recomendação provisória:** C como primeiro passo (P3), A como UX final (P11).
**Decisão final:** **(C) agora, (A) na P11; (B) descartada** (`fopen` não funciona sobre SAF). O app chama `getExternalFilesDir(null)` (`/sdcard/Android/data/io.github.thalyson42.mugen/files`, sem permissão em nenhuma versão; cai em `filesDir` se não houver armazenamento externo) e passa o caminho ao nativo (`Native.initPlatform`). `Mugen_SetDataDir` faz `chdir` nele: os caminhos relativos do engine (`data/mugen.cfg`, `chars/...`) e o `mugen.log` ficam ali, como no Windows. O jogo é copiado **direto** nessa pasta (`data`, `chars`, `stages`, `sound`, `font` na raiz dela), por exemplo `adb push <pasta do M.U.G.E.N>/. /sdcard/Android/data/io.github.thalyson42.mugen/files/`. O app de diagnóstico mostra o caminho.
**Verificado no A065M (Android 16):** o app lê e grava nessa pasta sem permissão, e o jogo copiado por `adb push` é achado. O armazenamento emulado **ignora a caixa** por conta própria (o resolvedor só age em sistemas de arquivos case-sensitive, como `filesDir`).
**Risco, não verificado:** a partir do Android 11 apps de terceiros não navegam em `Android/data`; pelo cabo (MTP/adb) funciona, e o gerenciador de arquivos da Samsung costuma conseguir. A cópia via SAF da P11 resolve isso para o usuário final.

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
**Dispositivo de referência:** Samsung **SM-A065M** (celular de entrada; o nome comercial exato fica em aberto, ver as observações abaixo). Todo critério de desempenho e teste é medido nele primeiro.
Especificações a **confirmar no aparelho** (vindas de memória, não verificadas): SoC MediaTek da faixa Dimensity, GPU Mali, tela HD+,
4–6 GB de RAM. Na P1 rodar um app de diagnóstico e anotar aqui: versão do Android, `ro.product.cpu.abi`, GLES (`glGetString`),
versão do Vulkan e extensões, resolução e taxa de atualização da tela, RAM livre.
**Os 3 renderers serão mantidos:** (1) **software** primeiro (P5), (2) **GLES** (P7), (3) **Vulkan** (P8). O software permanece como
fallback permanente e como referência visual para comparar os outros.
**Implicação de desempenho:** o aparelho (família A06) é fraco; o renderer software deve ser otimizado desde a P5 (NEON, resolução interna do jogo
conservadora, escala feita pela GPU) e medir FPS/tempo de frame antes de seguir para a P6.

## D-011 · Estrutura do build Android e zlib — DECIDIDA (P1)
1. **Um só `CMakeLists.txt`, dois ramos.** Com `ANDROID` definido (toolchain do NDK), o top-level exige `ANDROID_ABI=arm64-v8a`, define
   `MUGEN_PLATFORM_ANDROID`, inclui `cmake/MugenAndroid.cmake` e dá `return()`; o resto do arquivo (alvo `mugen.exe`) só vale para o Windows.
   `MugenSdk.cmake` e `ThirdParty.cmake` continuam exclusivos do Windows. O módulo Gradle aponta para esse mesmo `CMakeLists.txt`.
   O idioma `RC` passou de `project(... C RC)` para `enable_language(RC)` dentro de `if(MSVC)` (necessário porque o NDK não tem `RC`).
2. **P1 compila só as bibliotecas e um `libmugen.so` vazio** (`android/app/src/main/cpp/mugen_android_stub.c`); `MUGEN_SOURCES` entra na P2/P3. **P2:** passa a ser compilado como `mugen_core` (ver D-012), ainda sem entrar no `.so`.
3. **zlib do sistema (NDK `libz`)** em vez do zlib 1.2.3 do original; o header 1.2.3 de `third_party/sdk/include` não entra no caminho Android.
   Risco baixo (API estável); revisar se algum comportamento de `inflate` divergir nos testes diferenciais.
4. **libpng:** `PNG_NO_ASSEMBLER_CODE`/`PNG_NO_MMX_CODE` como definições PUBLIC (o MMX do 1.2.x é só x86; PUBLIC para o `png_struct` do engine bater com o da lib).
5. **Flags:** `-ffp-contract=off` no alvo `mugen` (R2; nunca `-ffast-math`), `-Wl,-z,max-page-size=16384` (páginas de 16 KB), `ANDROID_STL=none` (só C).

## D-012 · Portabilidade do código (P2) — DECIDIDA
1. **Camada `src/platform/`.** `platform.h` escolhe `MUGEN_PLATFORM_WIN32` ou `MUGEN_PLATFORM_ANDROID`, define `MUGEN_CDECL`, `MUGEN_STDCALL`, `MUGEN_INT64`,
   `MUGEN_INLINE`, `MUGEN_NORETURN` e inclui `platform_win32.h` (`windows.h`, `mmsystem.h`, `direct.h`) ou `platform_posix.h`. No MSVC cada macro expande para a palavra que
   substituiu (`__cdecl` etc.), então o fluxo de tokens do Windows não muda. Os `.c` não incluem mais `<windows.h>`; `mugen.h` e os 9 `.c` que o incluíam usam `platform/platform.h`.
2. **Shims em vez de reescrever chamadas.** `platform_posix.h/.c` dá, com os nomes Win32/MSVC, só o que o engine usa: `timeGetTime`, `QueryPerformanceCounter/Frequency`, `Sleep`,
   `LoadLibraryA/GetProcAddress/FreeLibrary` (dlopen; os plugins BGM `.dll` não carregam, ver P4), `_stricmp`, `_strlwr`, `_isnan`, `_chdir`, `_getcwd`, `InterlockedExchangeAdd` (`__atomic_fetch_add`),
   `SHGetFolderPathA(CSIDL_FONTS)` (devolve `/system/fonts`) e `GetLocaleInfoA(SISO639LANGNAME)` (lê `LC_ALL`/`LC_MESSAGES`/`LANG`; no Android costuma não achar e cai em "en").
   `_snprintf`/`_vsnprintf` mantêm o contrato do MSVC (sem terminador quando não cabe, `-1` ao truncar). **Provisório:** P3 troca tempo/arquivos/diálogo/log; P4 troca o carregamento de plugins.
3. **Código só-Windows atrás de `MUGEN_PLATFORM_WIN32`**, com versão Android mínima: `sdlmain.c` (o `WinMain`; entra na P3), diálogo de erro de `error.c` (hoje escreve em stderr), `Video_SetFullscreen`
   (só guarda o flag; a janela do Android já é tela cheia), `System_SetWindowIcon` (nada) e o `VirtualFree` de `stream.c` (ramo morto: nada aloca `STREAM_ALLOC_VIRTUAL`).
4. **`MUGEN_ASSERT_SIZE`/`MUGEN_ASSERT_OFFSET` valem só no Windows 32 bits** (D-001); no Android viram `_Static_assert(1, ...)`. Os tamanhos são os do binário de 32 bits; o que se compara depois é comportamento, não layout.
5. **Pointer-sized em vez de `int`.** Campos `int` que guardavam ponteiro viraram `intptr_t` (mesmo tamanho que `int` no x86, então os asserts do Windows continuam valendo): `ScreenLoop.cutscene`, `ScreenLoop.key_table`, `SysElem.text_ptr`.
   Os dois dwords de dados de um comando BGM guardam o ponteiro por `memcpy` (`bgm_data_ptr`); `CharData.quotes` é lido como `char *[]`, não como `int[]`; `DecompBuffer_Get` devolve o ponteiro e um `int *hit`
   (o original empacotava ponteiro + flag em 64 bits, que não cabe com ponteiro de 64 bits). `int64_t` virou `MUGEN_INT64` (no LP64 `int64_t` é `long`, não `long long`); `__time64_t` virou `time_t`.
   Três conversões ponteiro→`int` vêm de valores que o decompilador tirou de registradores (`bgmplugins.c` `size`, `bgmstream.c` `fallback` x2): ficam como `(int)(intptr_t)`, com o mesmo valor-lixo do original.
6. **`Ftol64`/`Trunc64`** (`ftol.h`) com a semântica do `fistp` (truncamento; `0x8000000000000000` para NaN/fora da faixa), escrita explicitamente porque o `fcvtzs` do arm64 satura (R2). Usado nos 3 pontos double→int64 de `sound.c`/`bg.c`.
7. **Flags do `mugen_core`:** `-ffp-contract=off` (R2, nunca `-ffast-math`), **`-fsigned-char`** (o `char` do arm64 é sem sinal e o original é com sinal) e **`-fno-strict-aliasing`** (o MSVC não faz TBAA).
   `-Werror` para `pointer-to-int-cast`, `int-to-pointer-cast`, `implicit-function-declaration` e `incompatible-pointer-types`: os bugs de 64 bits do original viram erro de build, não regressão silenciosa.
8. **`mugen_core`** (`cmake/MugenAndroid.cmake`): biblioteca de objetos com `MUGEN_SOURCES` + `platform_posix.c` + shaders embutidos, ligada às libs da P1 mas **não** ao `libmugen.so` (só `add_dependencies`, para o CI compilá-la). Opção `MUGEN_ANDROID_BUILD_CORE`.
   Para isso o `set(MUGEN_SOURCES ...)` subiu no `CMakeLists.txt` raiz, antes do ramo Android (nada muda para o Windows).
9. **Ainda abertos:** `matchload.c:274-275` usa `sprintf(dest, dir)` (formato não literal, comportamento do original, mantido) e `globals.c` usa um cast de `Stub_ReturnZero` para `BgmPluginDeinitFn` (decompilado com assinatura diferente, mantido).

## D-013 · Stubs temporários só para compilar (P2) — DECIDIDA
Só entram no caminho de include do `mugen_core` (Android); nenhum está no do Windows e todos somem quando a peça real chegar:
- `src/platform/stubs/GL/gl.h`: a API OpenGL 1.1, copiada da seção `GL_VERSION_1_1` do `glew.h` do repo (licença do GLEW), que é o que o `gl.h` do Windows dá ao engine. O resto do GLEW **não** é puxado (o engine declara os próprios ponteiros).
  `GL/glu.h` só declara `gluOrtho2D`. Saem com o renderer: P6 (abstração) e P7 (GLES).
- `src/platform/sdl_config_posix.h`: configuração do SDL 1.2 com base em `<stdint.h>`. Os headers de `third_party/sdk/include/SDL` são do SDK Windows e, fora do Windows, `SDL_config.h` cai em `SDL_config_minimal.h`, que redefine `size_t` e os tipos inteiros e quebra em 64 bits. Sai na P3 com D-002.
- `src/platform/stubs/libresample.h`: redireciona ao header do SDK removendo `__declspec`. Sai quando o libresample for compilado do fonte (D-003).
- `model.c` usa o `glew.h` direto; o único conflito era o tipo de `__glewDeleteRenderbuffersEXT` (`symbols.h`, agora com `const` como no GLEW), que também some um aviso do MSVC.

## D-014 · Plataforma sem SDL: arquivos, tempo, log, erro (P3a) — DECIDIDA
1. **Todo `fopen` do engine passa por `Mugen_fopen`** (`#define fopen` em `platform_posix.h`, depois do `<stdio.h>`; o Windows não muda). Nos outros pontos que recebem caminho há `MUGEN_PATH(p)` (no Windows é `(p)`): `unzOpen` (`select.c`, `stream.c`), `TTF_OpenFont` (`font.c`), `luaL_loadfile` (`luaapi.c`). Não envolvidos ainda (P3b): `remove`, `rename`, `freopen`.
2. **Regras de `platform_path.c`:** `\`→`/`, `//` colapsa, barra final cai; tenta o caminho como está (1 `stat`) e só então resolve componente a componente ignorando caixa **ASCII** (o Windows também iguala outras letras da página de código: nomes com letras não-ASCII que diferem só na caixa não casam). Com duas entradas que diferem só na caixa, vale a grafia exata; senão a menor por `strcmp`. `.` e `..` não são pesquisados; o que não existe fica como escrito (permite criar arquivo em diretório existente com a caixa corrigida). Cache de 64 diretórios, revalidado por inode+mtime+tamanho a cada uso; arquivo criado por `Mugen_fopen` invalida o diretório. Thread-safe (thread de BGM).
3. **Tempo:** `Timer_WaitFrame` no Android dorme até 0,5 ms antes do quadro (`nanosleep`) e gira o resto, em vez de girar até 2 ms por quadro; o cronograma de quadros não muda. `CLOCK_MONOTONIC`. Ao voltar do segundo plano a P3b deve chamar `Timer_Reset`.
4. **Log/erro:** o log de status (`g_log`, não o de partida) também vai ao logcat (tag `mugen`); `Mugen_StdioToLog` manda `printf`/`stderr` ao logcat; `Error_ShowDialog` → `Mugen_ShowError` (logcat + gancho; o app instala um `AlertDialog` que bloqueia a thread do engine até o OK). Textos passam por sanitização para UTF-8 antes do JNI (Shift-JIS derrubaria o CheckJNI).
5. **`libmugen.so`** recebe as mesmas flags de `mugen_core` (`-fsigned-char -ffp-contract=off -fno-strict-aliasing` + erros de ponteiro↔int) por `MUGEN_ANDROID_ENGINE_FLAGS`; o auto-teste verifica `char` com sinal dentro dele. `mugen_platform` (estática) é a plataforma; `mugen_p3a` (estática, **temporária**) são `timer/log/file/textring/util` reais + `mugen_p3a_support.c` (globais e stubs que o resto do engine daria). Na P3b sai `mugen_p3a` e entra `mugen_core`.
6. O CI conta agora 81 objetos de `mugen_core` (a plataforma saiu dele) e roda os testes de host (`tests/platform`).

### D-010 · Relatório do aparelho de teste (colado pelo dono em 2026-11-24, app de diagnóstico da P1)
| Item | Valor |
|---|---|
| Modelo | Samsung **SM-A065M** ("a06"), SoC MediaTek **MT6769V/CZ** |
| Android | 16 (API 36) |
| ABI | `arm64-v8a` (também aceita `armeabi-v7a`, `armeabi`); `libmugen.so` com ponteiros de 64 bits |
| RAM | total 3633 MB; **livre no momento ~1047 MB** → orçamento de memória apertado |
| Tela | 720x1600, 300 dpi, **60 Hz** |
| GPU | ARM **Mali-G52 MC2**, driver OpenGL ES **3.2** (`reqGlEsVersion` 0x30002) |
| GL | `GL_MAX_TEXTURE_SIZE` 16383; tem `GL_EXT_texture_format_BGRA8888`, `GL_EXT_texture_rg`, `GL_EXT_color_buffer_float`, `GL_EXT_buffer_storage`, `GL_ARM_shader_framebuffer_fetch`, ASTC; **sem textura 1D** (como todo ES) |
| Vulkan (feature do sistema) | `vulkan.version` 0x403000 (= 1.3.0 declarado pelo sistema), `vulkan.level` 1, `vulkan.compute` 0 |
| Libs no `libmugen.so` | Lua 5.1.4, libpng 1.2.38, zlib 1.3.0.1-motley |

**Observações / consequências:**
- O relatório diz **SM-A065M**. Pelo que sei (de memória, não verificado), esse código é o Galaxy A06 **4G** (Helio G85 / Mali-G52); o A06 **5G** teria outro SoC e outro código de modelo. O aparelho real de teste é, portanto, o do relatório; a referência de desempenho passa a ser **este aparelho** (SM-A065M).
- GLES 3.2 está disponível: D-007 pode assumir GLES 3.0 como mínimo, com 2.0 só se necessário.
- Vulkan: o sistema declara 1.3, mas a versão real do driver do Mali-G52 e as extensões ainda não foram lidas (fica para a P8 ou antes). `vulkan.compute` = 0 → **não depender de compute shaders** no backend Vulkan.
- 60 Hz casa com o tick de 60 fps do engine; sem necessidade de pacing para taxas altas.
- ~1 GB de RAM livre: caches de sprites/texturas e o buffer do renderer software precisam de limite de memória desde a P5.
