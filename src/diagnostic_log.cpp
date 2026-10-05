#include "diagnostic_log.h"
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>

namespace diagnostic_log {
namespace {
std::mutex guard;
std::filesystem::path file;
bool writable = false;
bool recorded = false;
std::string lastNativeStage;
unsigned long lastNativeCode = 0;
ULONGLONG lastNativeAt = 0;
constexpr std::uintmax_t maxBytes = 1024 * 1024;

void append(const char* stage, const char* kind, unsigned long code) noexcept {
    try {
        std::lock_guard lock(guard);
        if (!writable || !stage) return;
        // Recusar tokens não controlados por nós impede caminhos e dados de usuários.
        for (const char* c = stage; *c; ++c)
            if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '.' || *c == '_')) return;
        if (kind && std::string(stage) == lastNativeStage && code == lastNativeCode &&
            GetTickCount64() - lastNativeAt < 60000) return;
        std::error_code ec;
        if (std::filesystem::exists(file, ec) && std::filesystem::file_size(file, ec) >= maxBytes) {
            const auto previous = file.parent_path() / L"assistant.previous.log";
            // Substituição atômica: se falhar, os dois arquivos existentes ficam intactos.
            if (!MoveFileExW(file.c_str(), previous.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                recorded = false;
                return;
            }
        }
        SYSTEMTIME now{};
        GetSystemTime(&now);
        char timestamp[40]{};
        std::snprintf(timestamp, sizeof(timestamp), "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
            now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);
        std::ofstream output(file, std::ios::app | std::ios::binary);
        if (!output) { recorded = false; return; }
        output << timestamp << " pid=" << GetCurrentProcessId() << " " << stage;
        if (kind) output << " " << kind << "=" << code;
        output << '\n';
        output.flush();
        recorded = static_cast<bool>(output);
        if (recorded && kind) {
            lastNativeStage = stage;
            lastNativeCode = code;
            lastNativeAt = GetTickCount64();
        }
        if (recorded && std::string(stage) == "capture.recovered") lastNativeStage.clear();
    } catch (...) {
        std::lock_guard lock(guard);
        recorded = false;
    }
}
}

void configure(const std::filesystem::path& directory) noexcept {
    try {
        std::lock_guard lock(guard);
        writable = false;
        recorded = false;
        lastNativeStage.clear();
        file.clear();
        if (directory.empty()) return;
        const auto folder = directory / L"logs";
        std::filesystem::create_directories(folder);
        file = folder / L"assistant.log";
        writable = true;
    } catch (...) {
        std::lock_guard lock(guard);
        writable = false;
    }
}
void event(const char* stage) noexcept { append(stage, nullptr, 0); }
void win32(const char* stage, DWORD code) noexcept { append(stage, "win32", code); }
void hresult(const char* stage, HRESULT code) noexcept { append(stage, "hresult", static_cast<unsigned long>(code)); }
void filesystem(const char* stage, int code) noexcept { append(stage, "fs", static_cast<unsigned long>(code)); }
void detail(const char* stage, unsigned value) noexcept { append(stage, "id", value); }
std::wstring location() {
    std::lock_guard lock(guard);
    return writable && recorded ? file.wstring() : std::wstring{};
}
}
