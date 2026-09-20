#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "profiles.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
int checks = 0;
int failures = 0;

void check(bool result, const char* description) {
    ++checks;
    if (!result) { ++failures; std::cerr << "FALHOU: " << description << '\n'; }
}

struct TemporaryDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        (L"albion-perfis-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
         std::to_wstring(GetTickCount64()));
    TemporaryDirectory() { std::filesystem::create_directory(path); }
    ~TemporaryDirectory() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
};

aa::Settings settings(const std::wstring& name, int width = 1920, int height = 1080) {
    aa::Settings result;
    result.hudName = name;
    result.clientWidth = width;
    result.clientHeight = height;
    result.monitorDevice = L"\\\\.\\DISPLAY1";
    result.monitorDpi = 144;
    result.iconCalibrated = true;
    result.buffs = {10, 20, 300, 80};
    result.highlight = {900, 700, 60, 60};
    result.iconSize = 64;
    return result;
}

void persistenceTests() {
    TemporaryDirectory directory;
    const auto notebook = settings(L"Notebook ação 漢字 🗡");
    const auto wide = settings(L"Monitor 34", 3440, 1440);
    const auto notebookPath = aa::saveHudProfile(directory.path, notebook);
    const auto widePath = aa::saveHudProfile(directory.path, wide);

    check(notebookPath.parent_path() == directory.path && notebookPath.extension() == L".ini",
          "perfil usa a pasta solicitada e extensao INI");
    check(notebookPath.stem() == notebook.hudName, "nome Unicode e usado sem mutilacao");
    check(std::filesystem::exists(notebookPath) && std::filesystem::exists(widePath),
          "salvar outro perfil nao apaga o anterior");

    const auto profiles = aa::listHudProfiles(directory.path);
    check(profiles.size() == 2, "dois perfis salvos permanecem listados");
    bool foundNotebook = false;
    bool foundWide = false;
    for (const auto& profile : profiles) {
        if (profile.settings.hudName == notebook.hudName) {
            foundNotebook = profile.path == notebookPath && profile.settings.monitorDevice == notebook.monitorDevice &&
                profile.settings.monitorDpi == 144 && profile.settings.iconCalibrated;
        }
        if (profile.settings.hudName == wide.hudName)
            foundWide = profile.path == widePath && profile.settings.clientWidth == 3440;
    }
    check(foundNotebook, "perfil Unicode recarrega metadados da tela e calibracao");
    check(foundWide, "segundo perfil recarrega suas dimensoes");

    auto updated = notebook;
    updated.iconSize = 80;
    check(aa::saveHudProfile(directory.path, updated) == notebookPath,
          "mesmo nome exato atualiza o mesmo arquivo");
    check(aa::listHudProfiles(directory.path).size() == 2 &&
          aa::loadSettings(notebookPath.wstring()).iconSize == 80,
          "atualizacao nao cria copia nem remove outro perfil");
}

