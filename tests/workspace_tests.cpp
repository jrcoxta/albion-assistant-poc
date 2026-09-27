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
template<class W> auto& linked(W&& w,std::size_t set,std::size_t index) {
    const auto& id=w.sets.at(set).rules.at(index).ruleId;
    auto it=std::find_if(w.rules.begin(),w.rules.end(),[&](const auto& r){return r.id==id;});
    if(it==w.rules.end())throw std::runtime_error("vinculo de teste inexistente");
    return *it;
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
    set.rules.push_back({rule.id,true});w.rules.push_back(rule);
    rule.id = aa::newId(w); rule.statusId = second.id;
    rule.condition.name = L"Veneno ausente"; rule.condition.condition = aa::Condition::Absent;
    rule.condition.color = 0x123456; rule.effect = aa::OverlayEffect::Border;
    set.rules.push_back({rule.id,true});w.rules.push_back(rule);
    w.sets.push_back(set); w.activeSetId = set.id;
    set.id = aa::newId(w); set.name = L"Somente veneno"; set.rules.erase(set.rules.begin());
    w.sets.push_back(set);
    return w;
}
// Constrói uma fixture do formato anterior a partir da mesma configuração.
// Exercita o leitor antigo sem deixar o gravador novo produzir schemas obsoletos.
void legacyFixture(const std::filesystem::path& file,const aa::Workspace& workspace,int schema=2) {
    aa::saveWorkspace(file,workspace);
    const auto copySection=[&](const std::wstring& source,const std::wstring& target) {
        std::wstring fields(65536,L'\0');
        const auto size=GetPrivateProfileSectionW(source.c_str(),fields.data(),static_cast<DWORD>(fields.size()),file.c_str());
        check(size>0&&size<fields.size()-2,"secao legada de teste ausente");
        check(WritePrivateProfileSectionW(target.c_str(),fields.c_str(),file.c_str())!=FALSE,"falha ao copiar secao legada");
    };
    for(std::size_t i=0;i<workspace.sets.size();++i)for(std::size_t j=0;j<workspace.sets[i].rules.size();++j){
        const auto& link=workspace.sets[i].rules[j];
        const auto found=std::find_if(workspace.rules.begin(),workspace.rules.end(),[&](const auto& r){return r.id==link.ruleId;});
        check(found!=workspace.rules.end(),"fixture sem regra");
        const auto source=L"rule."+std::to_wstring(found-workspace.rules.begin());
        const auto target=L"set."+std::to_wstring(i)+L".rule."+std::to_wstring(j);
        copySection(source,target);
        ini(file,target.c_str(),L"enabled",link.enabled?L"1":L"0");
        ini(file,target.c_str(),L"ruleId",nullptr);
        if(schema<5)ini(file,target.c_str(),L"onlyWhenReady",nullptr);
        if(schema<3)ini(file,target.c_str(),L"triggerCount",nullptr);
        if(i==1)ini(file,target.c_str(),L"id",L"rule-set-two");
        for(std::size_t n=0;n<found->stackSamples.size();++n)
            copySection(source+L".stack."+std::to_wstring(n),target+L".stack."+std::to_wstring(n));
        if(schema>=3)for(std::size_t n=0;n<std::max<std::size_t>(1,found->triggers.size());++n){
            const auto sourceTrigger=source+L".trigger."+std::to_wstring(n);
            const auto targetTrigger=target+L".trigger."+std::to_wstring(n);
            copySection(sourceTrigger,targetTrigger);
            if(schema<4)for(const auto key:{L"kind",L"healthArea",L"healthComparison",L"healthPercent"})
                ini(file,targetTrigger.c_str(),key,nullptr);
            if(n<found->triggers.size())for(std::size_t k=0;k<found->triggers[n].stackSamples.size();++k)
                copySection(sourceTrigger+L".stack."+std::to_wstring(k),targetTrigger+L".stack."+std::to_wstring(k));
        }
    }
    for(std::size_t i=0;i<workspace.rules.size();++i){
        const auto source=L"rule."+std::to_wstring(i);
        for(std::size_t n=0;n<workspace.rules[i].stackSamples.size();++n)
            WritePrivateProfileStringW((source+L".stack."+std::to_wstring(n)).c_str(),nullptr,nullptr,file.c_str());
        for(std::size_t n=0;n<std::max<std::size_t>(1,workspace.rules[i].triggers.size());++n){
            const auto trigger=source+L".trigger."+std::to_wstring(n);
            if(n<workspace.rules[i].triggers.size())
                for(std::size_t m=0;m<workspace.rules[i].triggers[n].stackSamples.size();++m)
                    WritePrivateProfileStringW((trigger+L".stack."+std::to_wstring(m)).c_str(),nullptr,nullptr,file.c_str());
            WritePrivateProfileStringW(trigger.c_str(),nullptr,nullptr,file.c_str());
        }
        WritePrivateProfileStringW(source.c_str(),nullptr,nullptr,file.c_str());
    }
    for(std::size_t i=0;i<workspace.huds.size();++i)for(std::size_t j=0;j<workspace.huds[i].areas.size();++j){
        const auto section=L"hud."+std::to_wstring(i)+L".area."+std::to_wstring(j);
        if(schema<5){ini(file,section.c_str(),L"readyReferencePath",nullptr);ini(file,section.c_str(),L"readyConfirmed",nullptr);}
        if(schema<4){
            for(const auto key:{L"healthCalibrated",L"healthX",L"healthY",L"healthWidth",L"healthHeight",L"healthRed",L"healthGreen",L"healthBlue"})
                ini(file,section.c_str(),key,nullptr);
        }
    }
    for(std::size_t i=0;i<workspace.statuses.size();++i){
        const auto section=L"status."+std::to_wstring(i);
        for(std::size_t j=0;j<workspace.statuses[i].stacks.size();++j)
            WritePrivateProfileStringW((section+L".stack."+std::to_wstring(j)).c_str(),nullptr,nullptr,file.c_str());
        ini(file,section.c_str(),L"stackCount",nullptr);ini(file,section.c_str(),L"clockReferencePath",nullptr);
        ini(file,section.c_str(),L"debuff",nullptr);
    }
    ini(file,L"workspace",L"ruleCount",nullptr);ini(file,L"workspace",L"schema",std::to_wstring(schema).c_str());
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
    check(linked(loaded,0,0).triggers.size() == 1 && linked(loaded,0,0).action.name == L"Pronto: 3",
          "regra salva nao foi migrada para uma acao com um gatilho");
    {
        auto ready=w;
        linked(ready,0,0).onlyWhenReady=true;
        ready.huds[0].areas[1].readyReferencePath=ready.statuses[1].referencePath;
        ready.huds[0].areas[1].readyConfirmed=true;
        const auto readyFile=directory.path/L"habilidade.ini";
        aa::saveWorkspace(readyFile,ready);
        auto restored=aa::loadWorkspace(readyFile,{});
        check(linked(restored,0,0).onlyWhenReady&&restored.huds[0].areas[1].readyConfirmed&&restored.huds[0].areas[1].readyReferencePath==ready.huds[0].areas[1].readyReferencePath,
              "referencia de habilidade e opcao por regra nao sobreviveram ao schema 5");
        check(aa::readinessIssues(restored).empty(),"habilidade calibrada foi considerada ausente");
        ini(readyFile,L"hud.0.area.1",L"readyConfirmed",nullptr);
        const auto older=aa::loadWorkspace(readyFile,{});
        check(!older.huds[0].areas[1].readyConfirmed&&!aa::readinessIssues(older).empty(),
              "schema 5 sem confirmacao deveria abrir sem presumir habilidade pronta");
        restored.huds[0].areas[1].replaceRegion({900,700,60,60});
        check(restored.huds[0].areas[1].readyReferencePath.empty()&&!aa::readinessIssues(restored).empty(),
              "reposicionar habilidade nao exigiu nova imagem");
    }
    {
        auto health=w;
        health.huds[0].areas[0].healthCalibration={10,14,180,5,190,42,28};
        auto& rule=linked(health,0,0); rule.action=rule.condition;
        aa::RuleTrigger life; life.id=L"vida-49"; life.kind=aa::TriggerKind::Health; life.healthArea=L"Buffs";
        life.healthComparison=aa::HealthComparison::AtMost; life.healthPercent=49;
        rule.triggers={life};
        const auto healthFile=directory.path/L"vida.ini";
        aa::saveWorkspace(healthFile,health);
        const auto restored=aa::loadWorkspace(healthFile,{});
        const auto& restoredLife=linked(restored,0,0).triggers[0];
        check(restored.huds[0].areas[0].healthCalibration.valid()&&restoredLife.kind==aa::TriggerKind::Health&&
              restoredLife.healthArea==L"Buffs"&&restoredLife.healthComparison==aa::HealthComparison::AtMost&&restoredLife.healthPercent==49,
              "vida calibrada ou condicao nao persistiu");
    }
    { auto composite=w; auto& rule=linked(composite,0,0); rule.action=rule.condition; aa::RuleTrigger first{L"t1",rule.statusId,rule.sourceArea,rule.condition,rule.stackSamples,rule.clockReferencePath}; aa::RuleTrigger second{L"t2",composite.statuses[1].id,L"Buffs",rule.condition,{},{}}; second.condition.condition=aa::Condition::Present; rule.triggers={first,second}; aa::saveWorkspace(directory.path/L"composite.ini",composite); const auto restored=aa::loadWorkspace(directory.path/L"composite.ini",{}); check(linked(restored,0,0).triggers.size()==2&&linked(restored,0,0).triggers[1].statusId==composite.statuses[1].id,"regra composta perdeu condicao ao reabrir"); rejected([&]{aa::eraseStatus(composite,composite.statuses[1].id);},"status usado por segunda condicao pode ser excluido"); }

    check(loaded.sets[0].rules.size() == 2 && linked(loaded,0,0).effect == aa::OverlayEffect::Glow &&
          linked(loaded,0,1).condition.condition == aa::Condition::Absent &&
          linked(loaded,0,1).condition.color == 0x123456 &&
          loaded.statuses[1].referencePath == w.statuses[1].referencePath && loaded.statuses[1].stacks.size()==1,
          "roundtrip preserva regras, referencia e amostras legadas do status");
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
    aa::eraseRule(loaded,loaded.rules[1].id);
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
    {
        aa::HudArea area{L"Vida",{10,20,240,50},48,true,{8,4,180,5,190,42,28}};
        area.replaceRegion({30,40,200,48});
        check(!area.healthCalibration.valid()&&!area.iconCalibrated,
              "mover a area preservou calibracoes da posicao anterior");
    }
    {
        auto changed=populated(directory.path);
        auto& area=changed.huds[0].areas[0];
        area.healthCalibration={8,4,180,5,190,42,28};
        area.replaceRegion({30,40,200,48});
        aa::saveWorkspace(file,changed);
        const auto restored=aa::loadWorkspace(file,{});
        check(!restored.huds[0].areas[0].healthCalibration.valid()&&!restored.huds[0].areas[0].iconCalibrated,
              "reabrir a HUD restaurou calibrações de uma região antiga");
    }
    aa::saveWorkspace(file, populated(directory.path));
    ini(file, L"hud.0.area.1", L"shape", L"1");
    const auto loaded = aa::loadWorkspace(file, {});
    check(loaded.huds[0].areas[1].region.shape == aa::RegionShape::Circle, "formato circular nao carregou");
    aa::saveWorkspace(file, loaded);
    const auto roundtrip = aa::loadWorkspace(file, {});
    check(roundtrip.huds[0].areas[1].region.shape == aa::RegionShape::Circle && roundtrip.sets[0].rules[0].ruleId == loaded.sets[0].rules[0].ruleId,
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
    legacyWorkspace.statuses[0].stacks={{2,L"amostra-legada-2.png"},{3,L"amostra-legada-3.png"}};
    legacyWorkspace.statuses[0].clockReferencePath=L"relogio.png";
    linked(legacyWorkspace,0,0).followClock=true;
    legacyFixture(file, legacyWorkspace);
    ini(file,L"workspace",L"schema",L"1");
    ini(file,L"status.0",L"debuff",L"0");
    ini(file,L"status.0",L"stackCount",L"2");
    ini(file,L"status.0.stack.0",L"value",L"2");
    ini(file,L"status.0.stack.0",L"path",L"\"amostra-legada-2.png\"");
    ini(file,L"status.0.stack.1",L"value",L"3");
    ini(file,L"status.0.stack.1",L"path",L"\"amostra-legada-3.png\"");
    ini(file,L"status.1",L"debuff",L"1");
    ini(file,L"status.1",L"stackCount",L"1");
    ini(file,L"status.1.stack.0",L"value",L"7");
    ini(file,L"status.1.stack.0",L"path",L"\"amostra-status-sem-regra.png\"");
    ini(file,L"status.1",L"clockReferencePath",L"\"relogio-sem-regra.png\"");
    for(const auto& section:{L"set.0.rule.0",L"set.0.rule.1",L"set.1.rule.0"}) {
        ini(file,section,L"clockReferencePath",nullptr);
        ini(file,section,L"stackCount",nullptr);
    }
    ini(file, L"set.0.rule.0", L"followClock", L"1");
    ini(file, L"status.0", L"clockReferencePath", L"\"relogio.png\"");
    const auto loaded = aa::loadWorkspace(file, {});
    check(linked(loaded,0,0).condition.name == L"Pronto: 3", "relogio alterou regra existente");
    check(linked(loaded,0,0).followClock && linked(loaded,0,0).clockReferencePath == L"relogio.png", "opcao/referencia do relogio nao migrou para regra");
    check(linked(loaded,0,0).stackSamples.size()==1 && linked(loaded,0,0).stackSamples[0].value==3 &&
          linked(loaded,0,0).stackSamples[0].path==L"amostra-legada-3.png", "amostra legada nao migrou para regra");
    check(loaded.statuses[0].stacks.size()==2&&loaded.statuses[1].stacks.size()==1&&
          loaded.statuses[1].debuff&&loaded.statuses[1].clockReferencePath==L"relogio-sem-regra.png",
          "schema 1 perdeu amostra ou referencia sem regra correspondente");
    aa::saveWorkspace(file,loaded);
    const auto roundtrip=aa::loadWorkspace(file, {});
    check(linked(roundtrip,0,0).followClock && linked(roundtrip,0,0).clockReferencePath==linked(loaded,0,0).clockReferencePath &&
          roundtrip.huds[0].areas[0].region.x==loaded.huds[0].areas[0].region.x&&
          roundtrip.statuses[0].stacks.size()==2&&roundtrip.statuses[1].stacks.size()==1&&
          roundtrip.statuses[1].clockReferencePath==L"relogio-sem-regra.png"&&roundtrip.statuses[1].debuff,
          "schema 6 perdeu amostra, debuff ou relogio do schema 1");
    legacyFixture(file, loaded);
    ini(file, L"set.0.rule.0", L"followClock", nullptr);
    ini(file, L"status.0", L"clockReferencePath", nullptr);
    const auto legacy=aa::loadWorkspace(file, {});
    check(!linked(legacy,0,0).followClock && legacy.statuses[0].clockReferencePath.empty(), "workspace antigo habilitou relogio sozinho");
    for(const auto value:{L"2",L"-1",L"x",L""}) {
        ini(file,L"set.0.rule.0",L"followClock",value);const auto intact=bytes(file);
        rejected([&]{(void)aa::loadWorkspace(file,{});},"opcao invalida de relogio aceita");
        check(bytes(file)==intact,"opcao invalida sobrescreveu arquivo");
    }
}
void effectCompatibility() {
    TemporaryDirectory directory;
    const auto file = directory.path / L"workspace.ini";
    legacyFixture(file, populated(directory.path));
    ini(file, L"set.0.rule.0", L"effect", L"2");
    const auto loaded = aa::loadWorkspace(file, {});
    check(linked(loaded,0,0).condition.name == L"Pronto: 3" && linked(loaded,0,0).effect == aa::OverlayEffect::Pulse,
          "efeito explicito nao prevalece sobre brilho legado");
    for (int effect = 0; effect <= 4; ++effect) {
        auto changed = loaded; linked(changed,0,0).effect = static_cast<aa::OverlayEffect>(effect);
        legacyFixture(file, changed);
        const auto restored = aa::loadWorkspace(file, {});
        check(linked(restored,0,0).effect == linked(changed,0,0).effect &&
              linked(restored,0,1).condition.color == 0x123456 && restored.huds[1].areas[0].region.x == 111,
              "efeito nao persiste ou altera HUD/cor de outra regra");
    }
    ini(file, L"set.0.rule.0", L"effect", nullptr);
    ini(file, L"set.0.rule.0", L"glow", L"1");
    check(linked(aa::loadWorkspace(file, {}),0,0).effect == aa::OverlayEffect::Glow, "brilho legado nao migra");
    ini(file, L"set.0.rule.0", L"glow", L"0");
    check(linked(aa::loadWorkspace(file, {}),0,0).effect == aa::OverlayEffect::Border, "borda legada nao migra");
    for (const auto value : {L"5", L"-1", L"x", L""}) {
        ini(file, L"set.0.rule.0", L"effect", value);
        const auto intact = bytes(file);
        rejected([&] { (void)aa::loadWorkspace(file, {}); }, "efeito invalido foi aceito");
        check(bytes(file) == intact, "leitura invalida sobrescreveu workspace");
    }
    legacyFixture(file, loaded);const auto intact = bytes(file);
    auto invalid = loaded;linked(invalid,0,0).effect = static_cast<aa::OverlayEffect>(5);
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
    invalid = original; linked(invalid,0,0).statusId = L"nao-existe";
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
    check(hud != w.huds.end() && hud->name == L"Notebook" && set != w.sets.end() && linked(w,static_cast<std::size_t>(set-w.sets.begin()),0).condition.stacks == 3,
          "configuracao ativa determina HUD e set ativos");
    check(!aa::sameName(w.sets[0].name, w.sets[1].name) && linked(w,0,0).condition.stacks != linked(w,1,0).condition.stacks,
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
          linked(importedCustom,0,0).condition.stacks == 3 && !aa::readinessIssues(importedCustom).empty(),
          "referencia custom migrada preserva regra e exige cadastrar seus proprios stacks");
    const auto two = custom.path / L"legado-2.png", three = custom.path / L"legado-3.png";
    std::ofstream(two) << "fixture"; std::ofstream(three) << "fixture";
    const auto explicitCustom = aa::loadWorkspace(custom.path / L"workspace-explicit.ini", custom.path / L"settings.ini",
        {{2, two.wstring()}, {3, three.wstring()}});
    check(!explicitCustom.statuses[0].builtinAssassin && explicitCustom.statuses[0].referencePath == customSettings.referencePath,
          "importacao custom perdeu identidade do status");
    check(linked(explicitCustom,0,0).stackSamples.size()==2 && linked(explicitCustom,0,0).stackSamples[1].path==three.wstring(),
          "contador legado nao foi salvo na regra");
    check(aa::readinessIssues(explicitCustom).empty(), "amostra legada valida deixou regra pendente");
    check(linked(aa::loadWorkspace(custom.path / L"workspace-explicit.ini", custom.path / L"settings.ini"),0,0).stackSamples.size() == 2,
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
          linked(normalized,0,0).condition.name == L"Regra antiga", "migracao normaliza nomes antes aceitos sem perder entidades");
    check(normalized.huds[1].name.size() <= 251 && normalized.huds[2].name.size() <= 251 &&
          !aa::sameName(normalized.huds[1].name, normalized.huds[2].name) && normalized.sets[1].name.size() <= 251 &&
          linked(normalized,1,0).condition.name.size() <= 251,
          "nomes longos e colisao reservam espaco para sufixo sem quebrar Unicode");
    for(const auto condition:{aa::Condition::Present,aa::Condition::Absent,aa::Condition::StacksEqual}) {
        TemporaryDirectory zero;
        auto previous=legacy(L"Contador legado zero");previous.rule.stacks=0;previous.rule.condition=condition;previous.rule.enabled=true;
        aa::saveSettings((zero.path/L"settings.ini").wstring(),previous);
        const auto migrated=aa::loadWorkspace(zero.path/L"workspace.ini",zero.path/L"settings.ini");
        check(migrated.sets[0].rules[0].enabled==(condition!=aa::Condition::StacksEqual)&&linked(migrated,0,0).condition.stacks==1,
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
    linked(w,0,0).condition.stacks = 5;
    linked(w,0,1).sourceArea = L"Inexistente";
    auto issues = aa::readinessIssues(w);
    check(mentions(issues, L"Pronto: 3") && mentions(issues, L"Veneno ausente"),
          "readiness relata falhas em todas regras habilitadas");
    w = populated(directory.path); linked(w,0,0).condition.stacks = 7;
    std::filesystem::remove(w.statuses[0].stacks[0].path);
    check(!aa::readinessIssues(w).empty(), "amostra cadastrada ausente bloqueia inicio");
    w = populated(directory.path); w.statuses[1].stacks.clear();
    check(aa::readinessIssues(w).empty(), "presenca e ausencia funcionam sem amostras de stacks");
    w.statuses[1].referencePath.clear();
    check(mentions(aa::readinessIssues(w), L"Veneno ausente"), "status generico sem referencia bloqueia");
    w.sets[0].rules[1].enabled = false;
    check(aa::readinessIssues(w).empty(), "regra desabilitada nao bloqueia inicio");
    w.huds[0].areas[0].iconCalibrated = false;
    check(mentions(aa::readinessIssues(w), L"Pronto: 3"), "origem nao calibrada bloqueia leitura");
    w = populated(directory.path); linked(w,0,0).sourceArea.clear(); linked(w,0,0).statusId.clear();
    aa::saveWorkspace(directory.path / L"draft.ini", w);
    check(!aa::readinessIssues(aa::loadWorkspace(directory.path / L"draft.ini", {})).empty(),
          "rascunho pode ser salvo mas nao executado");
}
void sharedRules() {
    TemporaryDirectory directory;
    const auto file=directory.path/L"compartilhado.ini";
    auto w=populated(directory.path);
    const auto sharedId=w.sets[0].rules[1].ruleId;
    check(w.sets[1].rules[0].ruleId==sharedId,"perfil secundario nao usa a mesma regra");
    linked(w,0,1).effect=aa::OverlayEffect::Flames;
    linked(w,0,1).condition.condition=aa::Condition::StacksEqual;
    linked(w,0,1).condition.stacks=7;
    linked(w,0,1).stackSamples={{7,w.statuses[1].stacks[0].path}};
    w.sets[0].rules[1].enabled=false;
    aa::saveWorkspace(file,w);
    auto loaded=aa::loadWorkspace(file,{});
    check(loaded.rules.size()==2&&loaded.sets[1].rules[0].ruleId==sharedId&&
          linked(loaded,1,0).effect==aa::OverlayEffect::Flames&&
          linked(loaded,1,0).triggers[0].condition.condition==aa::Condition::StacksEqual&&
          linked(loaded,1,0).triggers[0].condition.stacks==7&&
          linked(loaded,1,0).triggers[0].stackSamples[0].path==w.statuses[1].stacks[0].path&&
          !loaded.sets[0].rules[1].enabled&&loaded.sets[1].rules[0].enabled,
          "edicao compartilhada de efeito/condicao/amostra ou ativacao independente nao persistiu");
    rejected([&]{aa::eraseRule(loaded,sharedId);},"regra vinculada a perfil inativo foi excluida");
    aa::eraseSet(loaded,loaded.sets[1].id);
    check(loaded.rules.size()==2,"excluir perfil removeu regra compartilhada");
    rejected([&]{aa::eraseRule(loaded,sharedId);},"vinculo desativado deixou excluir regra");
    loaded.sets[0].rules.erase(loaded.sets[0].rules.begin()+1);
    aa::eraseRule(loaded,sharedId);
    check(loaded.rules.size()==1,"desvincular nao permitiu remover definicao");
    loaded.sets[0].rules.push_back({L"nao-existe",true});
    rejected([&]{aa::saveWorkspace(file,loaded);},"vinculo quebrado foi salvo");
    loaded.sets[0].rules.back().ruleId=loaded.sets[0].rules.front().ruleId;
    rejected([&]{aa::saveWorkspace(file,loaded);},"vinculo duplicado foi salvo");

    auto legacy=populated(directory.path);
    const auto old=directory.path/L"legacy.ini";
    legacyFixture(old,legacy);
    ini(old,L"set.1.rule.0",L"name",L"\"Veneno ausente\"");
    ini(old,L"set.1.rule.0",L"condition",L"1");
    const auto original=bytes(old);
    auto migrated=aa::loadWorkspace(old,{});
    check(bytes(old)==original&&migrated.rules.size()==3&&migrated.sets[1].rules.size()==1&&
          linked(migrated,0,1).id!=linked(migrated,1,0).id&&
          !aa::sameName(linked(migrated,0,1).condition.name,linked(migrated,1,0).condition.name),
          "migracao uniu regras antigas homonimas ou alterou o arquivo antes de salvar");
    aa::saveWorkspace(old,migrated);
    const auto restored=aa::loadWorkspace(old,{});
    check(restored.rules.size()==3&&restored.sets[0].rules.size()==2&&restored.sets[1].rules.size()==1&&
          linked(restored,1,0).condition.condition==aa::Condition::Present,
          "schema 6 perdeu dados da migracao ao reabrir");
}
void schemaHistory() {
    TemporaryDirectory directory;
    for(int schema=3;schema<=5;++schema){
        auto w=populated(directory.path);
        auto& rule=linked(w,0,0);rule.action=rule.condition;
        aa::RuleTrigger status{L"status",w.statuses[0].id,L"Buffs",rule.condition,{},L""};
        aa::RuleTrigger alternate{L"alternativa",w.statuses[1].id,L"Buffs",rule.condition,{},L""};
        alternate.condition.condition=aa::Condition::Present;
        if(schema>=4){
            alternate.kind=aa::TriggerKind::Health;alternate.healthArea=L"Buffs";
            alternate.healthComparison=aa::HealthComparison::AtMost;alternate.healthPercent=37;
            w.huds[0].areas[0].healthCalibration={10,4,180,5,190,42,28};
        }
        if(schema==5){
            rule.onlyWhenReady=true;w.huds[0].areas[1].readyReferencePath=w.statuses[1].referencePath;
            w.huds[0].areas[1].readyConfirmed=true;
        }
        rule.triggers={status,alternate};
        const auto file=directory.path/(L"schema-"+std::to_wstring(schema)+L".ini");
        legacyFixture(file,w,schema);
        const auto original=bytes(file);
        auto loaded=aa::loadWorkspace(file,{});
        check(original==bytes(file)&&linked(loaded,0,0).triggers.size()==2&&
              linked(loaded,0,0).triggers[1].kind==alternate.kind&&
              linked(loaded,0,0).triggers[1].healthPercent==alternate.healthPercent&&
              linked(loaded,0,0).onlyWhenReady==(schema==5),
              "schema historico perdeu condicao, vida ou filtro antes de salvar");
        aa::saveWorkspace(file,loaded);
        const auto restored=aa::loadWorkspace(file,{});
        check(linked(restored,0,0).triggers.size()==2&&
              linked(restored,0,0).triggers[1].kind==alternate.kind&&
              linked(restored,0,0).triggers[1].healthPercent==alternate.healthPercent&&
              linked(restored,0,0).onlyWhenReady==(schema==5)&&
              restored.huds[0].areas[1].readyConfirmed==(schema==5),
              "schema 6 nao preservou condicao, vida ou cooldown apos migrar");
    }
}
}
int main() {
    try {
        const auto run=[](const char* name,auto test){try{test();}catch(const std::exception& error){throw std::runtime_error(std::string(name)+": "+error.what());}};
        run("temporarios",temporaryIsolation);run("roundtrip",roundtripAndIsolation);
        run("formas",shapeCompatibility);run("efeitos",effectCompatibility);
        run("relogio",clockCompatibility);run("dados invalidos",invalidData);
        run("migracao",migration);run("prontidao",readiness);run("compartilhamento",sharedRules);run("schemas 3-5",schemaHistory);
    }
    catch (const std::exception& error) { std::cerr << "FALHOU: " << error.what() << '\n'; return 1; }
    std::cout << checks << " verificacoes de workspace, 0 falhas\n";
    return 0;
}
