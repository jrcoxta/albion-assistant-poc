#include "capture.h"
#include "diagnostic_log.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace aa {
namespace {
using Microsoft::WRL::ComPtr;

struct ApiError : std::runtime_error {
    explicit ApiError(const std::string& message) : std::runtime_error(message) {}
};

struct RecalibrationError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

void check(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        char stage[96] = "capture.";
        std::size_t i = sizeof("capture.") - 1;
        for (const char* source = operation; *source && i + 1 < sizeof(stage); ++source)
            stage[i++] = ((*source >= 'a' && *source <= 'z') || (*source >= 'A' && *source <= 'Z') ||
                          (*source >= '0' && *source <= '9') || *source == '_') ? *source : '_';
        stage[i] = '\0';
        diagnostic_log::hresult(stage, result);
        char code[16]{};
        std::snprintf(code, sizeof(code), "0x%08lX", static_cast<unsigned long>(result));
        throw ApiError(std::string("Falha em ") + operation + " (" + code + ").");
    }
}

[[noreturn]] void win32Error(const char* operation) {
    const DWORD code = GetLastError();
    char stage[96] = "capture.";
    std::size_t i = sizeof("capture.") - 1;
    for (const char* source = operation; *source && i + 1 < sizeof(stage); ++source)
        stage[i++] = ((*source >= 'a' && *source <= 'z') || (*source >= 'A' && *source <= 'Z') ||
                      (*source >= '0' && *source <= '9') || *source == '_') ? *source : '_';
    stage[i] = '\0';
    diagnostic_log::win32(stage, code);
    throw std::runtime_error(std::string("Falha em ") + operation +
                             " (Win32 " + std::to_string(code) + ").");
}

bool sameRect(const RECT& a, const RECT& b) {
    return a.left == b.left && a.top == b.top &&
           a.right == b.right && a.bottom == b.bottom;
}

bool contains(const RECT& outer, const RECT& inner) {
    return inner.left < inner.right && inner.top < inner.bottom &&
           inner.left >= outer.left && inner.top >= outer.top &&
           inner.right <= outer.right && inner.bottom <= outer.bottom;
}

std::wstring windowClass(HWND target) {
    wchar_t name[256]{};
    const int length = GetClassNameW(target, name, 256);
    if (!length) win32Error("GetClassNameW");
    return std::wstring(name, static_cast<std::size_t>(length));
}

struct Target {
    HWND window = nullptr;
    RECT roi{}, client{}, monitorRect{};
    HMONITOR monitor = nullptr;
    DWORD pid = 0;
    std::wstring className;

    void remember(HWND hwnd, RECT region) {
        window = hwnd;
        roi = region;
        if (!IsWindow(window)) throw std::runtime_error("Janela alvo inválida.");
        if (!GetWindowThreadProcessId(window, &pid)) win32Error("GetWindowThreadProcessId");
        className = windowClass(window);
        if (!GetClientRect(window, &client)) win32Error("GetClientRect");
        if (!contains(client, roi))
            throw std::runtime_error("ROI vazia ou fora da área cliente; refaça a calibração.");
        monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
        if (!monitor) throw std::runtime_error("Janela alvo sem monitor disponível.");
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(monitor, &info)) win32Error("GetMonitorInfoW");
        monitorRect = info.rcMonitor;
        // Guardar identidade/geometria não exige foco: a UI pode estar em primeiro plano.
    }

    RECT validate() const {
        if (!IsWindow(window)) throw RecalibrationError("Janela alvo deixou de existir; reconecte.");
        DWORD currentPid = 0;
        if (!GetWindowThreadProcessId(window, &currentPid)) win32Error("GetWindowThreadProcessId");
        if (currentPid != pid || windowClass(window) != className)
            throw RecalibrationError("Identidade da janela alvo mudou; reconecte.");
        if (!IsWindowVisible(window)) throw std::runtime_error("Janela alvo não está visível.");
        if (IsIconic(window)) throw std::runtime_error("Janela alvo está minimizada.");
        RECT currentClient{};
        if (!GetClientRect(window, &currentClient)) win32Error("GetClientRect");
        if (!sameRect(client, currentClient))
            throw RecalibrationError("Tamanho cliente mudou; reinicie a captura e refaça a calibração.");
        if (!contains(currentClient, roi))
            throw std::runtime_error("ROI fora da área cliente; refaça a calibração.");
        if (MonitorFromWindow(window, MONITOR_DEFAULTTONULL) != monitor)
            throw RecalibrationError("Monitor do alvo mudou; reinicie a captura e refaça a calibração.");
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(monitor, &info)) win32Error("GetMonitorInfoW");
        if (!sameRect(info.rcMonitor, monitorRect))
            throw RecalibrationError("Geometria do monitor mudou; reinicie a captura e refaça a calibração.");
        POINT topLeft{roi.left, roi.top}, bottomRight{roi.right, roi.bottom};
        if (!ClientToScreen(window, &topLeft) || !ClientToScreen(window, &bottomRight))
            win32Error("ClientToScreen");
        RECT absolute{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
        if (!contains(monitorRect, absolute))
            throw std::runtime_error("ROI não está totalmente contida no monitor original; refaça a calibração.");
        if (static_cast<std::int64_t>(absolute.right) - absolute.left != roi.right - roi.left ||
            static_cast<std::int64_t>(absolute.bottom) - absolute.top != roi.bottom - roi.top)
            throw std::runtime_error("Janela mudou de posição durante a validação; aguardando estabilidade.");
        if (GetForegroundWindow() != window)
            throw std::runtime_error("Janela alvo não está em primeiro plano.");
        return absolute;
    }
};