void validationTests() {
    TemporaryDirectory directory;
    aa::saveHudProfile(directory.path, settings(L"Notebook"));
    bool collisionRejected = false;
    try { aa::saveHudProfile(directory.path, settings(L"NOTEBOOK")); }
    catch (const std::invalid_argument&) { collisionRejected = true; }
    check(collisionRejected, "colisao sem diferenca de caixa nao sobrescreve outro perfil");
    check(aa::listHudProfiles(directory.path).size() == 1, "colisao preserva o perfil existente");

    const std::wstring invalidNames[] = {
        L"", L" ", L".", L"..", L"../fora", L"..\\fora", L"pasta/nome", L"pasta\\nome",
        L"CON", L"con.txt", L"CON .txt", L"PRN", L"AUX", L"NUL", L"COM1", L"com9.dados",
        L"COM¹", L"COM².txt", L"COM³", L"LPT1", L"LPT9", L"LPT¹", L"LPT².txt", L"LPT³",
        L"nome.", L"nome ", L"a<b", L"a>b", L"a:b", L"a\"b", L"a|b", L"a?b", L"a*b",
        std::wstring(1, static_cast<wchar_t>(1))
    };
    for (const auto& name : invalidNames) {
        bool rejected = false;
        try { aa::saveHudProfile(directory.path, settings(name)); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "nome vazio, reservado, traversal ou invalido e recusado");
    }
    check(aa::listHudProfiles(directory.path).size() == 1,
          "nomes recusados nao deixam arquivos parciais");

    const auto unnamed = directory.path / L"sem-nome.ini";
    aa::saveSettings(unnamed.wstring(), aa::Settings{});
    WritePrivateProfileStringW(L"hud", L"name", nullptr, unnamed.c_str());
    auto corrupt = settings(L"Corrompido");
    aa::saveSettings((directory.path / L"corrompido.ini").wstring(), corrupt);
    WritePrivateProfileStringW(L"hud", L"name", L"..\\fora", (directory.path / L"corrompido.ini").c_str());
    std::ofstream(directory.path / L"ignorar.txt") << "nao e perfil";
    check(aa::listHudProfiles(directory.path).size() == 1,
          "listagem ignora arquivo sem nome valido, corrompido e extensao diferente");
    check(aa::listHudProfiles(directory.path / L"inexistente").empty(),
          "pasta de perfis ainda inexistente produz lista vazia");

    const auto monitorPath = aa::saveHudProfile(directory.path, settings(L"Monitor"));
    auto renamed = settings(L"Monitor");
    renamed.iconSize = 96;
    bool otherProfileRejected = false;
    try { aa::saveHudProfile(directory.path, renamed, L"Notebook"); }
    catch (const std::invalid_argument&) { otherProfileRejected = true; }
    check(otherProfileRejected && aa::loadSettings(monitorPath.wstring()).iconSize == 64,
          "renomear perfil carregado nao sobrescreve outro perfil existente");

    auto notebookUpdate = settings(L"Notebook");
    notebookUpdate.iconSize = 72;
    aa::saveHudProfile(directory.path, notebookUpdate, L"Notebook");
    check(aa::loadSettings((directory.path / L"Notebook.ini").wstring()).iconSize == 72,
          "nome original exato continua permitindo atualizar o perfil carregado");

    const auto names = aa::listHudProfiles(directory.path);
    check(aa::uniqueHudName(names, L"Outra tela") == L"Outra tela",
          "nome preferido livre permanece inalterado");
    aa::saveHudProfile(directory.path, settings(L"Outra tela"));
    aa::saveHudProfile(directory.path, settings(L"Outra tela 2"));
    check(aa::uniqueHudName(aa::listHudProfiles(directory.path), L"outra tela") == L"outra tela 3",
          "nome unico considera caixa e sufixos existentes");

    const auto activePath = directory.path / L"ativo.ini";
    aa::saveSettings(activePath.wstring(), settings(L"Ativa"));
    check(aa::hasExplicitHudName(activePath), "settings novo informa que possui nome de HUD explicito");
    WritePrivateProfileStringW(L"hud", L"name", nullptr, activePath.c_str());
    check(!aa::hasExplicitHudName(activePath), "settings legado sem chave de nome e identificado");

    std::wstring longName(238, L'a');
    longName += L"🗡";
    const std::wstring screenSuffix = L" - 3440x1440";
    const auto bounded = aa::uniqueHudName({}, longName + screenSuffix);
    const bool validUtf16 = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, bounded.data(),
        static_cast<int>(bounded.size()), nullptr, 0, nullptr, nullptr) > 0;
    check(bounded.size() <= 251 && bounded.ends_with(screenSuffix) && validUtf16,
          "nome longo preserva sufixo da tela sem exceder limite ou quebrar surrogate");
}

void compatibilityTests() {
    const auto profile = settings(L"Monitor");
    check(aa::matchesScreen(profile, 1920, 1080, 144, L"\\\\.\\DISPLAY1"),
          "tela com dimensoes, DPI e dispositivo iguais e compativel");
    check(!aa::matchesScreen(profile, 1919, 1080, 144, L"\\\\.\\DISPLAY1") &&
          !aa::matchesScreen(profile, 1920, 1079, 144, L"\\\\.\\DISPLAY1"),
          "qualquer dimensao diferente e incompatível");
    check(!aa::matchesScreen(profile, 1920, 1080, 120, L"\\\\.\\DISPLAY1"),
          "DPI conhecido diferente e incompativel");
    check(!aa::matchesScreen(profile, 1920, 1080, 144, L"\\\\.\\DISPLAY2"),
          "dispositivo conhecido diferente e incompativel");
    check(aa::matchesScreen(profile, 1920, 1080, 0, L""),
          "metadados atuais desconhecidos nao inventam incompatibilidade");

    auto legacy = profile;
    legacy.monitorDpi = 0;
    legacy.monitorDevice.clear();
    check(aa::matchesScreen(legacy, 1920, 1080, 120, L"\\\\.\\DISPLAY2"),
          "metadados ausentes no perfil legado nao impedem dimensoes iguais");
    check(!aa::matchesScreen(profile, 0, 1080, 144, L"\\\\.\\DISPLAY1"),
          "dimensoes atuais invalidas nunca combinam");
}
}

int main() {
    try { persistenceTests(); validationTests(); compatibilityTests(); }
    catch (const std::exception& error) { ++failures; std::cerr << "ERRO: " << error.what() << '\n'; }
    std::cout << checks << " verificacoes, " << failures << " falhas\n";
    return failures == 0 ? 0 : 1;
}
