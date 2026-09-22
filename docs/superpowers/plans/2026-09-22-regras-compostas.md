# Regras compostas e edição consistente Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Permitir condições unidas por OU em uma ação de destaque e tornar a renomeação explícita em todos os cadastros.

**Architecture:** O schema 3 separa `StatusRule` em ação visual e `RuleTrigger` em condição de reconhecimento. O monitor cria leitores por gatilho e ativa uma única ação quando qualquer leitor correspondente satisfaz sua condição. A UI usa diálogos de renomeação já existentes, sem campos persistentes de nome.

**Tech Stack:** C++20, Win32, CMake/Ninja e testes C++ nativos.

**Spec:** `docs/superpowers/specs/2026-09-22-regras-compostas-design.md`

## Global Constraints

- Windows x64, sem dependências externas.
- Um único executável publicado em `dist/AlbionAssistant.exe`.
- Não controlar gameplay; somente captura visual e overlay.
- Preservar configurações do usuário e migrar schemas 1 e 2.
- Executar `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1` antes de publicar.

---

### Task 1: Modelo, migração e avaliação OU

**Files:**
- Modify: `src/workspace.h`
- Modify: `src/workspace.cpp`
- Modify: `src/monitor.h`
- Modify: `src/monitor.cpp`
- Test: `tests/workspace_tests.cpp`
- Test: `tests/monitor_tests.cpp`

**Interfaces:**
- Produces `RuleTrigger { id, statusId, sourceArea, Rule condition, vector<StackSample> stackSamples, wstring clockReferencePath }`.
- Produces `StatusRule { id, Rule action, targetArea, OverlayEffect effect, bool followClock, vector<RuleTrigger> triggers }`.
- `MonitorAction` retains one action and maps each trigger to one `MonitorReader`.

- [ ] Write failing tests for schema 2 migration to one trigger and two triggers joined by OU.
- [ ] Run `ctest --test-dir build/release -R "workspace|monitor" --output-on-failure`; expect compilation or assertions to fail before the model exists.
- [ ] Implement schema 3 read/write, schema 2 migration, readiness validation and action evaluation.
- [ ] Re-run the affected tests; commit `Migra regras para ação com condições`.

### Task 2: Editor de condições e nomes explícitos

**Files:**
- Modify: `src/app.h`
- Modify: `src/app_ui.cpp`
- Test: `tests/app_flow_tests.cpp`

**Interfaces:**
- Consumes `StatusRule::triggers` from Task 1.
- Produces direct Renomear/Excluir controls for área, status, perfil e regra.
- Produces AddTrigger/DeleteTrigger actions that preserve the action configuration.

- [ ] Write failing app-flow tests for direct renaming and adding/removing a second condition.
- [ ] Run `ctest --test-dir build/release -R app_flow --output-on-failure`; expect failure before controls and handlers exist.
- [ ] Implement the smallest master/detail condition list and direct rename controls.
- [ ] Re-run app flow tests; commit `Edita condições e nomes diretamente`.

### Task 3: Integração e entrega

**Files:**
- Modify: `docs/validacao.md`
- Modify: `docs/progresso-ux.md`

- [ ] Execute build canônico e conferir 15/15 testes.
- [ ] Fazer revisão independente de produto/arquitetura e QA do diff.
- [ ] Atualizar a validação com hash do executável e limitações do teste visual.
- [ ] Commit `Valida regras compostas`.
