#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "model.h"
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
int checks = 0;
int failures = 0;
void check(bool result, const char* description) {
    ++checks;
    if (!result) { ++failures; std::cerr << "FALHOU: " << description << '\n'; }
}

aa::Observation present(unsigned stacks = 3) {
    return {{aa::Presence::Present, stacks, 1.0f, {}, {}}, 1000, 7};
}

void ruleTests() {
    aa::Rule rule;
    check(aa::evaluate(rule, present(), 1000, 750, 7), "preset aciona com exatamente 3 stacks");
    for (unsigned wanted = 0; wanted <= 4; ++wanted) {
        rule.stacks = wanted;
        for (unsigned observed = 0; observed <= 4; ++observed)
            check(aa::evaluate(rule, present(observed), 1000, 750, 7) == (wanted > 0 && wanted == observed),
                  "stacks compara igualdade e rejeita contador zero");
    }
    rule.stacks = 3;
    auto reading = present();
    reading.detection.stacks.reset();
    check(!aa::evaluate(rule, reading, 1000, 750, 7), "sem contador nao infere stacks");
    rule.condition = aa::Condition::Present;
    check(aa::evaluate(rule, reading, 1000, 750, 7), "presenca independe de contador legivel");
    rule.condition = aa::Condition::Absent;
    check(!aa::evaluate(rule, reading, 1000, 750, 7), "presente sem numero nao significa ausente");

    for (auto condition : {aa::Condition::StacksEqual, aa::Condition::Present, aa::Condition::Absent}) {
        rule.condition = condition;
        reading = present();
        if (condition == aa::Condition::Absent) {
            reading.detection.presence = aa::Presence::Absent;
            reading.detection.stacks.reset();
        }
        check(aa::evaluate(rule, reading, 1749, 750, 7), "leitura recente satisfaz cada condicao");
        check(!aa::evaluate(rule, reading, 1750, 750, 7), "750 ms ja expira todas as condicoes");
        check(!aa::evaluate(rule, reading, 1751, 750, 7), "751 ms apaga todas as condicoes");
        check(!aa::evaluate(rule, reading, 999, 750, 7), "captura futura invalida todas as condicoes");
        check(!aa::evaluate(rule, reading, 1000, 0, 7), "validade zero invalida todas as condicoes");
        check(!aa::evaluate(rule, reading, 1000, -1, 7), "validade negativa invalida todas as condicoes");
        check(!aa::evaluate(rule, reading, 1000, 750, 8), "outra fonte invalida todas as condicoes");
        rule.enabled = false;
        check(!aa::evaluate(rule, reading, 1000, 750, 7), "regra desativada nunca aciona");
        rule.enabled = true;
        reading.detection.presence = aa::Presence::Unknown;
        check(!aa::evaluate(rule, reading, 1000, 750, 7), "desconhecido nao satisfaz nem ausencia");
        reading.detection.presence = static_cast<aa::Presence>(99);
        check(!aa::evaluate(rule, reading, 1000, 750, 7), "estado de presenca invalido nao aciona");
    }
    rule.condition = aa::Condition::StacksEqual;
    reading = present();
    reading.source = 0;
    check(!aa::evaluate(rule, reading, 1000, 750, 0), "fonte nao inicializada e invalida mesmo quando igual");
    reading = present();
    reading.capturedMs = 0;
    check(!aa::evaluate(rule, reading, 1, 750, 7), "captura nao inicializada nao aciona");
    reading.capturedMs = -1;
    check(!aa::evaluate(rule, reading, 1, 750, 7), "timestamp negativo nao aciona");
    reading.capturedMs = std::numeric_limits<std::int64_t>::min();
    check(!aa::evaluate(rule, reading, std::numeric_limits<std::int64_t>::max(), 750, 7), "timestamp extremo nao causa overflow e falso positivo");
    rule.condition = static_cast<aa::Condition>(99);
    check(!aa::evaluate(rule, present(), 1000, 750, 7), "condicao invalida nao aciona");
    rule.condition = aa::Condition::StacksEqual;
    rule.stacks = 5;
    check(aa::evaluate(rule, present(5), 1000, 750, 7), "contador generico 5 aciona");
    rule.stacks = 99;
    check(aa::evaluate(rule, present(99), 1000, 750, 7), "contador generico 99 aciona");
    rule.stacks = 100;
    check(!aa::evaluate(rule, present(100), 1000, 750, 7), "contador fora de 1..99 nao aciona");
}

