# Reformulação de usabilidade do Albion Assistant Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Tornar os cadastros de HUD, status, perfis e regras claros, independentes e diretamente utilizáveis, preservando configurações existentes e a leitura/overlay atuais.

**Architecture:** A base continua em Win32/C++ e mantém o editor concentrado em `src/app_ui.cpp`. O modelo move dados contextuais de stacks e relógio da biblioteca de status para cada regra, com migração na leitura do `workspace.ini`; o monitor recebe leitores materializados por regra quando a configuração visual for diferente. A interface passa a persistir mudanças válidas ao trocar de contexto e usa estados vazios e pendências estruturadas para levar o usuário ao cadastro necessário.

**Tech Stack:** C++20, Win32/CommCtrl, CMake/Ninja, testes nativos em C++ e script PowerShell de build Windows.

**Spec:** `docs/superpowers/specs/2026-09-22-ux-reconfiguration-design.md`

## Global Constraints

- Windows x64; nenhum novo framework, serviço ou dependência externa.
- O único artefato publicado continua sendo `dist/AlbionAssistant.exe`.
- Não automatizar entrada do jogo, login, memória ou rede; somente captura visual e overlay.
- Conservar dados do usuário em `%LOCALAPPDATA%\AlbionAssistant` e aceitar `--settings`.
- A leitura só inicia quando as dependências da HUD/perfil/regra ativa estiverem prontas.
- Usar o build canônico: `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1`.

---

## Estrutura de arquivos

| Arquivo | Responsabilidade após a mudança |
|---|---|
| `src/workspace.h` | Modelo persistido, migração e pendências estruturadas de execução. |
| `src/workspace.cpp` | Leitura/gravação compatível do workspace, validações e análise de dependências. |
| `src/monitor.h` | Contratos de leitores materializados e pendências resolvíveis. |
| `src/monitor.cpp` | Montagem de leitores por configuração efetiva de regra e ações de overlay. |
| `src/app.h` | IDs de controles, seleção de pendência e helpers de navegação do editor. |
| `src/app.cpp` | Página inicial, tentativa de início e acionamento de resolução de pendências. |
| `src/app_ui.cpp` | Abas, estados vazios, cadastro direto, edição automática, exclusão e editor contextual da regra. |
| `tests/workspace_tests.cpp` | Migração, validações e impacto das exclusões. |
| `tests/monitor_tests.cpp` | Leitores de stacks/relógio por regra e avaliação de ações. |
| `tests/app_flow_tests.cpp` | Fluxos visuais e funcionais do cadastro, navegação e monitor. |
| `README.md` | Guia de uso alinhado à nova interface. |

## Task 1: Migrar configurações contextuais para a regra

**Files:**
- Modify: `src/workspace.h:20-64`
- Modify: `src/workspace.cpp:60-105, 225-250, 420-450, 480-560`
- Modify: `src/monitor.h:3-16`
- Modify: `src/monitor.cpp:7-50`
- Modify: `tests/workspace_tests.cpp`
- Modify: `tests/monitor_tests.cpp`

**Interfaces:**
- Produces `StatusRule::stackSamples` e `StatusRule::clockReferencePath`.
- Produces `ReadinessIssue { std::wstring message; std::wstring ruleId; enum class Resolution { Hud, StatusReference, StackSample, ClockReference, SourceArea, SourceCalibration, TargetArea, ProfileRule }; }`.
- Replaces `std::vector<std::wstring> readinessIssues(const Workspace&)` with `std::vector<ReadinessIssue> readinessIssues(const Workspace&)`; consumidores usam `.message` para texto.
- `MonitorReader` continua entregando um `StatusDefinition`, mas recebe uma cópia materializada apenas com a amostra e a referência temporal requeridas pela regra.

- [ ] **Step 1: Escrever os testes de migração e isolamento de regra**

Em `tests/workspace_tests.cpp`, adicione um workspace legado com `StatusDefinition::stacks={{3,L"legado-3.png"}}` e `clockReferencePath=L"clock.png"`, duas regras para o mesmo status e valide que, após `loadWorkspace`, cada regra de stacks recebe sua própria cópia de `StackSample{3,...}` e cada regra com `followClock` recebe a referência do relógio. Valide que `saveWorkspace` não volta a gravar `debuff`, `builtinAssassin`, `status.stackCount` nem `status.clockReferencePath` em workspaces novos.

Em `tests/monitor_tests.cpp`, adicione duas regras do mesmo status/origem: uma com amostra de 2 e outra com amostra de 3. Verifique que `makeMonitorPlan` cria leitores distintos e que cada `MonitorReader.status.stacks` contém somente a amostra da regra correspondente.

