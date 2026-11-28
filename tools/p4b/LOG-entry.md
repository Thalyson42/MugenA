### 2026-11-28 · P4 · P4b — libmpg123 e BGM (MP3) · conversa P3b/P4 #3
Estado: **P4a CONFIRMADA no aparelho (CI Android e `Build` verdes; o dono ouve os efeitos sonoros no jogo; no logcat `Initializing sound... OK`).** **P4b: código pronto e validado no HOST; CI e aparelho ainda não.** Falta o dono rodar `tools/vendor-p4b.sh`. P4c (SDL_ttf + FreeType + `zoomSurface`) não começada.
Branch: `android/p3b-sdl`. Decisão: **D-020** (complementa D-019).
Feito: `tools/vendor-p4b.sh` + `tools/p4b/mugen-integration.patch` (vendoriza `third_party/mpg123`; alvo `mpg123` SHARED no CMake com gate; `Mugen_LoadLibraryA` mapeia `libmpg123-0.dll` para `libmpg123.so`).
Verificado aqui (host): ver D-020 item 6 (compila, 21 símbolos, MP3 decodificado pela sequência do engine). **Não verificado:** NDK, `dlopen` no app, música no A065M.
Pendente / próximos passos:
1. Dono (Termux, na pasta do repo): `git pull`, `sh tools/vendor-p4b.sh`, `git add .`, `git commit -m "android(p4b): vendor libmpg123 1.33.7 + BGM"`, `git push`. O script também mescla este bloco no LOG e a D-020 em `03-decisoes.md`.
2. Dono: `gh workflow run Build --ref android/p3b-sdl` (Windows) e olhar o CI Android; se falhar, colar o log da etapa que falhou. Conferir no log do CI que `libmpg123.so` entrou no APK (a etapa "Check the SDL stack in the APK" lista as `.so`; se não listar, me avise).
3. Com CI verde: instalar, abrir o menu e mandar `adb logcat -d | grep -i mugen > ~/storage/downloads/mugen-logcat.txt`. Esperado: `Initializing BGM... OK` (antes: `Could not initialize BGM.`) e música no menu/intro se o `data/` tiver MP3 (o 1.1b1 do `kfm`/menu usa `sound/*.mp3`? confirmar nos arquivos do dono; se não houver MP3 no pacote, o dono pode copiar um MP3 e apontar no `system.def`/`select`).
4. P4c: SDL_ttf + FreeType + `zoomSurface` (texto TTF).
Problemas conhecidos / observações:
 (a) **Possível causa do desempenho baixo (HIPÓTESE, não medida):** o APK é `assembleDebug` e o CMake do projeto não define nível de otimização, então o AGP compila tudo (engine, SDL2, sdl12-compat, SDL_mixer) com `-O0`. Isto explicaria o `engine-thread cpu` de 31–46 ms por quadro. Teste barato para o P5: construir com `-O2` (ou variante release) e comparar o `perf:` do `mugen.log`. Não mexi nisso: é decisão do dono/P5 (R2: sem `-ffast-math`, sem FMA, só `-O2`).
 (b) `Initializing pads... failed` no log é esperado (nenhum controle conectado).
 (c) A libmpg123 é compilada com `-O2` mesmo em Debug, por causa da thread de BGM.
Arquivos alterados: tools/vendor-p4b.sh, tools/p4b/*, cmake/MugenAndroid.cmake, src/platform/platform_posix.[ch], third_party/mpg123/** (pelo script), docs/android-port/{README,03-decisoes,LOG}.md (pelo script)
Arquivos que a PRÓXIMA conversa precisa receber: README.md, 03-decisoes.md (D-003, D-006, D-019, D-020), esta entrada, o `mugen.log` + logcat com a música (ou sem ela), `src/bgm.c`, `src/bgmthread.c`, `src/bgmstream.c`.
