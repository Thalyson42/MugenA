# Port Android do mugen-unleashed — índice e protocolo de handoff

> **Leia este arquivo primeiro.** Ele é o ponto de entrada para qualquer conversa ou conta nova.

## Objetivo
Portar o `mugen-unleashed` (reconstrução em C do M.U.G.E.N 1.1 Beta 1, repo `Thalyson42/MugenA`,
hoje só Win32 x86/MSVC) para **Android**, com renderers **Vulkan** e **OpenGL ES (GLES)**.
Depois do port, haverá uma segunda etapa de **alterações/novos recursos** (fase reservada, ver `02-fases.md`).

## Decisões fixas
- **ABI:** `arm64-v8a` nativo, sem emulação (D-001). **Dispositivo de referência:** Samsung **SM-A065M** (relatório em D-010).
- **Renderers:** software (primeiro) → GLES → Vulkan; os três ficam no projeto (D-010).

## Princípios (valem para todas as conversas)
1. **Uma conversa = uma fase.** Cada fase tem tarefas, arquivos de entrada e critério de pronto em `02-fases.md`.
2. **O build do Windows não pode quebrar.** O build x86/MSVC é a referência de comportamento. Mudanças de
   plataforma ficam atrás de `MUGEN_PLATFORM_ANDROID` / `__ANDROID__` ou na camada `src/platform/`.
3. **Comportamento fiel ao original.** O repo reproduz bugs do original de propósito (ver `docs/ANALYSIS.md`).
   Correções e recursos novos devem ser marcados no código (`/* MUGEN-UNLEASHED: ... */`) e só entram na fase de alterações.
4. **Nada de dados do jogo no repo.** O usuário traz o próprio M.U.G.E.N 1.1b1.
5. **Toda decisão técnica vai para `03-decisoes.md`.** Toda conversa termina atualizando o **Quadro de status** abaixo.

## Quadro de status  (atualizar ao fim de TODA conversa)
| Fase | Nome | Estado | Branch / commit | Última conta/conversa | Observações |
|---|---|---|---|---|---|
| P0 | Fundação e decisões | EM ANDAMENTO | — | — | docs criados; D-001 e D-010 decididas (arm64 nativo; SM-A065M; software→GLES→Vulkan); D-002..D-006 pendentes |
| P1 | Build Android (NDK/CMake/Gradle/CI) | CONCLUÍDA | `android/p1-build` @ 6b561ce (PR → `android/main`) | conversa P1 #1 + coordenadora | CI Android e Build Windows verdes no PR; APK rodou no aparelho (relatório em D-010). Pendente menor: o botão "Copy report" não aparece no app (ver LOG) |
| P2 | Portabilidade do código (64 bits, Win32, `__stdcall`) | CONCLUÍDA | `android/p2-portabilidade` (run #9 do `android.yml` verde; `Build` Windows verde no lote 2) | conversa P2 #1 | `src/*.c` compila para arm64 pelo NDK (`mugen_core`, CI conta os `.o`); D-012 e D-013. Windows só foi verificado por compilação, não por execução |
| P3a | Plataforma sem SDL (tempo, arquivos, log, erro) | CONCLUÍDA (auto-teste 19/19 no A065M; mesclada na `android/main`, PR #4) | `android/p3-plataforma` | conversa P3 #1 | Resolve D-002 (A, condicional) e D-005 (C); testes de host em `tests/platform`; D-014 |
| P3b | SDL + ciclo de vida + boot (janela/superfície) | EM ANDAMENTO (M1a CONCLUÍDA e verificada; próximo: M1b) | `android/p3b-sdl` | conversa P3b #1 + coordenadora | M1a: SDL2 2.32.10 + sdl12-compat vendorizados, SDLActivity, `SDL_main` de teste (D-015); CI Android e Windows verdes no commit 567cf4a; janela, renderer GLES2 e ciclo de vida confirmados no A065M. Faltam M1b (API SDL 1.2 `SDL_SetVideoMode` 16 bpp), M2 (D-003 + ligar `mugen_core`), M3 (ciclo de vida no engine), M4 (verificação em execução, orientação) |
| P4 | Áudio e BGM | não iniciada | — | — | |
| P5 | Primeiro boot (renderer software → tela de título) | não iniciada | — | — | **Marco M1** |
| P6 | Abstração de renderer | não iniciada | — | — | |
| P7 | Backend GLES | não iniciada | — | — | **Marco M2** |
| P8 | Backend Vulkan | não iniciada | — | — | **Marco M3** |
| P9 | Modelos 3D / shaders de ambiente | não iniciada | — | — | |
| P10 | Entrada e controles de toque | não iniciada | — | — | |
| P11 | App shell, dados do jogo, ciclo de vida | não iniciada | — | — | |
| P12 | QA, desempenho e release | não iniciada | — | — | **Marco M4** |
| P13 | Alterações pós-port | reservada | — | — | escopo a definir pelo dono |

Estados: `não iniciada` · `EM ANDAMENTO` · `BLOQUEADA (motivo)` · `CONCLUÍDA` · `reservada`.

## Arquivos desta pasta
| Arquivo | Para quê |
|---|---|
| `README.md` | este índice, protocolo e quadro de status |
| `01-analise-do-repo.md` | o que foi descoberto no código e os riscos do port |
| `02-fases.md` | cada fase: objetivo, tarefas, **arquivos a levar para a conversa**, critério de pronto |
| `03-decisoes.md` | registro de decisões (ADR) e perguntas em aberto |
| `04-modelos-de-conversa.md` | prompt de abertura, bloco de handoff de fechamento, checklist de troca de conta |
| `LOG.md` | (criar na P1) diário curto: uma entrada por conversa |

## Como abrir uma conversa nova
1. Abra a fase no quadro; confirme que as fases de que ela depende estão `CONCLUÍDA` (grafo em `02-fases.md`).
2. Cole o **prompt de abertura** de `04-modelos-de-conversa.md`, trocando `<FASE>`.
3. Anexe/cole **somente** os arquivos listados na fase em "Arquivos para levar" + `README.md` + `03-decisoes.md`
   + a última entrada do `LOG.md`. Não leve o repo inteiro.
4. Se a conversa tiver acesso ao repo (clone), diga a branch e o último commit.

## Como fechar uma conversa
Peça o **bloco de handoff** (`04-modelos-de-conversa.md`), commit as mudanças na branch da fase, cole o bloco
no `LOG.md` e atualize o quadro acima.

## Troca de conta (checklist curto)
Ver `04-modelos-de-conversa.md`, seção "Troca de conta". Resumo: tudo que a outra conta precisa está **no repo**
(esta pasta + `LOG.md` + branches). Nada depende do histórico de chat.
