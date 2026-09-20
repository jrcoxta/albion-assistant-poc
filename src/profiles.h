#pragma once
#include "model.h"
#include <filesystem>
#include <string>
#include <vector>

namespace aa {
struct HudProfile {
    std::filesystem::path path;
    Settings settings;
};

std::vector<HudProfile> listHudProfiles(const std::filesystem::path& folder);
std::filesystem::path saveHudProfile(const std::filesystem::path& folder, const Settings& settings);
std::filesystem::path saveHudProfile(const std::filesystem::path& folder, const Settings& settings,
                                     const std::wstring& loadedHudName);
bool hasExplicitHudName(const std::filesystem::path& path);
std::wstring uniqueHudName(const std::vector<HudProfile>& profiles, const std::wstring& preferred);
bool matchesScreen(const Settings& settings, int width, int height, unsigned dpi,
                   const std::wstring& device);
}