struct FrameLease {
    IDXGIOutputDuplication* duplication;
    bool held = true;
    ~FrameLease() { if (held) duplication->ReleaseFrame(); }
    void release() {
        held = false;
        check(duplication->ReleaseFrame(), "IDXGIOutputDuplication::ReleaseFrame");
    }
};

struct MapLease {
    ID3D11DeviceContext* context;
    ID3D11Texture2D* texture;
    ~MapLease() { context->Unmap(texture, 0); }
};

struct Gpu {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGIOutputDuplication> duplication;
    ComPtr<ID3D11Texture2D> staging;
    DXGI_OUTDUPL_DESC description{};
    LARGE_INTEGER frequency{};

    explicit Gpu(const Target& target) {
        ComPtr<IDXGIFactory1> factory;
        check(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf())), "CreateDXGIFactory1");
        ComPtr<IDXGIAdapter1> selectedAdapter;
        ComPtr<IDXGIOutput> selectedOutput;
        for (UINT ai = 0; !selectedOutput; ++ai) {
            ComPtr<IDXGIAdapter1> adapter;
            const HRESULT ar = factory->EnumAdapters1(ai, adapter.GetAddressOf());
            if (ar == DXGI_ERROR_NOT_FOUND) break;
            check(ar, "EnumAdapters1");
            for (UINT oi = 0; ; ++oi) {
                ComPtr<IDXGIOutput> output;
                const HRESULT result = adapter->EnumOutputs(oi, output.GetAddressOf());
                if (result == DXGI_ERROR_NOT_FOUND) break;
                check(result, "EnumOutputs");
                DXGI_OUTPUT_DESC desc{};
                check(output->GetDesc(&desc), "IDXGIOutput::GetDesc");
                if (desc.Monitor != target.monitor) continue;
                if (!desc.AttachedToDesktop)
                    throw std::runtime_error("Monitor original não está conectado à área de trabalho.");
                if (desc.Rotation != DXGI_MODE_ROTATION_IDENTITY)
                    throw std::runtime_error("Rotação do monitor não suportada; use orientação sem rotação.");
                if (!sameRect(desc.DesktopCoordinates, target.monitorRect))
                    throw RecalibrationError("Geometria DXGI difere do monitor calibrado; refaça a calibração.");
                selectedAdapter = adapter;
                selectedOutput = output;
                break;
            }
        }
        if (!selectedOutput) throw std::runtime_error("Monitor original não encontrado pelo DXGI.");
        check(D3D11CreateDevice(selectedAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                               D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                               device.GetAddressOf(), nullptr, context.GetAddressOf()), "D3D11CreateDevice");
        ComPtr<IDXGIOutput1> output1;
        check(selectedOutput.As(&output1), "QueryInterface IDXGIOutput1");
        check(output1->DuplicateOutput(device.Get(), duplication.GetAddressOf()), "DuplicateOutput");
        duplication->GetDesc(&description);
        if (description.Rotation != DXGI_MODE_ROTATION_IDENTITY ||
            description.ModeDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM)
            throw std::runtime_error("Captura exige monitor sem rotação e formato BGRA8.");
        if (description.ModeDesc.Width != static_cast<std::int64_t>(target.monitorRect.right) - target.monitorRect.left ||
            description.ModeDesc.Height != static_cast<std::int64_t>(target.monitorRect.bottom) - target.monitorRect.top)
            throw RecalibrationError("Resolução DXGI mudou; reinicie a captura e refaça a calibração.");

        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(target.roi.right - target.roi.left);
        desc.Height = static_cast<UINT>(target.roi.bottom - target.roi.top);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        check(device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf()), "CreateTexture2D (ROI)");
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
            throw std::runtime_error("QueryPerformanceFrequency indisponível.");
    }

    std::int64_t timestamp(LARGE_INTEGER presented) const {
        LARGE_INTEGER current{};
        if (!QueryPerformanceCounter(&current)) win32Error("QueryPerformanceCounter");
        const auto now = static_cast<std::int64_t>(GetTickCount64());
        if (presented.QuadPart < 0 || presented.QuadPart > current.QuadPart)
            throw std::runtime_error("Timestamp de apresentação DXGI inválido.");
        const long double ageMs = static_cast<long double>(current.QuadPart - presented.QuadPart) *
                                  1000.0L / static_cast<long double>(frequency.QuadPart);
        return ageMs >= static_cast<long double>(now) ? 0 : now - static_cast<std::int64_t>(ageMs);
    }

    std::optional<CaptureFrame> capture(const Target& target, const RECT& absolute) {
        DXGI_OUTDUPL_FRAME_INFO info{};
        ComPtr<IDXGIResource> resource;
        const HRESULT result = duplication->AcquireNextFrame(100, &info, resource.GetAddressOf());
        if (result == DXGI_ERROR_WAIT_TIMEOUT) return std::nullopt;
        check(result, "AcquireNextFrame");
        FrameLease lease{duplication.Get()};
        if (!sameRect(target.validate(), absolute))
            throw std::runtime_error("Janela mudou de posição durante a captura; aguardando estabilidade.");
        // ProtectedContentMaskedOut cobre o desktop inteiro: uma janela excluída
        // fora da ROI não a invalida. Copiamos somente os pixels já entregues pelo
        // Windows, preservando qualquer máscara; ROI uniforme/preta cabe ao Recognizer.
        // Atualização somente do cursor não constitui uma nova imagem do desktop.
        if (!info.LastPresentTime.QuadPart) {
            lease.release();
            return std::nullopt;
        }
        CaptureFrame frame;
        frame.capturedMs = timestamp(info.LastPresentTime);
        ComPtr<ID3D11Texture2D> source;
        check(resource.As(&source), "QueryInterface ID3D11Texture2D");
        D3D11_TEXTURE2D_DESC desc{};
        source->GetDesc(&desc);
        if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM || desc.SampleDesc.Count != 1 ||
            desc.Width != description.ModeDesc.Width || desc.Height != description.ModeDesc.Height)
            throw std::runtime_error("Textura DXGI tem formato ou dimensões incompatíveis com a calibração.");
        D3D11_BOX box{};
        box.left = static_cast<UINT>(static_cast<std::int64_t>(absolute.left) - target.monitorRect.left);
        box.top = static_cast<UINT>(static_cast<std::int64_t>(absolute.top) - target.monitorRect.top);
        box.right = static_cast<UINT>(static_cast<std::int64_t>(absolute.right) - target.monitorRect.left);
        box.bottom = static_cast<UINT>(static_cast<std::int64_t>(absolute.bottom) - target.monitorRect.top);
        box.back = 1;
        if (box.right > desc.Width || box.bottom > desc.Height)
            throw std::runtime_error("ROI fora da textura do monitor original.");
        context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, source.Get(), 0, &box);
        frame.image.width = target.roi.right - target.roi.left;
        frame.image.height = target.roi.bottom - target.roi.top;
        const std::size_t rowBytes = static_cast<std::size_t>(frame.image.width) * 4;
        if (static_cast<std::size_t>(frame.image.height) > (std::numeric_limits<std::size_t>::max)() / rowBytes)
            throw std::runtime_error("ROI grande demais para a memória disponível.");
        frame.image.bgra.resize(rowBytes * static_cast<std::size_t>(frame.image.height));
        {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            check(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Map (ROI)");
            MapLease mapping{context.Get(), staging.Get()};
            if (!mapped.pData || mapped.RowPitch < rowBytes)
                throw std::runtime_error("Map retornou dados ou RowPitch inválidos para a ROI.");
            for (int y = 0; y < frame.image.height; ++y) {
                std::memcpy(frame.image.bgra.data() + static_cast<std::size_t>(y) * rowBytes,
                            static_cast<const std::uint8_t*>(mapped.pData) + static_cast<std::size_t>(y) * mapped.RowPitch,
                            rowBytes);
            }
        }
        check(device->GetDeviceRemovedReason(), "GetDeviceRemovedReason");
        lease.release(); // Nenhum frame adquirido nem Map permanece ativo durante callbacks.
        if (!sameRect(target.validate(), absolute))
            throw std::runtime_error("Janela mudou de posição durante a captura; aguardando estabilidade.");
        frame.available = true;
        return frame;
    }
};
} // namespace