- [ ] **Step 2: Executar os testes e confirmar a falha inicial**

Run:

```powershell
cmake --build build/windows-release --target workspace_tests monitor_tests
ctest --test-dir build/windows-release -R "workspace_tests|monitor_tests" --output-on-failure
```

Expected: os novos testes falham porque `StatusRule` não possui `stackSamples` nem `clockReferencePath`.

- [ ] **Step 3: Implementar o modelo e a migração mínima**

Em `src/workspace.h`, mantenha `StackSample`, remova os campos novos de `StatusDefinition` que não pertencem à biblioteca e acrescente à regra:

```cpp
struct StatusRule {
    std::wstring id, statusId;
    std::wstring sourceArea, targetArea;
    Rule condition;
    OverlayEffect effect = OverlayEffect::Border;
    std::vector<StackSample> stackSamples;
    std::wstring clockReferencePath;
    bool followClock = false;
};
```

Na leitura de formatos antigos, leia `status.stacks` e `status.clockReferencePath` apenas como dados de migração. Para cada regra carregada, copie a amostra do valor exigido para `rule.stackSamples` quando a condição for `StacksEqual`; copie o relógio para `rule.clockReferencePath` somente quando `followClock` estiver ativo. A gravação nova persiste `rule.stackCount`, `rule.stack.N` e `rule.clockReferencePath`; não regrava os campos legados do status.

Faça `readinessIssues` retornar objetos estruturados. Para uma condição de stacks, procure exclusivamente `rule.stackSamples` pelo valor de `rule.condition.stacks`. Para relógio, procure exclusivamente `rule.clockReferencePath` quando `followClock` estiver ligado. Preserve a mensagem concreta atual e associe a `Resolution` correta.

Em `src/monitor.cpp`, materialize um leitor copiando a identidade do status, substituindo `stacks` pelas amostras da regra e `clockReferencePath` pela referência da regra. Só reutilize leitor se `statusId`, origem, amostras exigidas e referência temporal forem iguais.

- [ ] **Step 4: Executar os testes focados**

Run:

```powershell
cmake --build build/windows-release --target workspace_tests monitor_tests
ctest --test-dir build/windows-release -R "workspace_tests|monitor_tests" --output-on-failure
```

Expected: PASS, incluindo importação de workspace antigo e dois leitores distintos para configurações diferentes.

- [ ] **Step 5: Commit**

```powershell
git add src/workspace.h src/workspace.cpp src/monitor.h src/monitor.cpp tests/workspace_tests.cpp tests/monitor_tests.cpp
git commit -m "Move stacks e relógio para regras"
```

## Task 2: Reordenar as abas e remover estados sem saída

**Files:**
- Modify: `src/app.h:20-90`
- Modify: `src/app.cpp:load/start/refresh paths`
- Modify: `src/app_ui.cpp:210-350, 690-760`
- Modify: `tests/app_flow_tests.cpp`

**Interfaces:**
- Produces as páginas `HudPage=0`, `StatusPage=1`, `RulesPage=2`, `MonitorPage=3`.
- Produces `App::openPageFor(ReadinessIssue::Resolution)` e `App::resolveSelectedIssue()`.
- A seleção atual da pendência do Monitor fica em `App::selectedIssue`.

- [ ] **Step 1: Escrever testes de abas e estados vazios**

Em `tests/app_flow_tests.cpp`, crie um `Workspace{}` vazio e verifique:

```cpp
app.load();
require(app.page == HudPage, "primeiro uso deve abrir em HUDs");
require(shows(app, L"Nenhuma HUD cadastrada"), "HUD vazio não orienta criação");
require(app.item(NewHud) != nullptr, "HUD vazio não oferece criação");
```

Depois acrescente HUD, status, perfil e regra prontos, recarregue e verifique `app.page == MonitorPage`. Em cada aba vazia, valide a mensagem e o único botão de criação aplicável. Na página Monitor sem pré-requisitos, valide que não há comboboxes vazios e que existe uma lista de pendências com ação `Resolver`.

- [ ] **Step 2: Executar o teste e confirmar a falha inicial**

Run:

```powershell
cmake --build build/windows-release --target app_flow_tests
ctest --test-dir build/windows-release -R app_flow_tests --output-on-failure
```

Expected: FAIL porque a página inicial é Monitor e os seletores ainda aparecem vazios.

- [ ] **Step 3: Implementar navegação orientada ao estado**

