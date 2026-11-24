# Modelos de conversa e troca de conta

## 1. Prompt de abertura (colar no início de toda conversa de fase)
```
Projeto: port Android (Vulkan + GLES) do repo Thalyson42/MugenA (mugen-unleashed, reconstrução em C do M.U.G.E.N 1.1b1).
Fase desta conversa: <FASE, ex.: P3 — Camada de plataforma>.
Anexei: README.md da pasta docs/android-port (protocolo + quadro), 03-decisoes.md, a última entrada do LOG.md
e os arquivos listados em "Levar" da fase no 02-fases.md.
Branch: <android/p3-...>  Último commit: <hash>.
Regras: não quebrar o build Windows; manter comportamento original; marcar mudanças novas no código;
registrar decisões em 03-decisoes.md. Comece confirmando o escopo da fase e o que falta nas tarefas; se faltar
algum arquivo, peça só o necessário.
Ao final, me dê o bloco de handoff (modelo da seção 2).
```

## 2. Bloco de handoff (a conversa deve gerar ao terminar; colar no LOG.md)
```
### <AAAA-MM-DD> · <FASE> · conta <nome> · conversa <n>
Estado da fase: <não iniciada | EM ANDAMENTO | BLOQUEADA | CONCLUÍDA>
Branch/commit: <branch> @ <hash>
Feito:
- ...
Decisões tomadas (ids em 03-decisoes.md): D-0xx ...
Pendente / próximos passos (em ordem):
1. ...
Problemas conhecidos / armadilhas:
- ...
Como validar (comandos exatos / cenários):
- ...
Arquivos alterados: <lista>
Arquivos que a PRÓXIMA conversa precisa receber: <lista, só o necessário>
```

## 3. Troca de conta
**Quem entrega:**
1. Faça push de todas as branches `android/*` e abra/atualize um PR rascunho por fase em andamento.
2. Confirme que `README.md` (quadro) e `LOG.md` estão atualizados e commitados.
3. Conceda à outra conta acesso ao repo (colaborador) — as conversas de chat **não** são transferíveis.
4. Anote no `LOG.md` quais fases ficam com quem (evita trabalho duplicado).

**Quem recebe:**
1. Clone o repo, abra a branch da fase indicada, leia `docs/android-port/README.md` → `01` → `03` → última entrada do `LOG.md`.
2. Use o **prompt de abertura** com a fase e o hash do último commit.
3. Rode a validação descrita no último bloco de handoff antes de mexer em qualquer coisa.
4. Se algo no handoff não bater com o código, registre a divergência no `LOG.md` e corrija a doc primeiro.

## 4. Regras para manter a documentação viva
- Um fato, um lugar: decisões em `03`, estado em `README.md`, histórico em `LOG.md`, mapa técnico em `01`.
- Não registre informação que só existe no chat (comandos, versões, caminhos de SDK/NDK usados).
- Se uma fase mudar de escopo, edite `02-fases.md` e deixe uma linha no `LOG.md` explicando por quê.

## 5. Conversa coordenadora (papel fixo)
Existe uma conversa **coordenadora** (a primeira do projeto, ou a que a substituir). Ela não implementa; ela:
1. entrega a tarefa de cada fase (objetivo, tarefas, arquivos a levar, prompt de abertura pronto);
2. recebe de volta o bloco de handoff + branch/commit e **verifica no repo** (diff, build, validação descrita) antes de aprovar;
3. marca a fase como `CONCLUÍDA` (quadro do `README.md` + `LOG.md`) e entrega a próxima;
4. mantém `03-decisoes.md` e `02-fases.md` coerentes (divide/une fases quando o escopo mudar).

Ciclo: coordenadora → tarefa → conversa de fase (branch `android/p<n>-<tema>`) → handoff → coordenadora verifica → próxima tarefa.
Se a coordenadora for perdida ou trocada: abrir conversa nova, anexar `README.md`, `03-decisoes.md`, `LOG.md` e dizer
"você é a conversa coordenadora deste projeto"; ela reconstrói o estado pelo quadro e pelo `git log` das branches `android/*`.
