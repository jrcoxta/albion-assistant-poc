#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "profiles.h"
#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace aa {
namespace {
bool equalIgnoringCase(const std::wstring& left, const std::wstring& right) {
    return CompareStringOrdinal(left.c_str(), static_cast<int>(left.size()), right.c_str(),
                                static_cast<int>(right.size()), TRUE) == CSTR_EQUAL;
}

bool validUnicode(const std::wstring& value) {
    for (std::size_t i = 0; i < value.size(); ++i) {
        const auto code = static_cast<unsigned>(value[i]);
        if (code >= 0xD800 && code <= 0xDBFF) {
            if (++i == value.size()) return false;
            const auto low = static_cast<unsigned>(value[i]);
            if (low < 0xDC00 || low > 0xDFFF) return false;
        } else if (code >= 0xDC00 && code <= 0xDFFF) {
            return false;
        }
    }
    return true;
}

bool reservedWindowsName(const std::wstring& value) {
    const auto dot = value.find(L'.');
    auto base = value.substr(0, dot);
    while (!base.empty() && (base.back() == L' ' || base.back() == L'.')) base.pop_back();
    std::transform(base.begin(), base.end(), base.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(
                       c >= L'a' && c <= L'z' ? c - (L'a' - L'A') : c); });
    if (base == L"CON" || base == L"PRN" || base == L"AUX" || base == L"NUL" ||
        base == L"CONIN$" || base == L"CONOUT$" || base == L"CLOCK$") return true;
    if (base.size() == 4 && (base.compare(0, 3, L"COM") == 0 || base.compare(0, 3, L"LPT") == 0)) {
        const auto suffix = base[3];
        return (suffix >= L'1' && suffix <= L'9') || suffix == L'¹' || suffix == L'²' || suffix == L'³';
    }
    return false;
}

bool validHudName(const std::wstring& value) {
    if (value.empty() || value.size() > 251 || value == L"." || value == L".." ||
        value.front() == L' ' || value.back() == L' ' || value.back() == L'.' ||
        !validUnicode(value) || reservedWindowsName(value)) return false;
    for (const auto c : value)
        if (c < 32 || std::wstring_view(L"<>:\"/\\|?*").find(c) != std::wstring_view::npos)
            return false;
    return true;
}

std::optional<std::wstring> storedHudName(const std::filesystem::path& path) {
    std::wstring value(32768, L'\0');
    const auto fullPath = std::filesystem::absolute(path).wstring();
    const auto size = GetPrivateProfileStringW(L"hud", L"name", L"\x1", value.data(),
                                               static_cast<DWORD>(value.size()), fullPath.c_str());
    if (size == 0 || size == value.size() - 1) return std::nullopt;
    value.resize(size);
    if (value == L"\x1" || !validHudName(value)) return std::nullopt;
    return value;
}

bool iniExtension(const std::filesystem::path& path) {
    return equalIgnoringCase(path.extension().wstring(), L".ini");
}

std::wstring utf16Prefix(const std::wstring& value, std::size_t maximum) {
    auto result = value.substr(0, maximum);
    if (!result.empty()) {
        const auto last = static_cast<unsigned>(result.back());
        if (last >= 0xD800 && last <= 0xDBFF) result.pop_back();
    }
    return result;
}

std::wstring boundedHudName(const std::wstring& preferred, const std::wstring& suffix) {
    constexpr std::size_t maximum = 251;
    const auto available = maximum - std::min(maximum, suffix.size());
    if (preferred.size() <= available) return preferred + suffix;
    const auto separator = preferred.rfind(L" - ");
    if (separator != std::wstring::npos) {
        const auto tail = preferred.substr(separator);
        if (tail.size() < available)
            return utf16Prefix(preferred.substr(0, separator), available - tail.size()) + tail + suffix;
    }
    return utf16Prefix(preferred, available) + suffix;
}

std::filesystem::path saveHudProfileImpl(const std::filesystem::path& folder,
                                         const Settings& settings,
                                         const std::wstring* loadedHudName) {
    if (folder.empty()) throw std::invalid_argument("Pasta de perfis vazia.");
    if (!validHudName(settings.hudName)) throw std::invalid_argument("Nome de HUD invalido.");
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    if (error || !std::filesystem::is_directory(folder))
        throw std::runtime_error("Nao foi possivel criar a pasta de perfis.");

    std::filesystem::path target = folder / (settings.hudName + L".ini");
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (!entry.is_regular_file() || !iniExtension(entry.path())) continue;
        const auto stem = entry.path().stem().wstring();
        if (!equalIgnoringCase(stem, settings.hudName)) continue;
        if (stem != settings.hudName)
            throw std::invalid_argument("Ja existe um perfil com este nome usando outra capitalizacao.");
        if (loadedHudName && *loadedHudName != settings.hudName)
            throw std::invalid_argument("Ja existe outro perfil com este nome.");
        target = entry.path();
        break;
    }
    saveSettings(target.wstring(), settings);
    return target;
}
}

std::vector<HudProfile> listHudProfiles(const std::filesystem::path& folder) {
    std::vector<HudProfile> result;
    std::error_code error;
    if (!std::filesystem::is_directory(folder, error)) return result;
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        if (!entry.is_regular_file() || !iniExtension(entry.path())) continue;
        const auto name = storedHudName(entry.path());
        if (!name) continue;
        auto settings = loadSettings(entry.path().wstring());
        if (settings.hudName != *name) continue;
        result.push_back({entry.path(), std::move(settings)});
    }
    std::sort(result.begin(), result.end(), [](const HudProfile& left, const HudProfile& right) {
        return left.path.native() < right.path.native();
    });
    return result;
}

std::filesystem::path saveHudProfile(const std::filesystem::path& folder, const Settings& settings) {
    return saveHudProfileImpl(folder, settings, nullptr);
}

std::filesystem::path saveHudProfile(const std::filesystem::path& folder, const Settings& settings,
                                     const std::wstring& loadedHudName) {
    return saveHudProfileImpl(folder, settings, &loadedHudName);
}

bool hasExplicitHudName(const std::filesystem::path& path) {
    if (path.empty()) return false;
    wchar_t value[2]{};
    const auto fullPath = std::filesystem::absolute(path).wstring();
    const auto size = GetPrivateProfileStringW(L"hud", L"name", L"\x1", value, 2, fullPath.c_str());
    return size != 1 || value[0] != L'\x1';
}

std::wstring uniqueHudName(const std::vector<HudProfile>& profiles, const std::wstring& preferred) {
    const auto available = [&](const std::wstring& candidate) {
        return std::none_of(profiles.begin(), profiles.end(), [&](const HudProfile& profile) {
            return equalIgnoringCase(profile.settings.hudName, candidate);
        });
    };
    auto candidate = boundedHudName(preferred, L"");
    if (available(candidate)) return candidate;
    for (unsigned suffix = 2;; ++suffix) {
        candidate = boundedHudName(preferred, L" " + std::to_wstring(suffix));
        if (available(candidate)) return candidate;
        if (suffix == std::numeric_limits<unsigned>::max())
            throw std::runtime_error("Nao foi possivel criar um nome de HUD unico.");
    }
}

bool matchesScreen(const Settings& settings, int width, int height, unsigned dpi,
                   const std::wstring& device) {
    if (width <= 0 || height <= 0 || settings.clientWidth != width || settings.clientHeight != height)
        return false;
    if (settings.monitorDpi != 0 && dpi != 0 && settings.monitorDpi != dpi) return false;
    if (!settings.monitorDevice.empty() && !device.empty() && settings.monitorDevice != device) return false;
    return true;
}
}
