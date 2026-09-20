#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "model.h"
#include <cerrno>
#include <cwchar>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace aa {
namespace {
std::optional<std::wstring> read(const std::wstring& path, const wchar_t* section,
                               const wchar_t* key, bool& valid) {
    std::wstring buffer(32768, L'\0');
    const auto size = GetPrivateProfileStringW(section, key, L"\x1", buffer.data(),
                                              static_cast<DWORD>(buffer.size()), path.c_str());
    if (size == buffer.size() - 1) { valid = false; return std::nullopt; }
    buffer.resize(size);
    if (buffer == L"\x1") return std::nullopt;
    return buffer;
}

int number(const std::wstring& path, const wchar_t* section, const wchar_t* key,
           int fallback, int minimum, int maximum, bool& valid) {
    const auto value = read(path, section, key, valid);
    if (!value) return fallback;
    wchar_t* end = nullptr;
    errno = 0;
    const auto parsed = std::wcstoll(value->c_str(), &end, 10);
    if (errno == ERANGE || end == value->c_str() || *end != L'\0' || parsed < minimum || parsed > maximum) {
        valid = false;
        return fallback;
    }
    return static_cast<int>(parsed);
}

Region region(const std::wstring& path, const wchar_t* section, int width, int height, bool& valid) {
    bool parsed = true;
    int fields = 0;
    for (const auto* key : {L"x", L"y", L"width", L"height"})
        if (read(path, section, key, parsed)) ++fields;
    if (fields != 0 && fields != 4) parsed = false;
    constexpr int maximum = std::numeric_limits<int>::max();
    Region result{number(path, section, L"x", 0, 0, maximum, parsed),
                  number(path, section, L"y", 0, 0, maximum, parsed),
                  number(path, section, L"width", 0, 0, maximum, parsed),
                  number(path, section, L"height", 0, 0, maximum, parsed)};
    const bool empty = result.x == 0 && result.y == 0 && result.width == 0 && result.height == 0;
    if (!parsed || (!empty && (!result.valid() ||
        static_cast<std::int64_t>(result.x) + result.width > width ||
        static_cast<std::int64_t>(result.y) + result.height > height))) {
        valid = false;
        return {};
    }
    return result;
}
}

Settings loadSettings(const std::wstring& path) {
    Settings result;
    if (path.empty()) return result;
    const auto fullPath = std::filesystem::absolute(path).wstring();
    bool valid = true;
    bool clientValid = true;
    result.clientWidth = number(fullPath, L"capture", L"clientWidth", 0, 0, 32768, clientValid);
    result.clientHeight = number(fullPath, L"capture", L"clientHeight", 0, 0, 32768, clientValid);
    if ((result.clientWidth == 0) != (result.clientHeight == 0)) clientValid = false;
    result.iconSize = number(fullPath, L"capture", L"iconSize", result.iconSize, 24, 256, valid);
    result.validityMs = number(fullPath, L"capture", L"validityMs", result.validityMs, 1, 60000, valid);
    result.buffs = region(fullPath, L"buffs", result.clientWidth, result.clientHeight, valid);
    result.highlight = region(fullPath, L"highlight", result.clientWidth, result.clientHeight, valid);
    if (!clientValid) {
        result.clientWidth = result.clientHeight = 0;
        result.buffs = result.highlight = {};
        valid = false;
    }
    auto& rule = result.rule;
    if (const auto value = read(fullPath, L"rule", L"name", valid)) rule.name = *value;
    if (const auto value = read(fullPath, L"rule", L"profile", valid)) rule.profile = *value;
    if (const auto value = read(fullPath, L"rule", L"referencePath", valid)) result.referencePath = *value;
    rule.enabled = number(fullPath, L"rule", L"enabled", 1, 0, 1, valid) != 0;
    rule.condition = static_cast<Condition>(number(fullPath, L"rule", L"condition",
        static_cast<int>(rule.condition), 0, 2, valid));
    rule.stacks = static_cast<unsigned>(number(fullPath, L"rule", L"stacks", rule.stacks, 0, 4, valid));
    rule.color = static_cast<std::uint32_t>(number(fullPath, L"rule", L"color", rule.color, 0, 0xFFFFFF, valid));
    rule.enabled = rule.enabled && valid;
    return result;
}

void saveSettings(const std::wstring& path, const Settings& settings) {
    if (path.empty()) throw std::runtime_error("Caminho da configuracao vazio.");
    const auto fullPath = std::filesystem::absolute(path).wstring();
    const auto temporary = fullPath + L".tmp-" + std::to_wstring(GetCurrentProcessId()) +
                           L"-" + std::to_wstring(GetTickCount64());
    const auto file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) throw std::runtime_error("Nao foi possivel criar a configuracao.");
    // O BOM faz as APIs de INI manterem os textos em UTF-16, inclusive em sistemas com outra codepage.
    const unsigned char bom[] = {0xFF, 0xFE};
    DWORD written = 0;
    const bool created = WriteFile(file, bom, sizeof(bom), &written, nullptr) && written == sizeof(bom);
    CloseHandle(file);
    try {
        if (!created) throw std::runtime_error("Nao foi possivel iniciar a configuracao Unicode.");
        const auto write = [&](const wchar_t* section, const wchar_t* key, const std::wstring& value) {
            if (!WritePrivateProfileStringW(section, key, value.c_str(), temporary.c_str()))
                throw std::runtime_error("Nao foi possivel gravar a configuracao.");
        };
        const auto writeNumber = [&](const wchar_t* section, const wchar_t* key, auto value) {
            write(section, key, std::to_wstring(value));
        };
        const auto writeText = [&](const wchar_t* key, const std::wstring& value) {
            if (value.find_first_of(L"\r\n") != std::wstring::npos || value.find(L'\0') != std::wstring::npos || value.size() > 32000)
                throw std::runtime_error("Texto de configuracao invalido.");
            write(L"rule", key, L"\"" + value + L"\"");
        };
        writeNumber(L"capture", L"clientWidth", settings.clientWidth);
        writeNumber(L"capture", L"clientHeight", settings.clientHeight);
        writeNumber(L"capture", L"iconSize", settings.iconSize);
        writeNumber(L"capture", L"validityMs", settings.validityMs);
        const auto writeRegion = [&](const wchar_t* section, const Region& value) {
            writeNumber(section, L"x", value.x); writeNumber(section, L"y", value.y);
            writeNumber(section, L"width", value.width); writeNumber(section, L"height", value.height);
        };
        writeRegion(L"buffs", settings.buffs);
        writeRegion(L"highlight", settings.highlight);
        writeText(L"name", settings.rule.name);
        writeText(L"profile", settings.rule.profile);
        writeText(L"referencePath", settings.referencePath);
        writeNumber(L"rule", L"enabled", settings.rule.enabled ? 1 : 0);
        writeNumber(L"rule", L"condition", static_cast<int>(settings.rule.condition));
        writeNumber(L"rule", L"stacks", settings.rule.stacks);
        writeNumber(L"rule", L"color", settings.rule.color);
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, temporary.c_str());
        if (!MoveFileExW(temporary.c_str(), fullPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Nao foi possivel substituir a configuracao.");
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, fullPath.c_str());
    } catch (...) {
        DeleteFileW(temporary.c_str());
        throw;
    }
}
}
