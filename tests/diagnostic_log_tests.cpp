#include "diagnostic_log.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <iostream>

int main() {
    const auto folder = std::filesystem::temp_directory_path() /
        (L"albion-diagnostic-test-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()));
    try {
        diagnostic_log::configure(folder);
        diagnostic_log::event("startup.version_0.1.0");
        diagnostic_log::win32("overlay.apply_capture_policy", ERROR_ACCESS_DENIED);
        diagnostic_log::hresult("capture.api_failure", E_ACCESSDENIED);
        diagnostic_log::event("my_private_path:C:\\Users\\Pessoa\\workspace.ini");
        const auto path = folder / L"logs" / L"assistant.log";
        const auto read = [](const auto& file) {
            std::ifstream stream(file, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        };
        const auto text = read(path);
        if (text.find("startup.version_0.1.0") == std::string::npos ||
            text.find("overlay.apply_capture_policy win32=5") == std::string::npos ||
            text.find("capture.api_failure hresult=2147942405") == std::string::npos ||
            text.find("Users") != std::string::npos || text.find("workspace.ini") != std::string::npos)
            throw std::runtime_error("Registro não preservou códigos ou vazou caminho privado");
        { std::ofstream prefill(path, std::ios::binary | std::ios::app); prefill << std::string(1024 * 1024, 'x'); }
        diagnostic_log::event("session.start_requested");
        if (read(path).find("session.start_requested") == std::string::npos ||
            !std::filesystem::exists(folder / L"logs" / L"assistant.previous.log"))
            throw std::runtime_error("Rotação do log não preservou evento e arquivo anterior");
        const auto previous = folder / L"logs" / L"assistant.previous.log";
        const auto first = read(previous);
        { std::ofstream prefill(path, std::ios::binary | std::ios::app); prefill << std::string(1024 * 1024, 'y'); }
        const auto locked = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (locked == INVALID_HANDLE_VALUE) throw std::runtime_error("Não foi possível bloquear log de teste");
        diagnostic_log::event("session.stopped");
        const bool preserved = read(previous) == first && read(path).find("session.stopped") == std::string::npos;
        CloseHandle(locked);
        if (!preserved) throw std::runtime_error("Rotação falha perdeu arquivo anterior ou excedeu limite");
        diagnostic_log::event("session.stopped");
        if (read(path).find("session.stopped") == std::string::npos || read(previous).find("session.start_requested") == std::string::npos)
            throw std::runtime_error("Falha transitória não permitiu recuperar rotação");
        const auto obstruction = folder / L"blocked";
        { std::ofstream blocked(obstruction); blocked << "não é diretório"; }
        diagnostic_log::configure(obstruction);
        diagnostic_log::event("startup.begin");
        if (!diagnostic_log::location().empty()) throw std::runtime_error("Logger indisponível alegou gravar log");
        diagnostic_log::configure({});
        std::filesystem::remove_all(folder);
        std::cout << "Log local de falhas, códigos, privacidade e rotação aprovados\n";
        return 0;
    } catch (const std::exception& error) {
        diagnostic_log::configure({});
        std::filesystem::remove_all(folder);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
