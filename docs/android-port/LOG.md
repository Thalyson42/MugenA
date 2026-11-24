# LOG do port Android

Uma entrada por conversa (modelo em 04-modelos-de-conversa.md). Mais recente no topo.

### 2026-11-24 · P1 · fechamento pela coordenadora
P1 **CONCLUÍDA**. Verificado: CI Android (`assembleDebug`, arm64) verde; `Build` Windows verde no PR; APK instalado e executado no aparelho de teste, relatório em D-010.
Pendente menor (não bloqueia): o app de diagnóstico não mostra o botão "Copy report" no aparelho (o texto aparece e dá para copiar manualmente); corrigir quando mexer no app.
Próxima: P2 — Portabilidade do código. Levar: `src/mugen.h`, `src/game.h`, `src/ftol.h` e a lista de structs com `MUGEN_ASSERT_SIZE`.

### 2026-11-24 · P1 — Build Android · conta (dono do repo) · conversa 1
Estado da fase: EM ANDAMENTO (código escrito; **não validado** no CI nem no aparelho)
Branch/commit: android/p1-build @ e2b42f4 (código) — o commit seguinte só adiciona esta entrada do LOG. **Push não feito** (ver abaixo).
Feito:
- `CMakeLists.txt`: ramo `ANDROID` (exige `arm64-v8a`, define `MUGEN_PLATFORM_ANDROID`, inclui `cmake/MugenAndroid.cmake`, `return()` antes do alvo `mugen.exe`);
  `RC` virou `enable_language(RC)` dentro de `if(MSVC)`.
- `cmake/MugenAndroid.cmake`: lua51, png12, minizip, cgltf, stb_image (de `third_party/`), zlib do NDK, e `libmugen.so` vazio (`android/app/src/main/cpp/mugen_android_stub.c`).
  GLEW e import-libs `.def` ficam fora do caminho Android.
- `CMakePresets.json`: `android-arm64-debug|release` (usa `$env{ANDROID_NDK_HOME}`).
- `android/`: Gradle 8.9 + AGP 8.7.3, `abiFilters arm64-v8a`, `externalNativeBuild` apontando para o `CMakeLists.txt` da raiz; wrapper (jar/gradlew do repo gradle/gradle v8.9.0).
- App de diagnóstico (`MainActivity.kt`): modelo/SoC/Android/ABI, RAM, GLES (`glGetString`), features Vulkan do sistema, tela/Hz, versões das libs do `libmugen.so`; botão "Copy report".
- `.github/workflows/android.yml`: `assembleDebug` + checagem de que o APK só tem `arm64-v8a` + artefato.
- `03-decisoes.md`: D-011 (nova), notas de progresso em D-003 e D-004. D-002 e D-003 **não** foram resolvidas.
Decisões tomadas (ids em 03-decisoes.md): D-011. Notas em D-003/D-004 (continuam abertas/propostas).
Pendente / próximos passos (em ordem):
1. Dar push de `android/p1-build` e abrir PR rascunho; **conferir o CI Windows (`build.yml`) verde** e o `android.yml` verde.
2. Corrigir o que o CI Android acusar (nada foi compilado com o NDK/Gradle até aqui).
3. Instalar o APK no Galaxy A06 5G, copiar o relatório e registrá-lo em D-010 (Android, ABI, GLES, tela, RAM).
4. Vulkan: versão/extensões de dispositivo não são lidas na P1 (só as features do sistema); ler via chamada nativa na P8 ou antes.
5. Fechar D-004 (targetSdk exigido pela Play) antes da P12.
6. P3: spike de D-002 e decisão de D-003 (SDL/mixer/ttf/gfx/libresample/mpg123 ainda fora do build).
Problemas conhecidos / armadilhas:
- Sem NDK, Android SDK, Gradle nem push na conversa: só foi verificado que o ramo ANDROID configura e que lua/libpng/minizip/cgltf/stb compilam com **gcc x86_64 do host**
  (`cmake -DANDROID=ON -DANDROID_ABI=arm64-v8a`, só os 5 alvos de biblioteca). O `libmugen.so`, o JNI, o Kotlin e o Gradle nunca foram compilados.
- O build Windows **não foi rodado**; a mudança de `project(... C RC)` para `enable_language(RC)` em `if(MSVC)` é a que mais merece olho no CI.
- O JNI do stub depende do nome `io.github.thalyson42.mugen.Native` (Kotlin `object` com `@JvmStatic external`); se renomear o pacote, renomear a função C.
- `setEGLContextClientVersion(3)` no app de diagnóstico: em aparelho sem GLES 3 o contexto falha (o A06 5G deve ter).
- `CMakePresets.json` foi reserializado (formatação de blocos antigos mudou no diff, conteúdo igual).
Como validar (comandos exatos / cenários):
- Windows: `tools\msvc_env.bat cmake --preset x86-release` e `cmake --build --preset x86-release` (ou o job `Build`).
- Android: `export ANDROID_NDK_HOME=.../ndk/27.2.12479018 && cmake --preset android-arm64-debug && cmake --build --preset android-arm64-debug` (gera `build/android-arm64-debug/libmugen.so`);
  `cd android && ./gradlew assembleDebug` → `android/app/build/outputs/apk/debug/app-debug.apk`; `adb install -r` no A06 5G e abrir "MUGEN (P1 diag)"; `adb logcat -s mugen mugen-diag`.
- Esperado no relatório: "64-bit pointers", Lua 5.1.4, libpng 1.2.38, ro.product.cpu.abi = arm64-v8a.
Arquivos alterados: CMakeLists.txt, CMakePresets.json, .gitignore, cmake/MugenAndroid.cmake (novo), android/** (novo), .github/workflows/android.yml (novo), docs/android-port/{README,03-decisoes,LOG}.md
Arquivos que a PRÓXIMA conversa precisa receber: README.md e 03-decisoes.md (docs/android-port), esta entrada do LOG.md, o log do CI Android (`android.yml`) e o relatório do aparelho. Para P2: `src/mugen.h`, `src/game.h`, `src/ftol.h` e a lista de structs com `MUGEN_ASSERT_SIZE` (ver 02-fases.md).

### 2026-11-24 · P0 (atualização)
Decididas D-001 (arm64-v8a nativo) e D-010 (A06 5G; ordem software→GLES→Vulkan). Docs atualizados.

### 2026-11-24 · P0 · conversa 1
Estado da fase: EM ANDAMENTO
Feito: análise do repo (commit cc94cfe) e criação de docs/android-port/ (README, 01, 02, 03, 04).
Pendente: confirmar D-001..D-006; criar branch android/main e commitar estes docs.