Em `src/app.h`, nomeie os índices de aba e substitua usos literais de `page==0`, `page==1` etc. Em `App::load`, escolha `HudPage` quando `workspace.huds.empty()` e `MonitorPage` em qualquer outro caso.

Em `makeUI`, substitua os rótulos por `HUDs`, `Status`, `Perfis e regras`, `Monitorar`, nesta ordem. Nas três abas de cadastro, quando a coleção estiver vazia, renderize a mensagem da especificação e só o botão `Novo` correspondente. Não crie combos/listas sem itens nesses estados.

Na aba Monitorar, mantenha os seletores de HUD/perfil quando existem opções. Quando alguma dependência estiver ausente, renderize a lista de `ReadinessIssue` e o botão `Resolver` associado ao item selecionado; `resolveSelectedIssue()` seleciona a HUD/status/perfil/regra pertinente, abre a página apropriada e refaz a interface. Use a mesma API para erros retornados por `start()`.

- [ ] **Step 4: Executar os testes focados**

Run:

```powershell
cmake --build build/windows-release --target app_flow_tests
ctest --test-dir build/windows-release -R app_flow_tests --output-on-failure
```

Expected: PASS, com primeiro uso em HUDs e Monitorar sem controles vazios.

- [ ] **Step 5: Commit**

```powershell
git add src/app.h src/app.cpp src/app_ui.cpp tests/app_flow_tests.cpp
git commit -m "Organiza navegação e estados vazios"
```

## Task 3: Tornar criação, edição e exclusão consistentes

**Files:**
- Modify: `src/app.h:20-90`
- Modify: `src/app_ui.cpp:30-170, 450-690`
- Modify: `src/workspace.cpp:470-560`
- Modify: `tests/app_flow_tests.cpp`
- Modify: `tests/workspace_tests.cpp`

**Interfaces:**
- Removes `requestName`, `NamePrompt` e IDs obsoletos `StatusKind`, `CaptureStack`, `StackList`, `DeleteStack`, `AddPreset`.
- Produces `App::createItem(int id)`, que cria e seleciona um item com `uniqueName` e persiste imediatamente.
- Produces `App::commitEditorIfValid()`, usado em `EN_KILLFOCUS`, troca de seleção e troca de aba.
- Produces `std::vector<std::wstring> dependentRules(const Workspace&, EntityRef)` para mensagens de exclusão.

- [ ] **Step 1: Escrever testes de criação em uma ação e persistência ao sair do campo**

Substitua os helpers baseados em diálogo em `tests/app_flow_tests.cpp` por cliques diretos. Para cada entidade, valide:

```cpp
app.command(NewHud, BN_CLICKED);
require(app.hud() && text(app.item(HudName)) == L"Nova HUD", "Nova HUD não criou item editável");
SetWindowTextW(app.item(HudName), L"Monitor 34");
app.command(HudName, EN_KILLFOCUS);
require(aa::loadWorkspace(app.workspacePath, {}).huds.front().name == L"Monitor 34", "edição não foi salva ao sair do campo");
```

Repita para área, status, perfil e regra. Valide que um nome vazio/repetido não substitui o valor salvo. Valide que apagar área/status/perfil exibe os nomes das regras impactadas e que apagar uma regra não altera outros itens.

- [ ] **Step 2: Executar o teste e confirmar a falha inicial**

Run:

```powershell
cmake --build build/windows-release --target app_flow_tests workspace_tests
ctest --test-dir build/windows-release -R "app_flow_tests|workspace_tests" --output-on-failure
```

Expected: FAIL porque `Novo` abre diálogo e o salvamento ainda depende de `Salvar`/troca de tela.

- [ ] **Step 3: Implementar cadastro direto e exclusões com impacto**

Remova `NamePrompt`, `requestName`, `validateNewName` e os IDs sem interface. Faça cada comando `New*` criar o item imediatamente com nomes únicos `Nova HUD`, `Nova área`, `Novo status`, `Novo perfil` e `Nova regra`; selecione e foque o campo de nome. `commit` ocorre logo após criação.

No manipulador de notificações, trate `EN_KILLFOCUS` dos campos de nome e valores da regra com `commitEditorIfValid()`. `EN_CHANGE` apenas atualiza o texto de estado para `Alteração não salva`; não redesenha a página. `commitEditorIfValid()` chama `editorValues()`, mantém o estado anterior se falhar e apresenta o motivo no rodapé.

Substitua `Salvar HUD`, `Salvar status`, `Salvar set e regra` e `Salvar ajuste` por nenhum botão de salvar. Preserve `Salvar` apenas se ainda for necessário para compatibilidade interna, sem renderizá-lo.

