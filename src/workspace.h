#pragma once
#include "overlay_effect.h"
#include "model.h"
#include <filesystem>

namespace aa {
struct HudArea {
    std::wstring name;
    Region region;
    int iconSize = 48;
    bool iconCalibrated = false;
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
};
struct StatusRule {
    std::wstring id, statusId;
    std::wstring sourceArea, targetArea;
    Rule condition;
    OverlayEffect effect = OverlayEffect::Border;
};
struct SetProfile { std::wstring id, name; std::vector<StatusRule> rules; };
struct Workspace {
    unsigned nextId = 1;
    int validityMs = 750;
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
