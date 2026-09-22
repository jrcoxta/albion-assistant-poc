#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "workspace.h"
#include "profiles.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace {
int checks = 0;
void check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template<class F> void rejected(F action, const char* message) {
    bool failed = false;
    try { action(); } catch (const std::exception&) { failed = true; }
    check(failed, message);
}
struct TemporaryDirectory {
    inline static unsigned sequence = 0;
    std::filesystem::path path;
    explicit TemporaryDirectory(ULONGLONG tick = GetTickCount64()) : path(std::filesystem::temp_directory_path() /
        (L"albion-workspace-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(tick) + L"-" + std::to_wstring(++sequence))) {
        if (!std::filesystem::create_directory(path)) throw std::runtime_error("pasta temporaria de teste ja existe");
    }
    ~TemporaryDirectory() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
};
void temporaryIsolation() {
    const auto tick = GetTickCount64();
    TemporaryDirectory first(tick);
    {
        TemporaryDirectory second(tick);
        check(first.path != second.path, "temporarios criados no mesmo instante compartilham pasta");
    }
    check(std::filesystem::exists(first.path), "limpeza de um temporario removeu outro ainda em uso");
}
std::string bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void ini(const std::filesystem::path& path, const wchar_t* section, const wchar_t* key, const wchar_t* value) {
    check(WritePrivateProfileStringW(section, key, value, path.c_str()) != FALSE, "alteracao da fixture INI");
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, path.c_str());
}
aa::Settings legacy(const std::wstring& name) {
    aa::Settings s;
    s.hudName = name; s.clientWidth = 1920; s.clientHeight = 1080;
    s.monitorDevice = L"\\\\.\\DISPLAY1"; s.monitorDpi = 144;
    s.iconCalibrated = true; s.iconSize = 48;
    s.buffs = {10, 20, 300, 80}; s.highlight = {900, 700, 60, 60};
    return s;
}
aa::Workspace populated(const std::filesystem::path& directory) {
    aa::Workspace w;
    const auto reference = directory / L"referência 漢字.png";
    const auto sample = directory / L"contador 7.png";
    std::ofstream(reference) << "fixture"; std::ofstream(sample) << "fixture";
    aa::HudLayout hud;
    hud.id = aa::newId(w); hud.name = L"HUD ação 漢字 🗡";
    hud.clientWidth = 1920; hud.clientHeight = 1080; hud.monitorDpi = 144;
    hud.monitorDevice = L"\\\\.\\DISPLAY1";
    hud.areas = {{L"Buffs", {10, 20, 300, 80}, 48, true}, {L"Destino", {900, 700, 60, 60}, 48, false}};
    w.huds.push_back(hud); w.activeHudId = hud.id;
    hud.id = aa::newId(w); hud.name = L"Monitor 34"; hud.clientWidth = 3440;
    hud.areas[0].region.x = 111; hud.areas[1].region.x = 1500;
    w.huds.push_back(hud);
    aa::StatusDefinition first;
    first.id = aa::newId(w); first.name = L"Espírito Assassino"; first.builtinAssassin = true;
    first.stacks = {{7, sample.wstring()}};
    w.statuses.push_back(first);
    aa::StatusDefinition second;
    second.id = aa::newId(w); second.name = L"Veneno 🧪"; second.debuff = true;
    second.referencePath = reference.wstring(); second.stacks = {{7, sample.wstring()}};
    w.statuses.push_back(second);
    aa::SetProfile set;
    set.id = aa::newId(w); set.name = L"Set " L"\"ação\"";
    aa::StatusRule rule;
    rule.id = aa::newId(w); rule.statusId = first.id;
    rule.sourceArea = L"buffs"; rule.targetArea = L"Destino";
    rule.condition.name = L"Pronto: 3"; rule.condition.profile.clear(); rule.effect = aa::OverlayEffect::Glow;
    set.rules.push_back(rule);
    rule.id = aa::newId(w); rule.statusId = second.id;
    rule.condition.name = L"Veneno ausente"; rule.condition.condition = aa::Condition::Absent;
    rule.condition.color = 0x123456; rule.effect = aa::OverlayEffect::Border;
    set.rules.push_back(rule);
    w.sets.push_back(set); w.activeSetId = set.id;
    set.id = aa::newId(w); set.name = L"Somente veneno"; set.rules.erase(set.rules.begin());
    set.rules[0].id = aa::newId(w); w.sets.push_back(set);
    return w;
}
void roundtripAndIsolation() {
    TemporaryDirectory directory;
    const auto file = directory.path / L"workspace.ini";
    auto w = populated(directory.path);
    w.shareOverlayInCapture = true;
    aa::saveWorkspace(file, w);
    const auto content = bytes(file);
    check(content.size() > 2 && static_cast<unsigned char>(content[0]) == 0xFF &&
          static_cast<unsigned char>(content[1]) == 0xFE, "persistencia usa UTF-16 LE");
    auto loaded = aa::loadWorkspace(file, directory.path / L"settings.ini");
    check(loaded.huds.size() == 2 && loaded.statuses.size() == 2 && loaded.sets.size() == 2,
          "roundtrip preserva todas colecoes");
    check(loaded.shareOverlayInCapture, "preferencia de compartilhamento do overlay nao foi preservada");
    check(loaded.huds[0].name == L"HUD ação 漢字 🗡" && loaded.sets[0].name == L"Set \"ação\"" &&
          loaded.statuses[1].name == L"Veneno 🧪", "roundtrip Unicode e aspas sem truncamento");
    check(loaded.huds[1].areas[0].region.x == 111 && loaded.huds[1].clientWidth == 3440 &&
          loaded.huds[0].monitorDpi == 144 && loaded.huds[0].areas[0].iconCalibrated,
          "coordenadas e calibracoes separadas por HUD");
    check(loaded.sets[0].rules.size() == 2 && loaded.sets[0].rules[0].effect == aa::OverlayEffect::Glow &&
          loaded.sets[0].rules[1].condition.condition == aa::Condition::Absent &&
          loaded.sets[0].rules[1].condition.color == 0x123456 &&
          loaded.statuses[1].referencePath == w.statuses[1].referencePath && loaded.statuses[1].stacks.empty(),
          "roundtrip preserva regras e referencia sem carregar metadados obsoletos do status");
    check(aa::readinessIssues(loaded).empty(), "modelo calibrado pronto com duas regras");
    auto longText = w;
    longText.statuses[1].referencePath = std::wstring(32000, L'x');
    aa::saveWorkspace(directory.path / L"texto-longo.ini", longText);
    check(aa::loadWorkspace(directory.path / L"texto-longo.ini", {}).statuses[1].referencePath == longText.statuses[1].referencePath,
          "maior texto aceito retorna integralmente sem truncamento nativo");
    loaded.activeHudId = loaded.huds[1].id;
    check(loaded.activeSetId == w.activeSetId && aa::readinessIssues(loaded).empty(),
          "trocar HUD reutiliza set por nomes sem alterar selecao do set");
    loaded.activeSetId = loaded.sets[1].id;
    check(loaded.huds[1].areas[0].region.x == 111 && loaded.activeHudId == loaded.huds[1].id,
          "trocar set nao altera HUD ou suas coordenadas");
    rejected([&] { aa::eraseStatus(loaded, loaded.statuses[1].id); }, "status usado por qualquer set nao pode ser excluido");
    const auto hudId = loaded.activeHudId;
    aa::eraseHud(loaded, hudId);
    check(loaded.activeHudId.empty() && loaded.huds.size() == 1 && loaded.statuses.size() == 2 && loaded.sets.size() == 2,
          "excluir HUD limpa somente HUD e selecao ativa");
    const auto setId = loaded.activeSetId;
    aa::eraseSet(loaded, setId);
    check(loaded.activeSetId.empty() && loaded.statuses.size() == 2 && loaded.huds.size() == 1,
          "excluir set preserva biblioteca e HUDs");
    const auto finalSet = loaded.sets[0].id;
    aa::eraseSet(loaded, finalSet);
    const auto statusId = loaded.statuses[1].id;
    aa::eraseStatus(loaded, statusId);
    check(loaded.statuses.size() == 1, "status sem dependencias pode ser excluido");
    const auto finalHud = loaded.huds[0].id;
    aa::eraseHud(loaded, finalHud);
    aa::saveSettings((directory.path / L"settings.ini").wstring(), legacy(L"Legada"));
    aa::saveWorkspace(file, loaded);
    loaded = aa::loadWorkspace(file, directory.path / L"settings.ini");
    check(loaded.huds.empty() && loaded.sets.empty() && loaded.statuses.size() == 1,
          "ultima HUD e set nao ressuscitam com legado presente ao reiniciar");
    check(aa::newId(loaded) != w.huds[0].id, "contador de IDs persiste apos exclusoes");
}
void shapeCompatibility() {
    TemporaryDirectory directory;
    const auto file = directory.path / L"workspace.ini";
    aa::saveWorkspace(file, populated(directory.path));
    ini(file, L"hud.0.area.1", L"shape", L"1");
    const auto loaded = aa::loadWorkspace(file, {});
    check(loaded.huds[0].areas[1].region.shape == aa::RegionShape::Circle, "formato circular nao carregou");
    aa::saveWorkspace(file, loaded);
    const auto roundtrip = aa::loadWorkspace(file, {});
    check(roundtrip.huds[0].areas[1].region.shape == aa::RegionShape::Circle && roundtrip.sets[0].rules[0].id == loaded.sets[0].rules[0].id,
          "circulo perdeu forma ou regra ao reabrir");
    ini(file, L"hud.0.area.1", L"shape", nullptr);
    check(aa::loadWorkspace(file, {}).huds[0].areas[1].region.shape == aa::RegionShape::Rectangle, "area legada nao assumiu retangulo");
    for (const auto value : {L"2", L"-1", L"", L"x"}) {
        ini(file, L"hud.0.area.1", L"shape", value);const auto intact = bytes(file);
        rejected([&] { (void)aa::loadWorkspace(file, {}); }, "formato invalido foi aceito");
        check(bytes(file) == intact, "formato invalido sobrescreveu arquivo salvo");
    }
    aa::saveWorkspace(file, loaded);const auto intact = bytes(file);
    auto invalid = loaded;invalid.huds[0].areas[1].region.height = 61;
    rejected([&] { aa::saveWorkspace(file, invalid); }, "circulo nao quadrado foi gravado");
    check(bytes(file) == intact, "circulo invalido sobrescreveu arquivo");
    invalid = loaded;invalid.huds[0].areas[1].region = {0,0,0,0,static_cast<aa::RegionShape>(5)};
    rejected([&] { aa::saveWorkspace(file, invalid); }, "rascunho com formato invalido foi gravado");
    ini(file, L"hud.0.area.1", L"height", L"61");
    rejected([&] { (void)aa::loadWorkspace(file, {}); }, "circulo nao quadrado foi lido");
}
void clockCompatibility() {
    TemporaryDirectory directory;
    const auto file = directory.path / L"workspace.ini";
    auto legacyWorkspace=populated(directory.path);
    legacyWorkspace.statuses[0].stacks={{3,L"amostra-legada-3.png"}};
    legacyWorkspace.statuses[0].clockReferencePath=L"relogio.png";
    legacyWorkspace.sets[0].rules[0].followClock=true;
    aa::saveWorkspace(file, legacyWorkspace);
    ini(file,L"workspace",L"schema",L"1");
    ini(file,L"status.0",L"debuff",L"0");
    ini(file,L"status.0",L"stackCount",L"1");
    ini(file,L"status.0.stack.0",L"value",L"3");
    ini(file,L"status.0.stack.0",L"path",L"\"amostra-legada-3.png\"");
    ini(file,L"status.1",L"debuff",L"0");
    ini(file,L"status.1",L"stackCount",L"0");
    for(const auto& section:{L"set.0.rule.0",L"set.0.rule.1",L"set.1.rule.0"}) {
        ini(file,section,L"clockReferencePath",nullptr);
        ini(file,section,L"stackCount",nullptr);
    }
    ini(file, L"set.0.rule.0", L"followClock", L"1");
    ini(file, L"status.0", L"clockReferencePath", L"\"relogio.png\"");
    const auto loaded = aa::loadWorkspace(file, {});
    check(loaded.sets[0].rules[0].condition.name == L"Pronto: 3", "relogio alterou regra existente");
    check(loaded.sets[0].rules[0].followClock && loaded.sets[0].rules[0].clockReferencePath == L"relogio.png", "opcao/referencia do relogio nao migrou para regra");
    check(loaded.sets[0].rules[0].stackSamples.size()==1 && loaded.sets[0].rules[0].stackSamples[0].value==3 &&
          loaded.sets[0].rules[0].stackSamples[0].path==L"amostra-legada-3.png", "amostra legada nao migrou para regra");
    aa::saveWorkspace(file, loaded);
    const auto roundtrip=aa::loadWorkspace(file, {});
    check(roundtrip.sets[0].rules[0].followClock && roundtrip.sets[0].rules[0].clockReferencePath==loaded.sets[0].rules[0].clockReferencePath &&
          roundtrip.huds[0].areas[0].region.x==loaded.huds[0].areas[0].region.x, "relogio nao persiste ou altera HUD");
    ini(file, L"set.0.rule.0", L"followClock", nullptr);
    ini(file, L"status.0", L"clockReferencePath", nullptr);
    const auto legacy=aa::loadWorkspace(file, {});
    check(!legacy.sets[0].rules[0].followClock && legacy.statuses[0].clockReferencePath.empty(), "workspace antigo habilitou relogio sozinho");
    for(const auto value:{L"2",L"-1",L"x",L""}) {
        ini(file,L"set.0.rule.0",L"followClock",value);const auto intact=bytes(file);
        rejected([&]{(void)aa::loadWorkspace(file,{});},"opcao invalida de relogio aceita");
        check(bytes(file)==intact,"opcao invalida sobrescreveu arquivo");
    }
}
void effectCompatibility() {
    TemporaryDirectory directory;
    const auto file = directory.path / L"workspace.ini";
    aa::saveWorkspace(file, populated(directory.path));
    ini(file, L"set.0.rule.0", L"effect", L"2");
    const auto loaded = aa::loadWorkspace(file, {});
    check(loaded.sets[0].rules[0].condition.name == L"Pronto: 3" && loaded.sets[0].rules[0].effect == aa::OverlayEffect::Pulse,
          "efeito explicito nao prevalece sobre brilho legado");
    for (int effect = 0; effect <= 3; ++effect) {
        auto changed = loaded; changed.sets[0].rules[0].effect = static_cast<aa::OverlayEffect>(effect);
        aa::saveWorkspace(file, changed);
        const auto restored = aa::loadWorkspace(file, {});
        check(restored.sets[0].rules[0].effect == changed.sets[0].rules[0].effect &&
              restored.sets[0].rules[1].condition.color == 0x123456 && restored.huds[1].areas[0].region.x == 111,
              "efeito nao persiste ou altera HUD/cor de outra regra");
    }
    ini(file, L"set.0.rule.0", L"effect", nullptr);
    ini(file, L"set.0.rule.0", L"glow", L"1");
    check(aa::loadWorkspace(file, {}).sets[0].rules[0].effect == aa::OverlayEffect::Glow, "brilho legado nao migra");
    ini(file, L"set.0.rule.0", L"glow", L"0");
    check(aa::loadWorkspace(file, {}).sets[0].rules[0].effect == aa::OverlayEffect::Border, "borda legada nao migra");
    for (const auto value : {L"4", L"-1", L"x", L""}) {
        ini(file, L"set.0.rule.0", L"effect", value);
        const auto intact = bytes(file);
        rejected([&] { (void)aa::loadWorkspace(file, {}); }, "efeito invalido foi aceito");
        check(bytes(file) == intact, "leitura invalida sobrescreveu workspace");
    }
    aa::saveWorkspace(file, loaded);const auto intact = bytes(file);
    auto invalid = loaded;invalid.sets[0].rules[0].effect = static_cast<aa::OverlayEffect>(4);
    rejected([&] { aa::saveWorkspace(file, invalid); }, "gravacao aceitou efeito invalido");
    check(bytes(file) == intact, "gravacao invalida alterou workspace salvo");
    ini(file, L"set.0.rule.0", L"glow", L"2");
    rejected([&] { (void)aa::loadWorkspace(file, {}); }, "efeito novo ocultou campo legado invalido");
}
void invalidData() {
    TemporaryDirectory directory;
    const auto file = directory.path / L"workspace.ini";
    const auto original = populated(directory.path);
    aa::saveWorkspace(file, original);
    const auto intact = bytes(file);
    auto invalid = original; invalid.huds[1].name = L"hud AÇÃO 漢字 🗡";
    rejected([&] { aa::saveWorkspace(file, invalid); }, "duplicatas de nome sem diferenca de caixa recusadas");
    check(bytes(file) == intact, "save invalido preserva arquivo anterior");
    invalid = original; invalid.statuses[1].id = invalid.statuses[0].id;
    rejected([&] { aa::saveWorkspace(file, invalid); }, "IDs duplicados recusados");
    invalid = original; invalid.huds[0].areas[1].name = L"BUFFS";
    rejected([&] { aa::saveWorkspace(file, invalid); }, "nomes de area duplicados recusados");
    invalid = original; invalid.statuses[1].name = std::wstring(252, L'x');
    rejected([&] { aa::saveWorkspace(file, invalid); }, "nome longo nao e truncado");
    invalid = original; invalid.huds[0].areas[0].region.width = 4000;
    rejected([&] { aa::saveWorkspace(file, invalid); }, "regiao fora da HUD recusada");
    invalid = original; invalid.sets[0].rules[0].statusId = L"nao-existe";
    rejected([&] { aa::saveWorkspace(file, invalid); }, "referencia de status quebrada recusada");
    invalid = original; invalid.statuses[0].stacks.push_back(invalid.statuses[0].stacks[0]);
    rejected([&] { aa::saveWorkspace(file, invalid); }, "rotulos duplicados recusados");
    invalid = original; invalid.huds[0].name = std::wstring(1, static_cast<wchar_t>(0xD800));
    rejected([&] { aa::saveWorkspace(file, invalid); }, "surrogate UTF-16 incompleto recusado");
    const auto locked = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(locked != INVALID_HANDLE_VALUE, "fixture bloqueia substituicao do destino");
    rejected([&] { aa::saveWorkspace(file, original); }, "falha de substituicao atomica e reportada");
    CloseHandle(locked);
    check(bytes(file) == intact, "falha atomica preserva todos os bytes anteriores");
    check(std::none_of(std::filesystem::directory_iterator(directory.path), std::filesystem::directory_iterator{},
        [](const auto& entry) { return entry.path().filename().wstring().find(L".tmp-") != std::wstring::npos; }),
        "falha atomica limpa somente seu arquivo temporario");
    const struct { const wchar_t* section; const wchar_t* key; const wchar_t* value; } corruptions[] = {
        {L"workspace", L"schema", L"3"}, {L"workspace", L"hudCount", L"-1"},
        {L"workspace", L"hudCount", L"1"}, {L"workspace", L"statusCount", L"65"},
        {L"workspace", L"validityMs", L"750x"}, {L"workspace", L"activeHudId", L"missing"},
        {L"hud.0", L"name", nullptr}, {L"hud.0.area.0", L"iconCalibrated", L"2"},
        {L"set.0.rule.0", L"condition", L"9"}, {L"set.0.rule.0", L"stacks", L"100"},
        {L"set.0.rule.0", L"color", L"16777216"}, {L"status.0.stack.0", L"value", L"0"}
    };
    for (const auto& c : corruptions) {
        aa::saveWorkspace(file, original); ini(file, c.section, c.key, c.value);
        const auto corrupt = bytes(file);
        rejected([&] { (void)aa::loadWorkspace(file, {}); }, "arquivo invalido gera erro de leitura");
        check(bytes(file) == corrupt, "erro de leitura nunca sobrescreve arquivo");
    }
    aa::saveWorkspace(file, original);
    { std::ofstream append(file, std::ios::binary | std::ios::app); const std::wstring duplicate = L"\r\n[workspace]\r\nschema=1\r\n";
      append.write(reinterpret_cast<const char*>(duplicate.data()), static_cast<std::streamsize>(duplicate.size() * sizeof(wchar_t))); }
    rejected([&] { (void)aa::loadWorkspace(file, {}); }, "secoes repetidas nao sao silenciosamente mescladas");
    aa::saveWorkspace(file, original);
    { std::ofstream append(file, std::ios::binary | std::ios::app); const std::wstring extra = L"\r\n[secao-desconhecida]\r\n";
      append.write(reinterpret_cast<const char*>(extra.data()), static_cast<std::streamsize>(extra.size() * sizeof(wchar_t))); }
    rejected([&] { (void)aa::loadWorkspace(file, {}); }, "secao desconhecida vazia tambem e recusada");
    { std::ofstream broken(file, std::ios::binary | std::ios::trunc); broken << "[workspace]\nschema=1\n"; }
    rejected([&] { (void)aa::loadWorkspace(file, {}); }, "workspace sem encoding e esquema completos recusado");
}
void migration() {
    TemporaryDirectory directory;
    const auto activePath = directory.path / L"settings.ini";
    const auto file = directory.path / L"workspace.ini";
    auto active = legacy(L"Notebook"); active.rule.profile = L"Mesmo set"; active.rule.stacks = 3;
    aa::saveSettings(activePath.wstring(), active);
    auto old = legacy(L"Monitor"); old.rule.profile = L"Mesmo set"; old.rule.stacks = 2;
    const auto oldPath = aa::saveHudProfile(directory.path / L"hud-profiles", old);
    const auto savedActivePath = aa::saveHudProfile(directory.path / L"hud-profiles", active);
    const auto activeBytes = bytes(activePath), oldBytes = bytes(oldPath), savedBytes = bytes(savedActivePath);
    auto w = aa::loadWorkspace(file, activePath);
    check(std::filesystem::exists(file), "primeira abertura grava marcador da migracao");
    check(w.huds.size() == 2 && w.sets.size() == 2 && w.statuses.size() == 1,
          "migracao deduplica HUD igual e preserva regras divergentes com mesmo nome");
    const auto hud = std::find_if(w.huds.begin(), w.huds.end(), [&](const auto& h) { return h.id == w.activeHudId; });
    const auto set = std::find_if(w.sets.begin(), w.sets.end(), [&](const auto& s) { return s.id == w.activeSetId; });
    check(hud != w.huds.end() && hud->name == L"Notebook" && set != w.sets.end() && set->rules[0].condition.stacks == 3,
          "configuracao ativa determina HUD e set ativos");
    check(!aa::sameName(w.sets[0].name, w.sets[1].name) && w.sets[0].rules[0].condition.stacks != w.sets[1].rules[0].condition.stacks,
          "regras divergentes ganham nomes unicos sem sobrescrita");
    check(bytes(activePath) == activeBytes && bytes(oldPath) == oldBytes && bytes(savedActivePath) == savedBytes,
          "migracao deixa todos os legados intactos");
    check(w.statuses[0].builtinAssassin && aa::stackValues(w.statuses[0]) == std::vector<unsigned>({2, 3}),
          "migracao conserva preset embutido apenas no status migrado");
    TemporaryDirectory empty;
    const auto emptyFile = empty.path / L"workspace.ini";
    w = aa::loadWorkspace(emptyFile, empty.path / L"settings.ini");
    check(w.huds.empty() && w.statuses.empty() && w.sets.empty() && std::filesystem::exists(emptyFile),
          "primeira abertura vazia persiste vazio sem preset automatico");
    aa::saveSettings((empty.path / L"settings.ini").wstring(), legacy(L"Chegou depois"));
    check(aa::loadWorkspace(emptyFile, empty.path / L"settings.ini").huds.empty(),
          "workspace vazio existente nunca reimporta legado");
    TemporaryDirectory custom;
    auto customSettings = legacy(L"Custom"); customSettings.referencePath = (custom.path / L"icone.png").wstring();
    std::ofstream(std::filesystem::path(customSettings.referencePath)) << "fixture";
    aa::saveSettings((custom.path / L"settings.ini").wstring(), customSettings);
    const auto importedCustom = aa::loadWorkspace(custom.path / L"workspace.ini", custom.path / L"settings.ini");
    check(importedCustom.statuses[0].referencePath == customSettings.referencePath &&
          !importedCustom.statuses[0].builtinAssassin && aa::stackValues(importedCustom.statuses[0]).empty() &&
          importedCustom.sets[0].rules[0].condition.stacks == 3 && !aa::readinessIssues(importedCustom).empty(),
          "referencia custom migrada preserva regra e exige cadastrar seus proprios stacks");
    const auto two = custom.path / L"legado-2.png", three = custom.path / L"legado-3.png";
    std::ofstream(two) << "fixture"; std::ofstream(three) << "fixture";
    const auto explicitCustom = aa::loadWorkspace(custom.path / L"workspace-explicit.ini", custom.path / L"settings.ini",
        {{2, two.wstring()}, {3, three.wstring()}});
    check(!explicitCustom.statuses[0].builtinAssassin && explicitCustom.statuses[0].referencePath == customSettings.referencePath,
          "importacao custom perdeu identidade do status");
    check(explicitCustom.sets[0].rules[0].stackSamples.size()==2 && explicitCustom.sets[0].rules[0].stackSamples[1].path==three.wstring(),
          "contador legado nao foi salvo na regra");
    check(aa::readinessIssues(explicitCustom).empty(), "amostra legada valida deixou regra pendente");
    check(aa::loadWorkspace(custom.path / L"workspace-explicit.ini", custom.path / L"settings.ini").sets[0].rules[0].stackSamples.size() == 2,
          "amostra explicita persiste sem precisar de reimportacao");
    TemporaryDirectory names;
    auto whitespace = legacy(L"  Notebook antigo  ");
    whitespace.rule.name = L"  Regra antiga  "; whitespace.rule.profile = std::wstring(250, L'A');
    aa::saveSettings((names.path / L"settings.ini").wstring(), whitespace);
    std::filesystem::create_directory(names.path / L"hud-profiles");
    auto longName = legacy(std::wstring(250, L'N') + L"🗡 nome ultrapassa limite");
    longName.rule.name = std::wstring(300, L'R'); longName.rule.profile = whitespace.rule.profile; longName.rule.stacks = 2;
    aa::saveSettings((names.path / L"hud-profiles" / L"primeira.ini").wstring(), longName);
    longName.highlight.x += 10;
    aa::saveSettings((names.path / L"hud-profiles" / L"segunda.ini").wstring(), longName);
    const auto normalized = aa::loadWorkspace(names.path / L"workspace.ini", names.path / L"settings.ini");
    check(normalized.huds.size() == 3 && normalized.sets.size() == 2 && normalized.huds[0].name == L"Notebook antigo" &&
          normalized.sets[0].rules[0].condition.name == L"Regra antiga", "migracao normaliza nomes antes aceitos sem perder entidades");
    check(normalized.huds[1].name.size() <= 251 && normalized.huds[2].name.size() <= 251 &&
          !aa::sameName(normalized.huds[1].name, normalized.huds[2].name) && normalized.sets[1].name.size() <= 251 &&
          normalized.sets[1].rules[0].condition.name.size() <= 251,
          "nomes longos e colisao reservam espaco para sufixo sem quebrar Unicode");
    for(const auto condition:{aa::Condition::Present,aa::Condition::Absent,aa::Condition::StacksEqual}) {
        TemporaryDirectory zero;
        auto previous=legacy(L"Contador legado zero");previous.rule.stacks=0;previous.rule.condition=condition;previous.rule.enabled=true;
        aa::saveSettings((zero.path/L"settings.ini").wstring(),previous);
        const auto migrated=aa::loadWorkspace(zero.path/L"workspace.ini",zero.path/L"settings.ini");
        check(migrated.sets[0].rules[0].condition.enabled==(condition!=aa::Condition::StacksEqual)&&migrated.sets[0].rules[0].condition.stacks==1,
              "zero legado so desabilita condicao de contagem; presenca/ausencia preservadas");
    }
}
bool mentions(const std::vector<std::wstring>& issues, const std::wstring& fragment) {
    return std::any_of(issues.begin(), issues.end(), [&](const auto& issue) { return issue.find(fragment) != std::wstring::npos; });
}
void readiness() {
    TemporaryDirectory directory;
    auto w = populated(directory.path);
    check(aa::stackValues(w.statuses[0]) == std::vector<unsigned>({2, 3, 7}) &&
          aa::stackValues(w.statuses[1]) == std::vector<unsigned>({7}), "stacks por status sem heranca oculta");
    w.sets[0].rules[0].condition.stacks = 5;
    w.sets[0].rules[1].sourceArea = L"Inexistente";
    auto issues = aa::readinessIssues(w);
    check(mentions(issues, L"Pronto: 3") && mentions(issues, L"Veneno ausente"),
          "readiness relata falhas em todas regras habilitadas");
    w = populated(directory.path); w.sets[0].rules[0].condition.stacks = 7;
    std::filesystem::remove(w.statuses[0].stacks[0].path);
    check(!aa::readinessIssues(w).empty(), "amostra cadastrada ausente bloqueia inicio");
    w = populated(directory.path); w.statuses[1].stacks.clear();
    check(aa::readinessIssues(w).empty(), "presenca e ausencia funcionam sem amostras de stacks");
    w.statuses[1].referencePath.clear();
    check(mentions(aa::readinessIssues(w), L"Veneno ausente"), "status generico sem referencia bloqueia");
    w.sets[0].rules[1].condition.enabled = false;
    check(aa::readinessIssues(w).empty(), "regra desabilitada nao bloqueia inicio");
    w.huds[0].areas[0].iconCalibrated = false;
    check(mentions(aa::readinessIssues(w), L"Pronto: 3"), "origem nao calibrada bloqueia leitura");
    w = populated(directory.path); w.sets[0].rules[0].sourceArea.clear(); w.sets[0].rules[0].statusId.clear();
    aa::saveWorkspace(directory.path / L"draft.ini", w);
    check(!aa::readinessIssues(aa::loadWorkspace(directory.path / L"draft.ini", {})).empty(),
          "rascunho pode ser salvo mas nao executado");
}
}
int main() {
    try { temporaryIsolation(); roundtripAndIsolation(); shapeCompatibility(); effectCompatibility(); clockCompatibility(); invalidData(); migration(); readiness(); }
    catch (const std::exception& error) { std::cerr << "FALHOU: " << error.what() << '\n'; return 1; }
    std::cout << checks << " verificacoes de workspace, 0 falhas\n";
    return 0;
}