Antes de excluir, obtenha dependências no workspace e componha a confirmação concreta. Para status, ofereça uma mensagem de bloqueio se houver regras dependentes; para área, informe as regras que perderão origem/destino; para perfil, informe a contagem de regras que será removida. Não apague imagens de referência neste escopo.

- [ ] **Step 4: Executar os testes focados**

Run:

```powershell
cmake --build build/windows-release --target app_flow_tests workspace_tests
ctest --test-dir build/windows-release -R "app_flow_tests|workspace_tests" --output-on-failure
```

Expected: PASS, com criação de uma ação, persistência ao sair do campo e exclusões explicadas.

- [ ] **Step 5: Commit**

```powershell
git add src/app.h src/app_ui.cpp src/workspace.cpp tests/app_flow_tests.cpp tests/workspace_tests.cpp
git commit -m "Simplifica cadastro e exclusão"
```

## Task 4: Implementar o editor contextual de regras

**Files:**
- Modify: `src/app.h:20-90`
- Modify: `src/app_ui.cpp:300-370, 490-690`
- Modify: `src/app.cpp:280-350`
- Modify: `tests/app_flow_tests.cpp`
- Modify: `tests/recognition_tests.cpp`

**Interfaces:**
- Produces comandos `CaptureRuleStack`, `ReplaceRuleStack`, `DeleteRuleStack`, `CaptureRuleClock`, `ReplaceRuleClock`, `DeleteRuleClock`.
- Produces `StatusRule::sample(unsigned value)` helper local em `app_ui.cpp` ou função equivalente que procura `rule.stackSamples`.
- `App::applyClockReference` passa a receber a regra selecionada e grava `rule.clockReferencePath`.

- [ ] **Step 1: Escrever testes do contexto de stacks e relógio**

Em `tests/app_flow_tests.cpp`, selecione uma regra `StacksEqual` e valide que aparecem texto de estado da amostra, miniatura/preview e os comandos `Capturar amostra`, `Substituir amostra` e `Excluir amostra`. Escolha `Present` e valide que todos eles desaparecem. Marque `FollowClock` em uma regra presente e valide que aparecem somente então `Capturar relógio`, `Substituir relógio` e `Excluir relógio`. Ao excluir a amostra, valide que a regra permanece e `readinessIssues` acusa exatamente essa regra.

Em `tests/recognition_tests.cpp`, construa um `MonitorReader` com a identidade válida e sem `clockReferencePath`; valide que presença continua reconhecível e somente `clockReady()` é falso.

- [ ] **Step 2: Executar o teste e confirmar a falha inicial**

Run:

```powershell
cmake --build build/windows-release --target app_flow_tests recognition_tests
ctest --test-dir build/windows-release -R "app_flow_tests|recognition_tests" --output-on-failure
```

Expected: FAIL porque relógio ainda é um cadastro global do status e não existem substituir/excluir para amostra e relógio da regra.

- [ ] **Step 3: Implementar blocos “Quando”, “Então” e “Testar”**

Em `makeUI`, apresente a regra em três grupos visuais:

- `Quando`: status, área de origem, condição, valor e amostra de stacks.
- `Então`: área de destino, efeito, cor e intensidade; mantenha os quatro efeitos existentes e use a intensidade para a opacidade/escala já suportada pelo overlay, sem novo motor de efeito.
- `Testar`: botão existente, habilitado somente se destino e efeito forem válidos.

Inclua a condição `Tem ao menos N stacks`. Para esta POC, ela usa a mesma referência de N stacks e ativa apenas quando o reconhecimento retornar contador conhecido maior ou igual a N; acrescente `Condition::StacksAtLeast` a `src/model.h`, `evaluate`, persistência, escolha de UI e testes.

Quando uma condição exigir stacks, `Capturar amostra` substitui a amostra daquele valor se já existir. `Excluir amostra` remove apenas a entrada em `rule.stackSamples`. A captura valida o contador com `Recognizer::setStackReference` antes de salvar.

Quando `FollowClock` estiver ativo e a condição não for ausência, ofereça captura/substituição/exclusão de referência temporal para a regra selecionada. Atualize `applyClockReference` para preservar status, HUD e amostras de outras regras. Uma referência temporal ausente deixa apenas o aro pendente; aura/borda/pulso continuam prontos.

- [ ] **Step 4: Executar os testes focados**

Run:

```powershell
cmake --build build/windows-release --target app_flow_tests recognition_tests
ctest --test-dir build/windows-release -R "app_flow_tests|recognition_tests" --output-on-failure
```