struct DesktopCapture::Impl {
    std::mutex mutex;
    std::condition_variable cv;
    bool stopping = false;
    std::thread worker;
    Target target;
    std::function<void(CaptureFrame)> callback;
    std::string initialError;
    std::string lastError;

    bool stopped() {
        std::lock_guard lock(mutex);
        return stopping;
    }

    void wait(unsigned milliseconds) {
        std::unique_lock lock(mutex);
        cv.wait_for(lock, std::chrono::milliseconds(milliseconds), [this] { return stopping; });
    }

    // Falha do consumidor também não pode escapar da thread nem causar terminate.
    bool deliver(CaptureFrame frame) noexcept {
        if (!callback) return true;
        try {
            callback(std::move(frame));
            return true;
        } catch (...) {
            try {
                CaptureFrame failure;
                failure.error = "Callback de captura lançou uma exceção; sessão encerrada.";
                callback(std::move(failure));
            } catch (...) { /* Consumidor falhou novamente; não há canal seguro restante. */ }
            return false;
        }
    }

    bool unavailable(const std::string& message) {
        if (stopped() || message == lastError) return true;
        lastError = message;
        CaptureFrame frame;
        frame.error = message;
        return deliver(std::move(frame));
    }

    void run() noexcept {
        try {
            if (!initialError.empty()) {
                if (!unavailable(initialError)) return;
                while (!stopped()) wait(100);
                return;
            }
            std::unique_ptr<Gpu> gpu;
            while (!stopped()) {
                unsigned delay = 0;
                try {
                    const RECT absolute = target.validate();
                    if (!gpu) gpu = std::make_unique<Gpu>(target);
                    auto frame = gpu->capture(target, absolute);
                    if (frame && !stopped()) {
                        // Revalidação final também após a liberação do frame DXGI.
                        if (!sameRect(target.validate(), absolute))
                            throw std::runtime_error("Janela mudou de posição durante a captura; aguardando estabilidade.");
                        lastError.clear();
                        if (!deliver(std::move(*frame))) return;
                    }
                } catch (const RecalibrationError& error) {
                    gpu.reset();
                    if (!unavailable(error.what())) return;
                    // Identidade/calibração invalidada nunca se recupera silenciosamente.
                    while (!stopped()) wait(100);
                    return;
                } catch (const ApiError& error) {
                    // Inclui ACCESS_LOST, DEVICE_REMOVED e DEVICE_RESET: recriar no adapter original.
                    gpu.reset();
                    if (!unavailable(error.what())) return;
                    delay = 250;
                } catch (const std::exception& error) {
                    if (!unavailable(error.what())) return;
                    delay = 100;
                } catch (...) {
                    gpu.reset();
                    if (!unavailable("Exceção desconhecida no worker de captura.")) return;
                    delay = 100;
                }
                if (delay) wait(delay);
            }
        } catch (...) {
            // Abrange inclusive falhas de alocação ao construir uma notificação de erro.
            try { unavailable("Falha interna no worker; reinicie a captura."); } catch (...) {}
        }
    }
};

DesktopCapture::DesktopCapture() : impl_(std::make_unique<Impl>()) {}
DesktopCapture::~DesktopCapture() { stop(); }

void DesktopCapture::start(HWND target, RECT clientRoi, std::function<void(CaptureFrame)> callback) {
    stop();
    impl_->target = {};
    impl_->callback = std::move(callback);
    impl_->initialError.clear();
    impl_->lastError.clear();
    try {
        impl_->target.remember(target, clientRoi);
    } catch (const std::exception& error) {
        impl_->initialError = error.what();
    } catch (...) {
        impl_->initialError = "Falha desconhecida ao validar a janela alvo.";
    }
    {
        std::lock_guard lock(impl_->mutex);
        impl_->stopping = false;
    }
    impl_->worker = std::thread([state = impl_.get()] { state->run(); });
}

void DesktopCapture::stop() {
    {
        std::lock_guard lock(impl_->mutex);
        impl_->stopping = true;
    }
    impl_->cv.notify_all();
    if (impl_->worker.joinable()) impl_->worker.join();
    impl_->callback = {};
}

} // namespace aa
