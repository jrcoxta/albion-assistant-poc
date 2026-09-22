# Condição por porcentagem de vida Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Acionar o destaque de uma regra quando uma barra de vida calibrada atingir um limiar percentual.

**Architecture:** A calibração pertence à área da HUD, pois a posição muda por tela. RuleTrigger ganha uma fonte Vida; ela participa do mesmo OU e compartilha a ação da regra. Um leitor de vida usa a captura atual e retorna uma fração ou estado desconhecido.

**Tech Stack:** C++20, Win32, D3D11 capture, CMake/Ninja e testes C++ nativos.

**Spec:** docs/superpowers/specs/2026-09-22-condicao-vida-design.md

## Global Constraints

- Windows x64, sem dependências externas ou OCR.
- Um único executável em dist/AlbionAssistant.exe.
- Leitura visual apenas; não envia entradas ao jogo.
- Barras horizontais que esvaziam da direita para a esquerda.
- Leitura incerta nunca aciona a regra.
- Schema 4 preserva e migra schemas 1, 2 e 3.

---

### Task 1: Medidor de barra

**Files:**

- Create: src/health_reader.h
- Create: src/health_reader.cpp
- Modify: CMakeLists.txt
- Create: tests/health_reader_tests.cpp

**Interfaces:**

- Produces HealthCalibration calibrateHealth(const Image&).
- Produces std::optional<float> readHealthFraction(const Image&, const HealthCalibration&).

- [ ] **Step 1: Write the failing test**

~~~
const auto calibration=aa::calibrateHealth(healthImage(1.0f,true));
require(calibration.valid(),"calibração da barra cheia falhou");
for(const auto [input,expected]:std::array{{.48f,.48f},{.49f,.49f},{.50f,.50f}})
    require(near(*aa::readHealthFraction(healthImage(input,true),calibration),expected,.02f),"percentual incorreto");
require(!aa::readHealthFraction(aa::Image{},calibration),"imagem incerta produziu percentual");
~~~

- [ ] **Step 2: Verify RED**

Run: cmake --build build/release --target health_reader_tests --config Release && build\release\health_reader_tests.exe

Expected: FAIL because the medidor does not exist.

- [ ] **Step 3: Write the minimal implementation**

~~~
struct HealthCalibration { int x=0,y=0,width=0,height=0; float hue=0, minimumSaturation=0; bool valid() const; };
HealthCalibration calibrateHealth(const Image& full);
std::optional<float> readHealthFraction(const Image& image,const HealthCalibration& calibration);
~~~

Find the widest saturated red horizontal run in the full reference. At read time inspect three saved rows, discard white and low-saturation pixels, use the median of the rightmost contiguous fill columns, and return nullopt unless two rows agree.

- [ ] **Step 4: Verify GREEN**

Run: cmake --build build/release --target health_reader_tests --config Release && build\release\health_reader_tests.exe

Expected: PASS for 48%, 49%, 50%, full health, text over the bar and invalid image.

- [ ] **Step 5: Commit**

~~~
git add CMakeLists.txt src/health_reader.h src/health_reader.cpp tests/health_reader_tests.cpp
git commit -m "Mede percentual de vida em barra calibrada"
~~~

### Task 2: Persistência e motor de regras

**Files:**

- Modify: src/model.h
- Modify: src/workspace.h
- Modify: src/workspace.cpp
- Modify: src/monitor.h
- Modify: src/monitor.cpp
- Test: tests/workspace_tests.cpp
- Test: tests/monitor_tests.cpp

**Interfaces:**

- Produces HudArea::healthCalibration.
- Produces RuleTrigger::kind, healthArea, healthComparison and healthPercent.
- Produces a tagged monitor reader for status or health.

- [ ] **Step 1: Write the failing tests**

~~~
trigger.kind=aa::TriggerKind::Health;
trigger.healthArea=L"Vida inimigo";
trigger.healthComparison=aa::HealthComparison::AtMost;
trigger.healthPercent=49;
require(aa::evaluateMonitor(plan,atHealth(.49f),1000,750,1)[0],"49% nao ativou");
require(!aa::evaluateMonitor(plan,atHealth(.50f),1010,750,1)[0],"50% ativou limite 49%");
require(restored.huds[0].areas[0].healthCalibration.valid(),"calibração não persistiu");
~~~

- [ ] **Step 2: Verify RED**

Run: ctest --test-dir build/release -R "workspace|monitor" --output-on-failure

Expected: FAIL because health trigger and calibration do not exist.

- [ ] **Step 3: Write the minimal implementation**

Add schema 4 and read schemas 1–3 unchanged. Serialize calibration on each HUD area and health fields on each trigger. Add health areas to the union capture rectangle. Unknown health stays inactive. Keep per-trigger hysteresis: <=49 releases above 51; >=49 releases below 47.

- [ ] **Step 4: Verify GREEN**

Run: ctest --test-dir build/release -R "workspace|monitor|health_reader" --output-on-failure

Expected: PASS for persistence, status OU vida, 48–51 thresholds and unknown health.

- [ ] **Step 5: Commit**

~~~
git add src/model.h src/workspace.h src/workspace.cpp src/monitor.h src/monitor.cpp tests/workspace_tests.cpp tests/monitor_tests.cpp
git commit -m "Integra condição de vida ao monitor"
~~~

### Task 3: Calibração e editor

**Files:**

- Modify: src/app.h
- Modify: src/app.cpp
- Modify: src/app_ui.cpp
- Test: tests/app_flow_tests.cpp

**Interfaces:**

- Produces CalibrateHealth, que captura a área selecionada e conserva a calibração anterior se falhar.
- Produces um editor de gatilho Status ou Vida.

- [ ] **Step 1: Write the failing app-flow test**

~~~
choose(app,TriggerKindBox,1);
chooseText(app,HealthArea,L"Vida inimigo");
choose(app,HealthComparisonBox,0);
setText(app.item(HealthPercent),L"49");
app.saveEditor();
require(app.rule()->triggers[0].kind==aa::TriggerKind::Health&&app.rule()->triggers[0].healthPercent==49,"editor nao salvou vida");
~~~

- [ ] **Step 2: Verify RED**

Run: ctest --test-dir build/release -R app_flow --output-on-failure

Expected: FAIL because controls and command do not exist.

- [ ] **Step 3: Write the minimal implementation**

Show Calibrar vida cheia beside Medir ícone for a saved rectangular area. Capture it with the existing game-capture guard, call calibrateHealth, and show a concrete failure without replacing old data. In a Vida trigger hide status, stacks and clock; show area, operator and 1–100%.

- [ ] **Step 4: Verify GREEN**

Run: ctest --test-dir build/release -R app_flow --output-on-failure

Expected: PASS for saving, failed recalibration and a separate status condition remaining intact.

- [ ] **Step 5: Commit**

~~~
git add src/app.h src/app.cpp src/app_ui.cpp tests/app_flow_tests.cpp
git commit -m "Configura condições por vida no painel"
~~~

### Task 4: Entrega

**Files:**

- Modify: docs/validacao.md

- [ ] **Step 1: Execute the canonical build**

Run: powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1

Expected: all tests pass and dist/AlbionAssistant.exe is verified.

- [ ] **Step 2: Capture UI evidence**

Run: build\release\app_flow_tests.exe build\ux-health-20260922

Expected: a UI image shows the compact health condition without status controls.

- [ ] **Step 3: Record validation and commit**

Record the automated result and the real-game check: calibrate at full health and observe a depleted bar.

~~~
git add docs/validacao.md
git commit -m "Valida condição de vida"
~~~
