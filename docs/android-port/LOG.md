# LOG do port Android

Uma entrada por conversa (modelo em 04-modelos-de-conversa.md). Mais recente no topo.

### 2026-11-26 · P3b · M2 — engine inteiro ligado no libmugen.so (stubs de D-003) · conversa P3b #2
Estado: **M2: CI Android verde no commit 9fc5123 (run 45781733743; o NDK linkou o `libmugen.so` com o `mugen_core` inteiro) e Build Windows verde (run 45781793354). NÃO verificada em execução**: ninguém ainda tocou "Start engine" no A065M. M3 não começada.
Feito: `libmugen.so` = `mugen_core` (81 arquivos de `src/`) + `mugen_platform` + JNI + `mugen_sdl_probe.c` (agora `Mugen_SdlProbeMain`) + `mugen_d003_stubs.c` (SDL_mixer, SDL_ttf, `zoomSurface`, libresample; D-003) + `mugen_gl_stubs.c` (GL 1.1 e GLEW; D-013); `mugen_p3a` e `mugen_p3a_support.c` removidos; o `SDL_main` exportado é o do engine (`src/sdlmain.c`), que roda a sonda M1b quando o app pede (`Mugen_SetStartMode`, JNI `Native.setStartMode`); app: botão **"Start engine (P3b M2)"**, e o relatório mostra o fim de `mugen.log` ao voltar; `MugenActivity` registra-se em `Native.activity` para o diálogo de erro aparecer por cima. Opção `MUGEN_ANDROID_BUILD_CORE` removida (o core é sempre construído).
Verificado aqui: fecho de símbolos no host (todos os objetos + stubs + `libSDL.so` do compat, `--no-undefined`): só faltam lua/png/minizip/cgltf/stb (libs estáticas) e `Mugen_SdlProbeMain` (arquivo Android); todas as funções `SDL_*` que o engine chama existem no compat; sem símbolo duplicado. No CI: link ok no NDK. **Não verificado:** carga do `libmugen.so` no aparelho (símbolos indefinidos só aparecem em `dlopen`), o boot do engine, o que o engine faz com `Sound_Init`/fontes/GL stubs, `RENDER_SYSTEM` por padrão (confirmar em `mugen.cfg`: `Video.RenderMode`).
Decisões: D-003 (decisão PARCIAL: stubs temporários até a P4; decisão final de fontes/versões segue pendente).
Pendente / próximos passos: (1) dono: instalar o APK do run 45781733743, tocar **"Start engine (P3b M2)"**; ao voltar ao app o relatório mostra o fim de `mugen.log`; mandar esse texto, o `adb logcat -d -s mugen mugen-sdl SDL SDL/APP AndroidRuntime DEBUG` se puder, e dizer o que apareceu na tela e se o app fechou; se o app **fechar ao abrir** (antes de tocar em qualquer coisa), é o `dlopen` do `libmugen.so`: mandar o logcat (`UnsatisfiedLinkError` diz o símbolo); (2) conforme o resultado, M3 (ciclo de vida: `SDL_ACTIVEEVENT` -> `g_onActivate`/`Events_Poll`, `Timer_Reset`; pausa real com Home) e correção do que o boot mostrar (sem `chars/kfm`, sem áudio e sem TrueType: o engine pode abortar em algum ponto previsível).
Problemas conhecidos / armadilhas: (a) o engine chama `exit(` em caminhos de erro: mataria o processo do app (conferir no log); (b) sem som e sem fontes TrueType (stubs); (c) `SHGetFolderPathA` do `system.c` na plataforma POSIX: já cobre o P3a (conferir `g_systemFontDir`); (d) a sonda do botão antigo continua disponível ("Run SDL 1.2 probe").
Como validar: `cd android && ./gradlew assembleDebug`; instalar por cima (assinatura fixa, D-016); botão "Start engine"; `adb logcat -s mugen mugen-sdl SDL`; o Windows por `workflow_dispatch`.
Arquivos alterados: android/app/src/main/cpp/{mugen_d003_stubs.c,mugen_gl_stubs.c (novos),mugen_p3a_support.c (removido),mugen_sdl_probe.c,mugen_selftest.c,mugen_android_stub.c}, android/app/src/main/java/.../{Native,MainActivity,MugenActivity}.kt, cmake/MugenAndroid.cmake, src/{sdlmain.c,platform/platform_app.c,platform/platform_posix.h}, .github/workflows/android.yml, docs/android-port/{README,03-decisoes,LOG}.md
Arquivos que a PRÓXIMA conversa (M3) precisa receber: README.md, 03-decisoes.md (D-003, D-014, D-015), esta entrada, o texto do `mugen.log` e o logcat do boot, e do código `src/sdlmain.c`, `src/events.c`, `src/system.c` (`System_OnActivate`, `g_onActivate`), `src/timer.c`, `src/main.c`.

