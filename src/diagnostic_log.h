#pragma once
#include <filesystem>
#include <string>
#include <windows.h>

// Apenas eventos de vocabulário fixo e códigos nativos entram no log: nunca
// nomes de perfis, caminhos do workspace, pixels ou mensagens arbitrárias.
namespace diagnostic_log {
void configure(const std::filesystem::path& directory) noexcept;
void event(const char* stage) noexcept;
void win32(const char* stage, DWORD code) noexcept;
void hresult(const char* stage, HRESULT code) noexcept;
void filesystem(const char* stage, int code) noexcept;
void detail(const char* stage, unsigned value) noexcept;
std::wstring location();
}