Expected: PASS, incluindo exclusão de amostra, relógio contextual e condição “ao menos”.

- [ ] **Step 5: Commit**

```powershell
git add src/model.h src/app.h src/app.cpp src/app_ui.cpp tests/app_flow_tests.cpp tests/recognition_tests.cpp
git commit -m "Torna regras contextuais"
```

## Task 5: Concluir Monitorar, documentação e entrega

**Files:**
- Modify: `src/app.cpp:refreshStatus/start`
- Modify: `src/app_ui.cpp:MonitorPage`
- Modify: `tests/app_flow_tests.cpp`
- Modify: `README.md`
- Modify: `docs/validacao.md`

**Interfaces:**
- `App::refreshStatus()` transforma `ReadinessIssue` em resumo operacional, sem termos de implementação desnecessários.
- `App::testAction()` continua sem leitura do jogo e respeita forma/cor/efeito da regra selecionada.

- [ ] **Step 1: Escrever os testes de monitoramento pronto e pendente**

Em `tests/app_flow_tests.cpp`, verifique que uma configuração pronta exibe HUD/perfil ativos, `Iniciar leitura`, `Parar`, teste e compartilhamento de overlay. Para uma regra com referência ausente, valide a mensagem concreta:

```cpp
require(shows(app, L"capture uma imagem de referência"), "Monitor não explica referência ausente");
require(app.item(ResolveIssue) != nullptr, "Monitor não oferece resolver pendência");
```

Acione `ResolveIssue` e valide a seleção do status/regra correta e a abertura da aba correspondente. Verifique que nenhuma mensagem usa “set”, “calibração” ou “escala” sem dizer a área e a ação necessária.

- [ ] **Step 2: Executar o teste e confirmar a falha inicial**

Run:

```powershell
cmake --build build/windows-release --target app_flow_tests
ctest --test-dir build/windows-release -R app_flow_tests --output-on-failure
```

Expected: FAIL porque o Monitor ainda mostra instruções genéricas e não consegue resolver uma pendência.

- [ ] **Step 3: Implementar o painel operacional e atualizar documentação**

Na página Monitorar, mantenha somente controles de operação: HUD/perfil ativos, conectar, iniciar/parar, teste quando houver regra selecionável e checkbox de compartilhamento. Use `ReadinessIssue.message` para cada pendência e `resolveSelectedIssue()` para abrir o item correto.

Revise `README.md` para descrever as quatro abas, a criação imediata, o editor “Quando/Então/Testar”, captura contextual de stacks/relógio, teste do overlay e compartilhamento. Remova menções a exemplos incluídos, tipo buff/debuff, cadastro global de amostras, botão Salvar e `Status → Capturar relógio`.

Em `docs/validacao.md`, registre como pendência de validação manual: primeira configuração vazia, exclusões com impacto, substituição/exclusão de amostra, regra ao menos N stacks, aro temporal e uso real em uma HUD diferente.

- [ ] **Step 4: Executar a validação completa e publicar o executável**

Run:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Expected: todos os testes do script passam e `dist\AlbionAssistant.exe` é o único arquivo publicado.

- [ ] **Step 5: Commit**

```powershell
git add src/app.cpp src/app_ui.cpp tests/app_flow_tests.cpp README.md docs/validacao.md
git commit -m "Finaliza fluxo de configuração guiado"
```

## Revisão do plano

### Cobertura da especificação

- Abas, primeiro uso e estados vazios: Task 2.
- Criar, editar, salvar automaticamente e excluir: Task 3.
- Biblioteca enxuta de status: Tasks 1 e 3.
- Perfis, linguagem da regra, stacks, aro temporal e teste: Task 4.
- Pendências úteis e execução ao vivo: Task 5.
- Migração de dados e preservação de configurações: Task 1.
- Artefato Windows único e documentação: Task 5.

### Verificação de consistência

- `StatusRule::stackSamples` e `StatusRule::clockReferencePath` são definidos na Task 1 e consumidos nas Tasks 1 e 4.
- `ReadinessIssue` é definido na Task 1 e consumido nas Tasks 2 e 5.
- `ResolveIssue` é introduzido na Task 2 e validado na Task 5.
- Cada tarefa começa por teste, comprova falha, implementa, comprova sucesso e termina em commit.

### Limites deliberados

- Não há novo motor de overlay nem dependências gráficas; os efeitos atuais são reaproveitados.
- Imagens que não estão mais referenciadas não são apagadas automaticamente, evitando remover arquivos do usuário por engano.
- O plano não tenta transformar contadores visuais em OCR universal; continua usando amostras de imagem do número visível.
