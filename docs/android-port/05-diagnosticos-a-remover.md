# Diagnósticos temporários — remover antes do release (P12)

> Tudo aqui foi colocado só para investigar problemas no aparelho de referência (SM-A065M). **Nenhum muda o comportamento
> do jogo**, mas custam tempo/log e não devem ir para o APK de release. Cada item está marcado no código com o texto
> **`REMOVER-NO-RELEASE`**. Para achar todos: `grep -rn "REMOVER-NO-RELEASE" .` (e conferir que não sobra nada com
> `grep -rniE "spritediag|mugen-wd|mugen-ui|Mugen_Diag|Mugen_WatchdogBeat" --include=*.c --include=*.kt --include=*.cmake --include=*.yml .`).
> Quando um item for removido, risque-o aqui e registre no `LOG.md`.

| # | Item | Onde | Tag no log | Para que serviu | Remover quando |
|---|------|------|-----------|-----------------|----------------|
| 1 | Gravador de escritas + detector de "traços" | `android/app/src/main/cpp/mugen_spritediag.c`; fonte na lista de `cmake/MugenAndroid.cmake`; laço `foreach(_w ... --wrap=...)` no mesmo arquivo (também `SDL_UpperBlit`/`SDL_FillRect`); chamadas `Mugen_DiagWantScan`/`Mugen_DiagScanFrame` em `Gfx_AndroidDebugFrame` (`src/gfx.c`) | `spritediag` | achar quem desenha os traços escuros (~8x2 px) do menu/lutas | causa dos traços achada **ou** decidido que não vale mais a pena (ver `LOG.md`, "dashes parked") |
| 2 | Log de quadro 30 da tela composta | `Gfx_AndroidDebugFrame` em `src/gfx.c` (bloco `#if defined(MUGEN_PLATFORM_ANDROID)`) | `android frame` | distinguir tela preta de problema de apresentação | junto com o item 3 (ele também chama o beat do watchdog) |
| 3 | Watchdog nativo de travamento | `android/app/src/main/cpp/mugen_watchdog.c` (+ lista de fontes no CMake); declaração e chamada `Mugen_WatchdogBeat` em `src/gfx.c` | `mugen-wd` | despejar a pilha de todas as threads quando a engine para de apresentar quadros (achou o deadlock do toque e o abort do Lua) | fim do P3b/M3 com boot estável (ou P12 no mais tardar) |
| 4 | Watchdog da thread de UI | `android/app/src/main/java/io/github/thalyson42/mugen/UiWatchdog.kt` e a chamada `UiWatchdog.start()` em `MugenActivity.kt` | `mugen-ui` | mostrar quem bloqueia a thread principal (achou o diálogo de asserção do SDL2 = ANR) | junto com o item 3 |
| 5 | Simbolização de endereços de crash no CI | passo "Symbolize device crash addresses" e o passo `nm` anterior em `.github/workflows/android.yml`; arquivo `android/tools/symbolize-addrs.txt` | (saída do CI) | transformar `pc` de tombstone em função/linha | quando não houver mais crash nativo para investigar |
| 6 | Nome "MUGEN (diag)" do app | `android:label` em `android/app/src/main/AndroidManifest.xml` | — | distinguir o APK de diagnóstico | P11 (app shell) / release: nome final |
| 7 | Relatório de diagnóstico do launcher e `mugen-diag` | `MainActivity.kt` (tela com relatório, cópia para a área de transferência, `GL_EXTENSIONS`) | `mugen-diag` | verificar GL/Vulkan e a sonda SDL no aparelho | P11: decidir o que vira tela "Sobre/Diagnóstico" do usuário e o que sai |

## NÃO remover (parecem diagnóstico, mas são correções)
- `SDL_ASSERT_LEVEL=0` no SDL2 e no sdl12-compat (`cmake/MugenAndroid.cmake`, D-018): sem isso o diálogo "Assertion Failed" trava a UI (ANR).
- Patch de ordem de locks no `sdl12-compat` (D-015 item 9): evita o deadlock do toque.
- Lua sem `_FORTIFY_SOURCE` (`cmake/MugenAndroid.cmake`): falso positivo do bionic.
- Override de `RenderMode` para System e profundidade 16 (D-013), `Timer_ResetFrame` em `System_OnFocusGained`, `SDL_ACCELEROMETER_AS_JOYSTICK=0`, `sensorLandscape`, `SDL_EnableUNICODE` pulado no Android (D-017).
- Os `Log_Printf("android: ...")` de decisões de configuração (poucos, informativos).

## Como remover (resumo)
1. Itens 1–3: apagar `mugen_spritediag.c` e `mugen_watchdog.c`, tirar os dois do `target_sources`, apagar o `foreach(--wrap)` no CMake,
   apagar `Gfx_AndroidDebugFrame` e a chamada em `Gfx_Present` (`src/gfx.c`). Compilar Android **e** rodar `Build` (Windows).
2. Item 4: apagar `UiWatchdog.kt` e a linha em `MugenActivity.kt`.
3. Item 5: apagar os dois passos do workflow e `android/tools/`.
4. Itens 6–7: no P11.
5. Conferir com os `grep` do topo; rodar um boot no aparelho (menu → luta → Home → voltar) para ver que nada dependia deles.