### 2026-11-26 · P3b · M1b FECHADA (visual ok) + M1c — engine com os headers do sdl12-compat · conversa P3b #2
Estado: **M1b CONCLUÍDA** (dono: "visual ok", captura de tela: moldura laranja, 8 barras, degradê azul→vermelho, cantos, barra branca, imagem inteira com faixas pretas; observações menores da sonda: o degradê vai de azul à esquerda para vermelho à direita, e o quadrado azul do canto inferior esquerdo some sobre o azul; defeitos do desenho de teste, não do SDL). **M1c CONCLUÍDA (só compilação, sem execução): CI Android verde no commit 7ab414e (run 45753414670, com os testes de host e a contagem dos 81 objetos de `mugen_core`) e `Build` Windows verde no 1ab7835 (run 45753384568, que já tinha as mudanças de `main.c`/`system.c`; o commit seguinte só mexe em `tests/platform`).** O primeiro run do Android (1ab7835) falhou porque `tests/platform/CMakeLists.txt` ainda usava `sdk/include/SDL` sem o `sdl_config_posix.h`; corrigido no 7ab414e (include do sdl12-compat + `SDL_ext`).
Feito (D-015 item 8): `MUGEN_ANDROID_ENGINE_INCLUDES` com `sdl12-compat/include/SDL` no lugar de `sdk/include/SDL`; `src/platform/stubs/SDL_ext/` (`SDL_mixer.h`, `SDL_ttf.h`, cópias); `sdl_config_posix.h` apagado (D-013); `main.c` -> `Mugen_EngineMain` no Android; `sdlmain.c` (ramo Android) = `SDL_main` do engine; `system.c`: `SDL_syswm.h` só no Win32.
Verificado aqui: os 81 `src/*.c` passam em `clang -fsyntax-only` (alvo x86_64 LP64, flags e `-Werror` do CMake, headers do sdl12-compat, shaders embutidos gerados com `cmake -P EmbedShader`); `video.c`, `sdlmain.c`, `system.c`, `events.c`, `main.c` sem avisos novos com `-Wall`. **Não verificado:** compilação com o NDK/bionic (CI), qualquer execução do `SDL_main` do engine, e o Windows (só o ramo Android e um `#include` atrás de `MUGEN_PLATFORM_WIN32` mudaram).
Decisões: D-015 item 8; D-013 (item do `sdl_config_posix.h` marcado como removido).
Pendente: **M2** = D-003 (decidir fonte/versão e incluir SDL_mixer incl. `Mix_QuerySpecEx` do SDL_mixer modificado do original, SDL_ttf + FreeType, SDL_gfx `zoomSurface`, libresample, libmpg123 ou stubs) + ligar `mugen_core` no `libmugen.so`, remover `mugen_p3a` e `mugen_p3a_support.c`, e trocar a sonda pelo `SDL_main` de `sdlmain.c` (o app de diagnóstico passa a iniciar o engine). Depois M3 (ciclo de vida: `SDL_ACTIVEEVENT` -> `Events_Poll`/`g_onActivate`, `Timer_Reset`) e M4 (verificação em execução; também pausa real com Home).
Problemas conhecidos / armadilhas: (1) `exit(` no engine mataria o processo do app; conferir na M2 (`grep -n "exit(" src/*.c`); (2) a flag `SDL_HWSURFACE` não volta na superfície do compat (ver M1b); (3) `RENDER_OPENGL*` pede `SDL_OPENGL` e GL de função fixa: só a P7; o primeiro boot (P5) usa `RENDER_SYSTEM`.
Como validar: igual à M1b (CI Android) e o Windows por `workflow_dispatch`; local: o laço de `clang -fsyntax-only` acima (ver `tools/`? não há script; comando no D-015 item 8).
Arquivos alterados: src/{main.c,sdlmain.c,system.c}, src/platform/{platform_posix.h,sdl_config_posix.h (removido),stubs/SDL_ext/*}, cmake/MugenAndroid.cmake, docs/android-port/{README,03-decisoes,LOG}.md
Arquivos que a PRÓXIMA conversa (M2) precisa receber: README.md, 03-decisoes.md (D-003, D-006, D-015), esta entrada, `cmake/MugenAndroid.cmake`, `src/sound.c`, `src/font.c`, `src/bgm*.c`, `src/wav.c`, `third_party/sdk/include/libresample.h`, `third_party/sdk/def/SDL_mixer.def`.

### 2026-11-26 · P3b · M1b verificada no A065M (relatório do app + logcat do dono)
Estado: **M1b: formato PASS no aparelho** (2 execuções, 0 falhas). Falta a **confirmação visual** do quadro (o dono ainda não disse o que viu) para fechar a M1b e liberar a M1c. APK do run 45750024935 (4e554af); a assinatura fixa já foi usada (instalação por cima: o dono reinstalou o jogo, `data/mugen.cfg`, `data/fight.def`, `sound` agora achados no auto-teste).
Dados (detalhes e decisão em D-002, "Estado na P3b"): 640x480 16 bpp RGB565, `SDL_FillRect`/`SDL_UpperBlit`/alpha/`SDL_Flip` ok, 2ª `SDL_SetVideoMode` ok; driver `Android`; janela passa a **paisagem 1600x720 sozinha** (SDLActivity pede `SENSOR_LANDSCAPE` para janela 640x480; `VIDEORESIZE 1600x720`); `SDL_Flip` 5,6 ms (sem pacing) / 7,0-7,7 ms (60 fps), desenho do quadro de teste 3,5-5,4 ms, 27/720 quadros >20 ms; `BUDGET` OK (11,92) numa execução e OVER (13,11) na outra, limite de 12 ms da sonda (critério da sonda, não do engine).
Esclarecido: o `INFO: pixel format wanted RGB24, got RGBX8888` (logcat, tag `mugen` nível E) é a mensagem do próprio SDL2 Android (`SDL/APP`, tag à parte) que o `Mugen_StdioToLog` capturou do stderr: **informativo**, não é erro do engine.
Não testado ainda: **ir de fato ao segundo plano e voltar** (nesta execução só houve perda/ganho de foco de entrada, `ACTIVEEVENT gain=0/1 state=0x2`, sem `APPACTIVE`, sem pausa da thread; foi assim que a tela perdeu o foco por ~5 s e o loop seguiu). A sonda sai por conta própria depois dos 15 s; o teste de pausa real é da M3 (e pode ser repetido com a sonda: tocar Home durante os 15 s).
Decisões: D-002 atualizada (condição 3: formato ok; custo de apresentação medido; P5 apresenta por textura RGB565 direta). Nenhuma nova.
Próxima: dono responde **o que viu na tela** (8 barras de cor, degradê vermelho→azul, moldura laranja, quadrados nos 4 cantos vermelho/verde/azul/amarelo, barra branca andando, imagem 640x480 inteira com faixas pretas, suave?). Se estiver certo: **M1c** (`video.c`, `sdlmain.c`, `system.c`, `events.c` com os headers do sdl12-compat; trocar `sdk/include/SDL` e `sdl_config_posix.h` de D-013; `SDL_main` do engine no lugar da sonda). Se a imagem estiver errada: parar e decidir D-002 (B).

### 2026-11-26 · P3b · correção do passo de certificado no CI (conversa P3b #2, continuação)
O run do CI no commit 1a4baa6 (log do dono): `assembleDebug` **verde** (0 falha de compilação; inclui `mugen_sdl12_probe.c` e o novo link só com `libSDL.so`), APK assinado com o certificado fixo: **SHA-256 `31fdc9f14e1142b1eb23478df92767faae95bfa145f0dd0d3121d99ce6a396ba`** (= D-016). O passo "Print the APK signing certificate" **falhou por bug do meu script**: `keytool` imprime `SHA256:` precedido de TAB e o `sed` só aceitava espaços (`want` vazio). Corrigido com `[[:space:]]*` (testado: bate). Por causa dessa falha os passos seguintes (inclusive "Check the SDL stack ... M1b") **não rodaram**: continuam sem verificação; o novo run diz.
**Verificado em 4e554af (push feito pela própria conversa, com acesso de escrita concedido via `add_repo`):** CI Android **verde** (run 45750024935): certificado do APK = do keystore (`31fdc9f1...96ba`); passo "Check the SDL stack (M1a/M1b)" ok: `libmugen.so` tem `NEEDED libSDL.so`, **não** tem `NEEDED libSDL2`, exporta `T SDL_main`, tem `U SDL_SetVideoMode`/`SDL_Init`/`SDL_Flip`/`SDL_UpperBlit`/`SDL_MapRGB`/`SDL_PollEvent`, e `libSDL.so` define `SDL_SetVideoMode`/`SDL_Flip`. `Build` Windows **verde** (run 45750274386, `workflow_dispatch`; o workflow `Build` só roda em PR/main/dispatch). **Falta só o aparelho** (passos 2-4 do handoff acima); o APK do artefato do run 45750024935 serve.

### 2026-11-26 · P3b · Passo 0 (assinatura fixa) + M1b (teste SDL 1.2) · conversa P3b #2
Estado: P3b EM ANDAMENTO. **Passo 0: pronto, falta o CI. M1b: código pronto e lógica validada no host; resultado no aparelho PENDENTE; M1c não começada (só segue se a M1b passar no A065M).**
Branch/commit: `android/p3b-sdl` a partir de 14c9db3; commits `android(p3b): ...` em cima (`git log 14c9db3..android/p3b-sdl`). **Sem push** (a conversa não tem credencial): entregue como `git bundle create p3b-m1b.bundle android/p3b-sdl` (base 14c9db3; `git fetch p3b-m1b.bundle android/p3b-sdl:android/p3b-sdl`).
Feito:
- **Passo 0 (D-016):** `android/debug.keystore` (PKCS12, alias `androiddebugkey`, senha `android`, RSA 2048, 100 anos) + `signingConfigs.debug` em `android/app/build.gradle.kts`; o CI roda `apksigner verify --print-certs`, imprime o SHA-256 e **falha** se não for o do keystore. SHA-256 esperado: `31:FD:C9:F1:4E:11:42:B1:EB:23:47:8D:F9:27:67:FA:AE:95:BF:A1:45:F0:DD:0D:31:21:D9:9C:E6:A3:96:BA`. Release (P12): chave própria em GitHub Secrets, nunca no repo.
- **M1b:** `mugen_sdl12_probe.c/.h` (objeto `mugen_sdl12_probe`, só os headers do sdl12-compat): `SDL_SetVideoMode(640,480,16,SDL_HWSURFACE)`; confere 640x480, 16 bpp/2 bytes, máscaras `0xF800/0x07E0/0x001F`, ponteiro e pitch; `SDL_MapRGB` (RGB565), escrita direta, `SDL_FillRect`, `SDL_UpperBlit` de `SDL_CreateRGBSurface` 16 bpp com e sem `SDL_SRCALPHA`; desenha o quadro de teste direto nos pixels e apresenta com `SDL_Flip` por 15 s de primeiro plano (3 s sem pacing para medir o custo de um `SDL_Flip`, depois 12 s a 60 fps com quadros atrasados contados); registra eventos `SDL_ACTIVEEVENT`/`SDL_QUIT`/mouse (toque chega como mouse); depois da pausa confere que a superfície continua a mesma, 16 bpp; chama `SDL_SetVideoMode` de novo com `HWSURFACE|DOUBLEBUF` (como o `video.c`). Linhas `ok`/`FAIL`, `BUDGET: ... OK|OVER` (draw + flip <= 12 ms de 16,7) e `M1b RESULT: PASS|FAIL` em `sdl-probe.txt` e no logcat `mugen-sdl`.
- **Achado que mudou o desenho (D-015 item 5b):** `libSDL2.so` e `libSDL.so` exportam os mesmos nomes `SDL_*`; com `libSDL2.so` no `NEEDED` de `libmugen.so` (M1a) o código SDL 1.2 ligaria no SDL 2. `libmugen.so` agora liga **só `libSDL.so`**; a fase SDL 2 da sonda saiu (já verificada na M1a). `mugen_sdl_probe.c` virou só o `SDL_main` (sem header SDL; pasta de dados por `dlsym` no `libSDL2.so` já carregado). CI: confere `NEEDED libSDL.so`, ausência de `NEEDED libSDL2`, `U SDL_Init/SetVideoMode/Flip/UpperBlit/MapRGB/PollEvent` em `libmugen.so` e `T SDL_SetVideoMode/Flip` em `libSDL.so`.
- App: botão "Run SDL 1.2 probe (P3b M1b, 15 s)".
Verificado aqui (sem NDK/aparelho): a sonda compila com `gcc -Wall -Wextra` contra os headers do sdl12-compat e **roda** no host (SDL2 podado + `SDL12_compat.c` do repo, `SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software`): `M1b RESULT: PASS (0 failed checks)`, 16 bpp RGB565, blit opaco `0xf800`, alpha 128 `0x7800`, 60,5 fps pacing. Os tempos do host não valem para o Mali. **Não verificado:** compilação com o NDK, carga no aparelho, o formato real da textura do renderer GLES2 (o compat converte 16 bpp para o formato da textura a cada `SDL_Flip`, é o custo a medir), quadro visível e letterbox, pausa/retomada pela API 1.2, que o `libSDL.so` não consiga ligar `SDL_*` do SDL 2 por engano. Windows: não tocado (só `android/`, `cmake/MugenAndroid.cmake`, `.github/workflows/android.yml`, `third_party` intacto, docs).
Decisões tomadas (ids em 03-decisoes.md): D-016 (nova); D-015 item 5b (emenda); D-002 (estado da condição 3).
Pendente / próximos passos (em ordem):
1. Dono: aplicar o bundle, push de `android/p3b-sdl`, e mandar do **CI Android**: o passo "Print the APK signing certificate" (SHA-256; deve bater com o de D-016 e ser igual em todo run) e o passo "Check the SDL stack in the APK (P3b M1a/M1b)"; e o resultado do **Build Windows**.
2. Dono: **desinstalar o APK antigo uma vez** (a chave mudou; apaga `Android/data/<pacote>`: antes, `adb pull /sdcard/Android/data/io.github.thalyson42.mugen/files` guarda o jogo), instalar o novo e **refazer o `adb push` do jogo** (conferir `data/mugen.cfg` e `data/fight.def`, que faltaram no último relatório). Dali em diante instalar por cima funciona.
3. Dono: tocar "Run SDL 1.2 probe", deixar os 15 s, **na 2a vez ir ao segundo plano e voltar**; colar o relatório do app (`sdl-probe.txt`) e `adb logcat -d -s mugen-sdl mugen SDL SDL/APP` do período; dizer **o que viu na tela** (8 barras de cor em cima, degradê vermelho→azul embaixo, moldura laranja, quadrados nos cantos vermelho/verde/azul/amarelo, barra branca andando; a imagem 640x480 inteira, com faixas pretas) e se a barra andou suave.
4. Coordenadora: se `M1b RESULT: PASS` e `BUDGET: ... OK` e a imagem estiver certa, seguir **M1c** (`video.c`, `sdlmain.c`, `system.c`, `events.c` com os headers do sdl12-compat, `sdlmain.c` com `SDL_main`; trocar `sdk/include/SDL` e `sdl_config_posix.h` de D-013 por `third_party/sdl12-compat/include/SDL`). Se `FAIL` ou `BUDGET: OVER`: parar e decidir D-002 (B) (ou otimizar a apresentação: a conversão 16->32 bpp no compat, textura em RGB565 direta).
Problemas conhecidos / armadilhas:
- A superfície que o compat devolve **não tem a flag `SDL_HWSURFACE`** (`flags 0x01000000`, PREALLOC) e `hw_available=0`: o `video.c` tolera (tenta de novo sem `HWSURFACE` só se der NULL), mas qualquer teste de `flags & SDL_HWSURFACE` no engine dará falso.
- `SDL_PumpEvents` só trabalha na thread do `SDL_SetVideoMode` (a do `SDL_main`); o engine tem de manter vídeo e eventos nela.
- Sem `SDL_VIDEO_VSYNC` no ambiente o compat não pede vsync ao renderer: o pacing é o do engine (`Timer_WaitFrame`) mais o que o EGL impuser; a sonda mede os dois.
- O compat só desenha pela lista de textura do renderer (`texture_formats[0]`); se for 32 bpp, cada `SDL_Flip` converte 640x480 de 16 para 32 bpp na CPU.
- A janela continua em retrato 720x1600 (orientação é da M4); o compat faz letterbox com `SDL_RenderSetLogicalSize`.
- `SDL_Quit` do 1.2 é chamado no fim do teste; depois disso `SDL_main` retorna e o `SDLActivity` encerra.
Como validar:
- Host: `cmake -S third_party/SDL2 -B /tmp/b2 -G Ninja -DSDL_TEST=OFF -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF -DSDL_ALSA=OFF -DSDL_JACK=OFF -DSDL_SNDIO=OFF -DSDL_HIDAPI=OFF -DSDL_OPENGL=OFF -DSDL_OPENGLES=OFF -DSDL_VULKAN=OFF -DSDL_STATIC=OFF -DSDL_CCACHE=OFF && cmake --build /tmp/b2`; `gcc -shared -fPIC -w -D_REENTRANT -Ithird_party/SDL2/include third_party/sdl12-compat/src/SDL12_compat.c -o /tmp/h/libSDL.so -ldl`; um `main` que chama `Mugen_Sdl12ModeTest(puts-like, 5)`; `gcc -I third_party/sdl12-compat/include/SDL -I android/app/src/main/cpp mugen_sdl12_probe.c main.c -L/tmp/h -lSDL`; `LD_LIBRARY_PATH=/tmp/b2:/tmp/h SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software ./probe 5`.
- Android: `cd android && ./gradlew assembleDebug`; `adb install -r app/build/outputs/apk/debug/app-debug.apk`; botão "Run SDL 1.2 probe"; `adb logcat -s mugen-sdl mugen`. Windows: `tools\msvc_env.bat cmake --preset x86-release` e `cmake --build --preset x86-release` (não deve mudar nada).
Arquivos alterados: android/debug.keystore (novo), android/app/build.gradle.kts, android/app/src/main/cpp/{mugen_sdl_probe.c,mugen_sdl12_probe.c,mugen_sdl12_probe.h}, android/app/src/main/java/.../MainActivity.kt, cmake/MugenAndroid.cmake, .github/workflows/android.yml, docs/android-port/{README,03-decisoes,LOG}.md
Arquivos que a PRÓXIMA conversa (M1c) precisa receber: README.md, 03-decisoes.md (D-002, D-013, D-015, D-016), esta entrada, o relatório `sdl-probe.txt` e o logcat `mugen-sdl`, o log do CI Android (passos do SHA-256 e do SDL); do código `cmake/MugenAndroid.cmake`, `android/app/src/main/cpp/mugen_sdl12_probe.c`, e para a M1c `src/video.c`, `src/sdlmain.c`, `src/system.c`, `src/events.c`, `src/platform/sdl_config_posix.h`.

### 2026-11-26 · P3b · M1a verificada pela coordenadora
Estado: **M1a CONCLUÍDA**. P3b continua EM ANDAMENTO (M1b, M2, M3, M4 pendentes). Branch `android/p3b-sdl` (continua aberta; PR para `android/main` só no fim da P3b).
Verificado: CI Android e `Build` Windows **verdes no commit 567cf4a** (runs 45727880642 e 45728418079): passo "Check the SDL stack in the APK" ok (`libSDL2.so`/`libSDL.so`/`libmugen.so`, `SDL_main` exportado, `libSDL.so` não liga `libSDL2.so`, sem o nome `libSDL2-2.0.so.0`), 0 avisos no `assembleDebug`, Windows 153/153, testes de host 100%.
No A065M (logcat `mugen`/`mugen-sdl`): auto-teste da P3a `19 checks, 0 failed` (pasta de dados case-insensitive, cache do app case-sensitive; `Timer_WaitFrame` 0 atrasados, 2% de CPU da thread; overshoot de `nanosleep(2 ms)` 58-302 us).
SDL probe: `SDL_main entered (SDL2 2.32.10, platform Android)`, driver `Android`, display 720x1600 @ 60 Hz, janela 720x1600, renderer `opengles2` (max texture 16383), **1203 quadros em 20019 ms = 16,6 ms/quadro**. Ciclo de vida chega na ordem certa: `FOCUS_LOST` -> `MINIMIZED` -> `APP_WILLENTERBACKGROUND` -> `APP_DIDENTERBACKGROUND`; na volta `WILLENTERFOREGROUND` -> `DIDENTERFOREGROUND` -> `RESTORED` -> `FOCUS_GAINED`. Mesmo PID durante os ciclos (app não morreu). A 2a execução não chegou a `probe time is over` porque o tempo em primeiro plano somou ~15 s (< 20 s) antes do fim da captura: não é falha.
Pontos para a M1b/M4:
- O formato do display aparece como `RGB565` na 1a execução e `RGBX8888` na 2a; o log tem `INFO: pixel format wanted RGB24, got RGBX8888` (tag `mugen`, nível E, thread diferente) a cada retomada; provavelmente informativo, **não confirmado** quem emite. Conferir que `SDL_SetVideoMode(...,16,...)` entrega uma superfície 16 bpp pela API 1.2 (condição de D-002).
- A janela sai em **retrato** (720x1600); o jogo é paisagem. Decidir a orientação na M4.
- No último auto-teste, `data\mugen.cfg` e `data\fight.def` deram **not found** (antes, em 25/11 16:06, tinham sido achados); `chars`, `stages` e `font` foram achados. Provável causa: reinstalar o APK apaga `Android/data/<pacote>` e o jogo foi copiado de novo só em parte. **Conferir** com `adb shell ls /sdcard/Android/data/io.github.thalyson42.mugen/files/data` e refazer o `adb push` se faltar; a M2/M3 precisa de `data/mugen.cfg` e `data/fight.def`.
Próxima: M1b (ver 02-fases/LOG da M1a): `SDL_SetVideoMode(640,480,16,SDL_HWSURFACE)` com um quadro visível pela API SDL 1.2; se falhar, parar e decidir D-002 (B).

### 2026-11-25 · P3b · M1a — SDL2 + sdl12-compat vendorizados, SDLActivity e SDL_main · conversa P3b #1
Estado da fase: P3b EM ANDAMENTO. **M1a: código pronto, CI Android e Build Windows ainda não rodaram; aparelho não testado.** M1b não foi começada (depende do resultado da M1a no aparelho).
Branch/commit: android/p3b-sdl (a partir de android/main @ a402e5b), 3 commits `android(p3b): ...` (vendorização, integração, docs; `git log android/main..android/p3b-sdl`). **Sem push** (a conversa não tem credencial): entregue como `git format-patch` (3 arquivos; `git am`; o primeiro tem ~12 MB por causa do SDL2).
Feito:
- `third_party/SDL2` = release-2.32.10 (`5d24957`), `third_party/sdl12-compat` = `e5fdbd3`; só `src/`, `include/`, `cmake/`, `CMakeLists.txt` e os Java do SDLActivity (SDL2); poda e patches em cada `README-mugen.md`. Patches: `libSDL2.so` em `SDL12_compat.c` (`__ANDROID__`); `mHIDDeviceManager = null` em `SDLActivity.java` (SDL2 sem HIDAPI, para não precisar de C++). Versões e decisões em **D-015**.
- `cmake/MugenAndroid.cmake`: `add_subdirectory(SDL2)` (→ `libSDL2.so`), alvo `SDL12` (→ `libSDL.so`), `mugen_sdl_probe` (objeto com `SDL_main`, único a ver os headers do SDL2), `libmugen.so` liga os dois com `LINK_ONLY` (nenhum header SDL entra nos `.c` do engine).
- App: `MugenActivity : SDLActivity` (libs `SDL2`, `SDL`, `mugen`), entrada no manifest, `sourceSets` do Gradle para os Java do SDL2, botão "Run SDL probe (P3b, 20 s)" no `MainActivity`, que mostra `sdl-probe.txt` ao voltar.
- `mugen_sdl_probe.c` (`SDL_main`, API SDL2): abre janela fullscreen + renderer com vsync, pinta cor/barra por 20 s, registra no logcat (tag `mugen-sdl`) versão, driver de vídeo, modo do display, tamanho da janela, renderer, eventos de ciclo de vida (`APP_*`, `WINDOWEVENT`, `RENDER_*_RESET`) e ms/quadro; escreve o mesmo em `<pasta de dados>/sdl-probe.txt`.
- CI (`android.yml`): novo passo confere no APK `libSDL2.so`/`libSDL.so`/`libmugen.so`, as classes `SDLActivity` e `MugenActivity` no dex, `SDL_main` exportado, `NEEDED` de `libmugen.so`, que `libSDL.so` **não** liga `libSDL2.so` e que tem o nome `libSDL2.so` (e não `libSDL2-2.0.so.0`).
Verificado aqui (sem NDK/aparelho): o SDL2 podado configura e compila inteiro no Linux (cmake 4.4/ninja, gcc; sem X11/Wayland/áudio de sistema, só para provar que a poda não quebrou referências do CMake); `SDL12_compat.c` compila no gcc e, com `-D__ANDROID__`, o objeto contém `libSDL2.so` e não o nome antigo; `mugen_sdl_probe.c` passa em `gcc -fsyntax-only -Wall -Wextra -D__ANDROID__` contra os headers do SDL2. **Não verificado:** SDL2 com o NDK r27, Kotlin/Gradle/manifest, carga das libs no aparelho, janela. O Build Windows não é tocado (só `third_party/SDL2|sdl12-compat` novos, `android/`, `cmake/MugenAndroid.cmake`, `.github/workflows/android.yml`, docs).
Decisões tomadas (ids em 03-decisoes.md): D-015.
Pendente / próximos passos (em ordem):
1. Dono: aplicar os patches, dar push de `android/p3b-sdl` e mandar o resultado do **CI Android** (log do passo `assembleDebug` e do passo "Check the SDL stack in the APK") e do **Build Windows**. Pontos prováveis de falha: SDL2 + NDK (`cpu-features.c`, `-marm`), `getLibraries` em Kotlin, `compileSdk`/Java do SDL2.
2. Dono: instalar o APK no A065M, tocar "Run SDL probe (P3b, 20 s)", **deixar rodar os 20 s e na segunda vez ir ao segundo plano e voltar**; colar o relatório do app (aparece ao voltar) e, se possível, `adb logcat -d -s mugen-sdl mugen SDL SDL/APP` do período. Esperado: `SDL_main entered`, `video driver: android`, `renderer: opengles2` (ou outro), janela 720x1600 (ou o tamanho do display), `presented N frames ... ms/frame` perto de 16,7, `SDL_main done`, e o app de diagnóstico de volta.
3. Só então M1b (`SDL_SetVideoMode(640,480,16,SDL_HWSURFACE)` com quadro visível pela API 1.2, no mesmo `SDL_main`/arquivo de teste de outro objeto que veja os headers do SDL 1.2). Se M1b falhar: parar e decidir a reversão para (B) (D-002).
Problemas conhecidos / armadilhas:
- `libSDL.so` tem construtor que **aborta** o processo se não achar `libSDL2.so`; isso também derruba o app de diagnóstico (que carrega `libmugen.so`, que depende de `libSDL.so`). Se o app fechar na hora ao abrir, é isso: logcat `mugen-sdl`/`DEBUG`.
- O patch de `SDLActivity.java` (HID) precisa ser reaplicado ao atualizar o SDL2.
- O relatório antigo `sdl-probe.txt` é apagado ao tocar o botão; se o probe morrer antes de escrever, o app diz isso.
- Desinstalar o app apaga `Android/data/<pacote>` (e o jogo copiado).
- A tela é fullscreen sem `screenOrientation` fixo; a orientação segue o sensor (a decisão de retrato/paisagem fica para a M4).
Como validar:
- Host (poda do SDL2): `cmake -S third_party/SDL2 -B /tmp/b2 -G Ninja -DSDL_TEST=OFF -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_KMSDRM=OFF -DSDL_PULSEAUDIO=OFF -DSDL_PIPEWIRE=OFF -DSDL_ALSA=OFF -DSDL_JACK=OFF -DSDL_SNDIO=OFF -DSDL_HIDAPI=OFF -DSDL_OPENGL=OFF -DSDL_OPENGLES=OFF -DSDL_VULKAN=OFF -DSDL_STATIC=OFF -DSDL_CCACHE=OFF && cmake --build /tmp/b2`.
- Android: `cd android && ./gradlew assembleDebug`; `adb install -r app/build/outputs/apk/debug/app-debug.apk`; botão "Run SDL probe"; `adb logcat -s mugen-sdl mugen`.
- Windows: `tools\msvc_env.bat cmake --preset x86-release` e `cmake --build --preset x86-release` (não deve mudar nada).
Arquivos alterados: third_party/SDL2/** e third_party/sdl12-compat/** (novos), cmake/MugenAndroid.cmake, android/app/build.gradle.kts, android/app/src/main/{AndroidManifest.xml,cpp/mugen_sdl_probe.c,java/.../MainActivity.kt,java/.../MugenActivity.kt}, .github/workflows/android.yml, docs/android-port/{README,03-decisoes,LOG}.md
Arquivos que a PRÓXIMA conversa (M1b) precisa receber: README.md, 03-decisoes.md (D-002, D-015), esta entrada, o log do CI Android e o relatório do app (`sdl-probe.txt`); do código, `android/app/src/main/cpp/mugen_sdl_probe.c` e `cmake/MugenAndroid.cmake`.

### 2026-11-25 · P3a · verificação no CI e no A065M (conversa P3 #1, continuação)
Estado da fase: P3a **CONCLUÍDA** (auto-teste corrigido confirmado no A065M, ver "Fechamento" abaixo).
Branch/commit: android/p3-plataforma @ 8c133fb (push pelo dono via `git am`) + commit de correção desta entrada.
Verificado:
- CI: `android.yml` verde (`assembleDebug` 1m35; testes de host passam no runner; `mugen_core` = 81 objetos; APK só `arm64-v8a`; `libmugen.so` exporta `initPlatform`/`platformSelfTest`/`testError`). `Build` Windows verde (153/153).
- A065M (Android 16): app abre, diretório de dados `/storage/emulated/0/Android/data/io.github.thalyson42.mugen/files`, `mugen.log` criado pelo `Log_Init` real, `char` com sinal no `libmugen.so`, `File_Find`/`File_Exists` OK. Com o M.U.G.E.N copiado por `adb push`: `data\mugen.cfg`, `data\fight.def`, `chars`, `stages`, `font` achados. O botão "Copy report" apareceu (o `fitsSystemWindows` resolveu) e o diálogo de erro funciona (bytes não-UTF-8 viram `?`).
- Tempo no A065M: `nanosleep(2 ms)` estoura 58–118 µs (média 70–89); `Timer_WaitFrame` 120 quadros a 60 fps = 1983,3–1983,5 ms (ideal 1983,3), 0 atrasados. O "cpu 109–117%" da primeira versão contava as threads do GL do app (`CLOCK_PROCESS_CPUTIME_ID`); agora mede só a thread.
Achado: **o armazenamento emulado (`/storage/emulated/0`) ignora a caixa**; o resolvedor nem precisa agir ali (o `stat` do caminho como está já acha). As 2 falhas do auto-teste eram suposição minha (esperava a grafia corrigida e a ausência de `SOUND`), não bug. O auto-teste agora detecta o tipo de sistema de arquivos e roda também na pasta de cache do app (case-sensitive, caminhos absolutos). Onde o resolvedor realmente importa: `filesDir` (fallback), e qualquer pasta que a P11 copie para o armazenamento interno.
Fechamento (2026-11-25, A065M + `android.yml` do commit 45a4c53): auto-teste `RESULT: 19 checks, 0 failed` (19 e não 20: onde o sistema de arquivos ignora a caixa, a checagem "sem segundo diretório" é pulada). Pasta de dados detectada como case-INSENSITIVE, cache do app como case-sensitive, ambos 100%. Com o M.U.G.E.N 1.1b1 copiado por `adb push`: `data\mugen.cfg`, `data\fight.def`, `chars`, `stages`, `font` achados (`sound` não existe na raiz do 1.1b1); 15 entradas na raiz. `nanosleep(2 ms)` estoura 60–100 µs; `Timer_WaitFrame` 120 quadros = 1983,3 ms, 0 atrasados, 2% de CPU na thread. CI: `BUILD SUCCESSFUL`, 81 objetos de `mugen_core`, `libmugen.so` contém `Mugen_fopen`, `Mugen_ResolvePath`, `Timer_WaitFrame`, `Log_Init`, `File_Find`.
Armadilha de teste: desinstalar o app apaga `Android/data/<pacote>`; refazer o `adb push` depois de reinstalar.
Pendente (o item 1 original foi cumprido):
1. ~~Reinstalar o APK com a correção~~ e colar o relatório: esperado `RESULT: 20 checks, 0 failed`, com "case-INSENSITIVE" para a pasta de dados e "case-sensitive" para o cache.
2. `TIMER_SPIN_NS` (0,5 ms) está folgado frente ao estouro medido (<0,12 ms); pode cair para 0,25 ms na P5 se o consumo importar.
3. Conferir no `android.yml` que o passo 10 acha a cópia sem strip e lista `Mugen_fopen`, `Timer_WaitFrame`, `Log_Init`, `File_Find` (antes imprimiu `nm: '': No such file`).
4. P3b.

### 2026-11-25 · P3a — Plataforma sem SDL · conversa P3 #1
Estado da fase: EM ANDAMENTO (código e testes de host prontos; **CI Android e auto-teste no aparelho ainda não rodaram**). P3 foi dividida em P3a/P3b (ver 02-fases.md).
Branch/commit: android/p3-plataforma @ <hash do push; ver `git log`>
Feito:
- `src/platform/platform_path.c` (novo): `Mugen_fopen`/`Mugen_ResolvePath`/`MUGEN_PATH`, `\`→`/` e caixa ignorada com cache de diretório (regras em D-014); `fopen` do engine redirecionado em `platform_posix.h`; `unzOpen`, `TTF_OpenFont` e `luaL_loadfile` com `MUGEN_PATH`.
- `src/platform/platform_app.c` (novo): diretório de dados (`Mugen_SetDataDir` = chdir), `Mugen_LogWrite` (logcat), `Mugen_StdioToLog`, `Mugen_ShowError`/gancho, `Mugen_SleepNs`.
- `timer.c` (espera de quadro Android), `log.c` (espelha o log de status no logcat), `error.c` (`Error_ShowDialog` → `Mugen_ShowError`).
- CMake: `MUGEN_ANDROID_ENGINE_FLAGS` (inclui `-fsigned-char`) em todos os alvos, `mugen_platform`, `mugen_p3a` (temporário) e `libmugen.so` ligando ambos. JNI: `initPlatform`, `platformSelfTest`, `testError`, `showError` (Kotlin, AlertDialog). App: define o diretório de dados, roda o auto-teste em thread, botão "Test error dialog", `fitsSystemWindows` (tentativa de corrigir o botão "Copy report" escondido).
- `tests/platform` (novo, só host): roda `timer.c`, `log.c`, `file.c` reais + a plataforma. CI roda no `android.yml`; contagem de objetos de `mugen_core` agora >= 81.
- D-002 **decidida** (A, condicional), D-005 **decidida** (C agora, A na P11), D-014 nova, achados para D-003 (ver 03-decisoes.md).
Verificado (aqui, sem NDK/aparelho): testes de host 73 verificações, 0 falhas, no clang e no gcc com ASan+UBSan, e TSan sem avisos; `mugen_core` (81 `src/*.c`) compila com `fopen` redirecionado pelo CMake real; as sources novas compilam sem avisos (`-Wall -Wextra`); o mesmo auto-teste do aparelho rodou no host contra uma pasta de jogo falsa (11/11). Não verificado: NDK/Gradle/Kotlin (nunca compilados aqui), JNI (`AttachCurrentThread` usa a assinatura do NDK, `JNIEnv**`), Windows (só por inspeção: nos arquivos compartilhados o Windows só ganha `MUGEN_PATH(p)` = `(p)`).
Decisões tomadas (ids em 03-decisoes.md): D-002, D-005, D-014.
Pendente / próximos passos (em ordem):
1. Push feito; **conferir `android.yml` e `Build` Windows verdes** (primeira vez que o `libmugen.so` liga `mugen_p3a`; se faltar símbolo, é o `mugen_p3a_support.c`).
2. Instalar o APK no A065M, tocar nada: o relatório do auto-teste aparece na tela; copiar e colar aqui. Para o teste de arquivos: `adb push <M.U.G.E.N>/. /sdcard/Android/data/io.github.thalyson42.mugen/files/` e reabrir o app (deve listar `data\mugen.cfg`, `chars` etc. achados). Tocar "Test error dialog" e confirmar o diálogo.
3. Registrar o número de "Timer_WaitFrame ... cpu %" e o `sleep(2 ms) overshoot` do A065M (calibra `TIMER_SPIN_NS`, hoje 0,5 ms).
4. P3b (02-fases.md).
Problemas conhecidos / armadilhas:
- `mugen_p3a` e `mugen_p3a_support.c` definem globais que também estão em `globals.c`: **não ligar junto com `mugen_core`**.
- Auto-teste grava `mugen.log` na pasta de dados (sobrescreve um existente).
- `remove`/`rename`/`freopen` ainda sem resolução de caixa (P3b). Letras não-ASCII não são igualadas (D-014).
- Se o `getExternalFilesDir` não for navegável no gerenciador de arquivos do aparelho, usar adb (D-005).
- O fix `fitsSystemWindows` para "Copy report" é palpite: confirmar no aparelho.
- O código baixado do `sdl12-compat` (spike) ficou fora do repo; nada dele foi copiado.
Como validar:
- Host: `cmake -S tests/platform -B build/platform-tests -G Ninja && cmake --build build/platform-tests && ctest --test-dir build/platform-tests --output-on-failure` (esperado: `73 checks, 0 failed`).
- Android: `cd android && ./gradlew assembleDebug`; `adb install -r app/build/outputs/apk/debug/app-debug.apk`; abrir "MUGEN (diag)"; `adb logcat -s mugen mugen-diag`. Esperado no relatório: `RESULT: N checks, 0 failed` e `char is signed ... [OK]`.
- Windows: `tools\msvc_env.bat cmake --preset x86-release` e `cmake --build --preset x86-release`.
Arquivos alterados: src/platform/{platform_path.c,platform_app.c,platform_posix.h,platform_win32.h}, src/{timer,log,error,font,select,stream,luaapi}.c, cmake/MugenAndroid.cmake, android/app/src/main/{cpp/mugen_android_stub.c,cpp/mugen_selftest.c,cpp/mugen_p3a_support.c,java/.../Native.kt,java/.../MainActivity.kt,AndroidManifest.xml}, tests/platform/**, .github/workflows/android.yml, docs/android-port/{README,01,02,03,LOG}.md
Arquivos que a PRÓXIMA conversa (P3b) precisa receber: README.md, 03-decisoes.md (D-002, D-003, D-013, D-014), esta entrada, o log do CI Android, o relatório do auto-teste do aparelho, e os de "Levar" da P3b em 02-fases.md.

### 2026-11-25 · P2 — Portabilidade do código · conversa 1
Estado da fase: CONCLUÍDA (fechamento: run #9 do `android.yml` verde com o passo que exige >= 82 objetos de `mugen_core`; `Build` Windows verde no lote 2, 81 objetos, sem avisos). O Windows só foi verificado por compilação.
Branch/commit: android/p2-portabilidade. Entregue em dois lotes (patch aplicado pelo dono com `git am`, a conversa não tinha credencial de push): lote 1 = camada de plataforma + código; lote 2 = avisos de 64 bits, alvo CMake e docs.
Feito: D-012 e D-013 (ver 03-decisoes.md). `src/platform/` (platform.h, platform_win32.h, platform_posix.h/.c, sdl_config_posix.h, stubs/); macros `MUGEN_*` em 26 arquivos; `windows.h` isolado; Win32 puro atrás de `MUGEN_PLATFORM_WIN32`;
  `MUGEN_ASSERT_SIZE` só no Win32; `Ftol64`/`Trunc64`; campos ponteiro-em-`int` corrigidos; alvo `mugen_core` no CMake Android.
Verificado: lote 1: `Build` Windows verde (153/153, sem avisos do MSVC) e `android.yml` verde (mas só compilava o stub da P1). Lote 2: 81/81 `src/*.c` compilam com clang `--target=aarch64-linux-gnu`
  (proxy local: headers glibc em vez de bionic, sem NDK), pelo CMake real (`mugen_core`), sem avisos de ponteiro↔int. **O CI Android com o NDK ainda não rodou o lote 2.**
Pendente (não bloqueia a P3):
1. Confirmar que o Windows continua idêntico em comportamento (só compilou até aqui). Se houver como rodar o `mugen.exe`, comparar o log de inicialização.
2. Revisar os usos de `Trunc64`/`Ftol64` e de `float`/`double` dependentes de x87 antes da P5 (R2).
Problemas conhecidos / armadilhas:
- `char` do arm64 é sem sinal; `-fsigned-char` está no alvo `mugen_core` e **tem de ir também para o `libmugen.so`** quando `mugen_core` for linkado (P3).
- `int64_t` é `long` no LP64: usar `MUGEN_INT64`/`long long`, nunca `int64_t`, onde o código declara `long long`.
- Os stubs de GL/SDL/libresample (D-013) e a `platform_posix` são provisórios: não os trate como a camada final.
- `Video_SetFullscreen`, o diálogo de erro e o ícone da janela no Android são mínimos (TODO P3); o plugin BGM `.dll` não carrega (P4).
- `sizeof` de várias structs muda no arm64 (ponteiros de 8 bytes); nada deve depender dos offsets do binário de 32 bits fora dos asserts do Windows.
Como validar: Windows: `tools\msvc_env.bat cmake --preset x86-release` e `cmake --build --preset x86-release`. Android: `cmake --preset android-arm64-debug && cmake --build --preset android-arm64-debug --target mugen_core`
  (ou o job `android.yml`: `assembleDebug` agora compila os 81 `src/*.c` + `platform_posix.c`).
Arquivos alterados: src/** (~50 arquivos), src/platform/** (novo), CMakeLists.txt, cmake/MugenAndroid.cmake, .github/workflows/android.yml (passo que conta os objetos), docs/android-port/{README,02-fases,03-decisoes,LOG}.md
Próxima: P3 — Camada de plataforma. Levar: README.md e 03-decisoes.md (docs/android-port), esta entrada do LOG.md e os arquivos de `02-fases.md` (P3): `src/video.c`, `system.c`, `sdlmain.c`, `main.c`, `events.c`, `frame.c`, `gameloop.c`, `timer.c`, `file.c`, `stream.c`, `ini.c`, `log.c`, `error.c`, `config.c`, `cmdline.c`, `src/platform/*`.

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