struct TemporaryIni {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        (L"albion-configuração-" + std::to_wstring(GetCurrentProcessId()) + L".ini");
    TemporaryIni() { std::filesystem::remove(path); }
    ~TemporaryIni() { std::error_code ignored; std::filesystem::remove(path, ignored); }
};

bool sameRegion(const aa::Region& a, const aa::Region& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

void settingsTests() {
    TemporaryIni file;
    const auto missing = aa::loadSettings(file.path.wstring());
    check(missing.rule.enabled && missing.rule.condition == aa::Condition::StacksEqual && missing.rule.stacks == 3,
          "arquivo ausente oferece preset de 3 stacks");
    check(!missing.buffs.valid() && !missing.highlight.valid(), "arquivo ausente exige calibracao");
    check(missing.validityMs == 750 && missing.iconSize == 48, "arquivo ausente preserva defaults de leitura");
    check(missing.hudName == L"Minha HUD" && missing.monitorDevice.empty() &&
          missing.monitorDpi == 0 && !missing.iconCalibrated,
          "arquivo ausente usa metadados neutros e calibracao pendente");

    aa::Settings original;
    original.hudName = L"Minha HUD ultrawide 漢字";
    original.monitorDevice = L"\\\\.\\DISPLAY2";
    original.monitorDpi = 144;
    original.iconCalibrated = true;
    original.clientWidth = 1920; original.clientHeight = 1080;
    original.buffs = {11, 23, 401, 70}; original.highlight = {901, 750, 53, 58};
    original.iconSize = 64; original.validityMs = 900;
    original.rule.name = L" Espírito \"Assassino\" — ação 漢字 🗡 ";
    original.rule.profile = L"Mortíficos / ação";
    original.rule.condition = aa::Condition::Absent;
    original.rule.stacks = 4; original.rule.color = 0x12ABEF;
    original.referencePath = L"C:\\Referências\\Espírito 漢字.png";
    aa::saveSettings(file.path.wstring(), original);
    const auto loaded = aa::loadSettings(file.path.wstring());
    check(loaded.hudName == original.hudName && loaded.monitorDevice == original.monitorDevice &&
          loaded.monitorDpi == original.monitorDpi && loaded.iconCalibrated,
          "metadados do perfil e calibracao persistem");
    check(loaded.rule.name == original.rule.name && loaded.rule.profile == original.rule.profile,
          "nome e perfil preservam Unicode, aspas e espacos");
    check(loaded.referencePath == original.referencePath, "caminho de referencia preserva Unicode");
    check(sameRegion(loaded.buffs, original.buffs) && sameRegion(loaded.highlight, original.highlight), "regioes persistem integralmente");
    check(loaded.clientWidth == 1920 && loaded.clientHeight == 1080 && loaded.iconSize == 64 && loaded.validityMs == 900,
          "calibracao e validade persistem");
    check(loaded.rule.enabled && loaded.rule.condition == aa::Condition::Absent && loaded.rule.stacks == 4 && loaded.rule.color == 0x12ABEF,
          "campos da regra persistem");
    auto uncalibrated = original;
    uncalibrated.iconCalibrated = false;
    aa::saveSettings(file.path.wstring(), uncalibrated);
    check(!aa::loadSettings(file.path.wstring()).iconCalibrated,
          "flag explicita de icone pendente prevalece sobre regioes completas");
    original.rule.enabled = false;
    original.rule.condition = aa::Condition::Present;
    aa::saveSettings(file.path.wstring(), original);
    const auto disabled = aa::loadSettings(file.path.wstring());
    check(!disabled.rule.enabled && disabled.rule.condition == aa::Condition::Present, "sobrescrita preserva desativacao e nova condicao");
    original.rule.enabled = true;
    auto smallestIcon = original;
    smallestIcon.iconSize = 24;
    aa::saveSettings(file.path.wstring(), smallestIcon);
    const auto minimum = aa::loadSettings(file.path.wstring());
    check(minimum.rule.enabled && minimum.iconSize == 24, "diametro minimo de 24 pixels permanece valido");

    struct InvalidValue { const wchar_t* section; const wchar_t* key; const wchar_t* value; };
    const InvalidValue invalids[] = {
        {L"rule", L"condition", L"99"}, {L"rule", L"condition", L"abc"},
        {L"rule", L"enabled", L"2"}, {L"rule", L"stacks", L"-1"}, {L"rule", L"stacks", L"5"},
        {L"rule", L"color", L"16777216"}, {L"capture", L"validityMs", L"0"},
        {L"capture", L"validityMs", L"60001"}, {L"capture", L"iconSize", L"7"},
        {L"capture", L"iconSize", L"8"}, {L"capture", L"iconSize", L"23"},
        {L"capture", L"iconSize", L"257"}, {L"capture", L"clientWidth", L"-1920"},
        {L"capture", L"clientHeight", L"1080garbage"}, {L"capture", L"clientWidth", L"999999999999999999999"},
        {L"buffs", L"x", L"-1"}, {L"buffs", L"width", L"0"},
        {L"buffs", L"x", nullptr}, {L"highlight", L"y", nullptr},
        {L"highlight", L"height", L"-1"}, {L"highlight", L"x", L"1900"},
        {L"buffs", L"width", L"2147483647"}
    };
    for (const auto& bad : invalids) {
        aa::saveSettings(file.path.wstring(), original);
        if (!WritePrivateProfileStringW(bad.section, bad.key, bad.value, file.path.c_str()))
            throw std::runtime_error("Falha ao preparar configuracao invalida");
        const auto invalid = aa::loadSettings(file.path.wstring());
        check(!invalid.rule.enabled, "configuracao invalida desativa a regra conservadoramente");
        if (std::wstring(bad.section) == L"buffs") check(!invalid.buffs.valid(), "buffs invalidos nao sao corrigidos por clamp");
        if (std::wstring(bad.section) == L"highlight") check(!invalid.highlight.valid(), "destaque invalido nao e corrigido por clamp");
        if (std::wstring(bad.key) == L"clientWidth" || std::wstring(bad.key) == L"clientHeight")
            check(!invalid.buffs.valid() && !invalid.highlight.valid(), "cliente invalido invalida ambas regioes");
    }
    aa::saveSettings(file.path.wstring(), original);
    WritePrivateProfileStringW(L"buffs", L"x", L"2147483647", file.path.c_str());
    WritePrivateProfileStringW(L"buffs", L"width", L"2147483647", file.path.c_str());
    check(!aa::loadSettings(file.path.wstring()).buffs.valid(), "soma extrema de coordenadas nao transborda");

    aa::saveSettings(file.path.wstring(), original);
    WritePrivateProfileStringW(L"hud", L"name", nullptr, file.path.c_str());
    WritePrivateProfileStringW(L"hud", L"monitorDevice", nullptr, file.path.c_str());
    WritePrivateProfileStringW(L"hud", L"monitorDpi", nullptr, file.path.c_str());
    WritePrivateProfileStringW(L"hud", L"iconCalibrated", nullptr, file.path.c_str());
    const auto legacy = aa::loadSettings(file.path.wstring());
    check(legacy.hudName == L"Minha HUD" && legacy.monitorDevice.empty() && legacy.monitorDpi == 0,
          "settings legado recebe metadados neutros sem perder compatibilidade");
    check(legacy.iconCalibrated && sameRegion(legacy.buffs, original.buffs) &&
          sameRegion(legacy.highlight, original.highlight) && legacy.iconSize == original.iconSize,
          "settings legado com diametro e regioes completas preserva calibracao");
    WritePrivateProfileStringW(L"capture", L"iconSize", L"invalido", file.path.c_str());
    check(!aa::loadSettings(file.path.wstring()).iconCalibrated,
          "diametro legado invalido nao e confundido com calibracao existente");
    bool saveFailed = false;
    try { aa::saveSettings((file.path / L"inexistente.ini").wstring(), original); }
    catch (const std::exception&) { saveFailed = true; }
    check(saveFailed, "erro de gravacao e informado ao chamador");
}
}

int main() {
    try { ruleTests(); settingsTests(); }
    catch (const std::exception& error) { ++failures; std::cerr << "ERRO: " << error.what() << '\n'; }
    std::cout << checks << " verificacoes, " << failures << " falhas\n";
    return failures == 0 ? 0 : 1;
}
