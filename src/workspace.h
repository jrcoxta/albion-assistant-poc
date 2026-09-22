#pragma once
#include "health_reader.h"
#include "overlay_effect.h"
#include "model.h"
#include <filesystem>

namespace aa {
struct HudArea {
    std::wstring name;
    Region region;
    int iconSize = 48;
    bool iconCalibrated = false;
    HealthCalibration healthCalibration;
    void replaceRegion(Region selected) {
        region = selected;
        iconCalibrated = false;
        healthCalibration = {};
    }
};
struct HudLayout {
    std::wstring id, name;
    int clientWidth = 0, clientHeight = 0;
    unsigned monitorDpi = 0;
    std::wstring monitorDevice;
    std::vector<HudArea> areas;
};
struct StackSample { unsigned value = 0; std::wstring path; };
struct StatusDefinition {
    std::wstring id, name;
    bool debuff = false;
    bool builtinAssassin = false;
    std::wstring referencePath;
    std::vector<StackSample> stacks;
    std::wstring clockReferencePath;
};
enum class TriggerKind { Status = 0, Health = 1 };
enum class HealthComparison { AtMost = 0, AtLeast = 1 };
struct RuleTrigger {
    std::wstring id, statusId, sourceArea;
    Rule condition;
    std::vector<StackSample> stackSamples;
    std::wstring clockReferencePath;
    TriggerKind kind = TriggerKind::Status;
    std::wstring healthArea;
    HealthComparison healthComparison = HealthComparison::AtMost;
    unsigned healthPercent = 50;
};
struct StatusRule {
    std::wstring id, targetArea;
    Rule action;
    OverlayEffect effect = OverlayEffect::Border;
    bool followClock = false;
    std::vector<RuleTrigger> triggers;

    // Compatibility fields for schema 1/2 data and old editor code.
    // Schema 3 persists action + triggers; the first trigger mirrors these fields.
    std::wstring statusId, sourceArea;
    Rule condition;
    std::vector<StackSample> stackSamples;
    std::wstring clockReferencePath;
};
struct SetProfile { std::wstring id, name; std::vector<StatusRule> rules; };
struct Workspace {
    unsigned nextId = 1;
    int validityMs = 750;
    bool shareOverlayInCapture = false;
    std::wstring activeHudId, activeSetId;
    std::vector<HudLayout> huds;
    std::vector<StatusDefinition> statuses;
    std::vector<SetProfile> sets;
};

// Limites persistidos: 64 HUDs/status/sets, 32 areas por HUD/regras por set,
// nomes com ate 251 unidades UTF-16, IDs ate 64, stacks 1..99 (rotulos unicos).
// Entidades e regras podem ser rascunhos; readinessIssues valida a execucao.
std::wstring newId(Workspace& workspace);
bool sameName(const std::wstring& left, const std::wstring& right);
// Arquivo novo ausente: importar settings.ini e hud-profiles uma vez, sem modificá-los.
// Arquivo novo existente, mesmo sem HUDs: nunca ressuscitar dados legados.
// legacyStacks materializa contadores antigos somente nos status customizados importados.
Workspace loadWorkspace(const std::filesystem::path& file, const std::filesystem::path& legacySettings,
                        const std::vector<StackSample>& legacyStacks = {});
void saveWorkspace(const std::filesystem::path& file, const Workspace& workspace);
// Excluir somente a entidade escolhida; dependências compartilhadas permanecem.
void eraseHud(Workspace& workspace, const std::wstring& id);
void eraseSet(Workspace& workspace, const std::wstring& id);
void eraseStatus(Workspace& workspace, const std::wstring& id); // recusar enquanto houver regras dependentes
std::vector<unsigned> stackValues(const StatusDefinition& status);
// Erros concretos nas regras habilitadas do set/HUD ativos. Arquivos ausentes também bloqueiam.
std::vector<std::wstring> readinessIssues(const Workspace& workspace);
}
