#include "overlay.h"
#include "diagnostic_log.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>

namespace {
struct OverlayPlacement {
    POINT destination{}, source{};
    SIZE size{}, bitmapSize{};
};
std::optional<OverlayPlacement> overlayPlacement(RECT client, RECT icon, POINT origin, int padding) {
    if (padding <= 0 || client.right <= client.left || client.bottom <= client.top ||
        icon.left < client.left || icon.top < client.top || icon.right > client.right ||
        icon.bottom > client.bottom || icon.right <= icon.left || icon.bottom <= icon.top) return std::nullopt;
    // Alargar antes de subtrair também protege coordenadas de monitores à esquerda/acima.
    const std::int64_t left = static_cast<std::int64_t>(icon.left) - padding;
    const std::int64_t top = static_cast<std::int64_t>(icon.top) - padding;
    const std::int64_t right = static_cast<std::int64_t>(icon.right) + padding;
    const std::int64_t bottom = static_cast<std::int64_t>(icon.bottom) + padding;
    const auto clippedLeft = std::max<std::int64_t>(left, client.left);
    const auto clippedTop = std::max<std::int64_t>(top, client.top);
    const auto clippedRight = std::min<std::int64_t>(right, client.right);
    const auto clippedBottom = std::min<std::int64_t>(bottom, client.bottom);
    const std::int64_t x = origin.x + clippedLeft, y = origin.y + clippedTop;
    const auto width = right - left, height = bottom - top;
    constexpr auto maxLong = (std::numeric_limits<LONG>::max)();
    constexpr auto minLong = (std::numeric_limits<LONG>::min)();
    if (width > maxLong || height > maxLong || x < minLong || y < minLong ||
        origin.x + clippedRight > maxLong || origin.y + clippedBottom > maxLong ||
        static_cast<std::uint64_t>(width) * height > 64000000) return std::nullopt;
    return OverlayPlacement{{static_cast<LONG>(x), static_cast<LONG>(y)},
        {static_cast<LONG>(clippedLeft - left), static_cast<LONG>(clippedTop - top)},
        {static_cast<LONG>(clippedRight - clippedLeft), static_cast<LONG>(clippedBottom - clippedTop)},
        {static_cast<LONG>(width), static_cast<LONG>(height)}};
}

LRESULT CALLBACK overlayProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    // Extras defensivos: o click-through entre processos vem de LAYERED + TRANSPARENT.
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message == WM_ERASEBKGND) return 1;
    return DefWindowProcW(window, message, w, l);
}

void applyCapturePolicy(HWND window, DWORD mode, const char* stage, const char* explanation,
                        BOOL (WINAPI *apply)(HWND, DWORD) = SetWindowDisplayAffinity) {
    if (apply(window, mode)) return;
    const auto reason = GetLastError(); // Antes de escrever o log ou destruir a janela.
    diagnostic_log::win32(stage, reason);
    throw std::runtime_error(std::string(explanation) + " (Win32 " + std::to_string(reason) + ").");
}
}

Overlay::~Overlay() {
    if (window_) DestroyWindow(window_);
    // Destruir o DC primeiro libera a seleção do bitmap antes de excluí-lo.
    if (memoryDC_) DeleteDC(memoryDC_);
    if (bitmap_) DeleteObject(bitmap_);
}

void Overlay::initialize(HINSTANCE instance) {
    if (window_) return;
    WNDCLASSW cls{};
    cls.hInstance = instance;
    cls.lpfnWndProc = overlayProc;
    cls.lpszClassName = L"AlbionAssistantPocOverlay";
    if (!RegisterClassW(&cls)) {
        const auto reason=GetLastError();
        if(reason!=ERROR_CLASS_ALREADY_EXISTS){diagnostic_log::win32("overlay.RegisterClassW",reason);throw std::runtime_error("RegisterClass overlay (Win32 "+std::to_string(reason)+").");}
    }
    window_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST, cls.lpszClassName, L"Albion Assistant Overlay",
        WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
    if (!window_){const auto reason=GetLastError();diagnostic_log::win32("overlay.CreateWindowExW",reason);throw std::runtime_error("CreateWindow overlay (Win32 "+std::to_string(reason)+").");}
    // Não permitir falha silenciosa na política de captura selecionada.
    try {
        applyCapturePolicy(window_, captureVisible_ ? WDA_NONE : WDA_EXCLUDEFROMCAPTURE,
            "overlay.apply_capture_policy", "SetWindowDisplayAffinity overlay: nao foi possivel aplicar a politica de captura");
    } catch (...) {
        DestroyWindow(window_);
        window_ = nullptr;
        throw;
    }
    memoryDC_ = CreateCompatibleDC(nullptr);
    if (!memoryDC_) {
        const auto reason=GetLastError();
        diagnostic_log::win32("overlay.CreateCompatibleDC",reason);
        DestroyWindow(window_);
        window_ = nullptr;
        throw std::runtime_error("CreateCompatibleDC overlay (Win32 "+std::to_string(reason)+").");
    }
}

void Overlay::setCaptureVisible(bool enabled) {
    // Apenas diagnóstico de screenshot do overlay do próprio app;
    // não altera o jogo nem máscaras de captura de outras janelas.
    if (captureVisible_ == enabled) return;
    if (window_)
        applyCapturePolicy(window_, enabled ? WDA_NONE : WDA_EXCLUDEFROMCAPTURE,
            "overlay.change_capture_policy", "SetWindowDisplayAffinity overlay: nao foi possivel alterar a visibilidade na captura");
    captureVisible_ = enabled;
}

void Overlay::hide() {
    if (window_ && IsWindowVisible(window_)) ShowWindow(window_, SW_HIDE);
}

void Overlay::setRemaining(std::optional<float> remaining) {
    if (remaining && (!std::isfinite(*remaining) || *remaining < 0 || *remaining > 1)) remaining.reset();
    // Quantizar só o desenho evita recodificar o DIB por ruído entre frames, sem inventar um relógio.
    if (remaining) remaining = std::round(*remaining * 120.0f) / 120.0f;
    if (remaining == 0.0f) remaining.reset();
    if (remaining_ != remaining) { remaining_ = remaining; drawn_ = false; }
}

bool Overlay::draw(int iconWidth, int iconHeight, std::uint64_t elapsedMs) {
    aa::Image image;
    try { image = aa::renderOverlayEffect(iconWidth, iconHeight, effect_, color_, shape_, remaining_, elapsedMs); }
    catch (const std::exception&) { return false; }
    if (!image.valid()) return false;
    const int width = image.width, height = image.height;
    if (width_ != width || height_ != height) {
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height; // DIB top-down, BGRA de 32 bits.
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        HBITMAP bitmap = CreateDIBSection(memoryDC_, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!bitmap) return false;
        HGDIOBJ previous = SelectObject(memoryDC_, bitmap);
        if (!previous || previous == HGDI_ERROR) {
            DeleteObject(bitmap);
            return false;
        }
        if (bitmap_) DeleteObject(bitmap_);
        bitmap_ = bitmap;
        pixels_ = pixels;
        width_ = width;
        height_ = height;
        drawn_ = false;
    }
    std::memcpy(pixels_, image.bgra.data(), image.bgra.size());
    return true;
}

void Overlay::update(HWND target, RECT icon, bool highlight) {
    if (!window_) return;
    if (!highlight || !IsWindow(target) || IsIconic(target) || !IsWindowVisible(target) ||
        GetForegroundWindow() != target) {
        hide();
        return;
    }
    RECT client{};
    POINT origin{};
    if (!GetClientRect(target, &client) || !ClientToScreen(target, &origin)) {
        hide();
        return;
    }
    const auto iconWidth = static_cast<std::int64_t>(icon.right) - icon.left;
    const auto iconHeight = static_cast<std::int64_t>(icon.bottom) - icon.top;
    if (iconWidth <= 0 || iconHeight <= 0 || iconWidth > 16384 || iconHeight > 16384) { hide(); return; }
    const int iw = static_cast<int>(iconWidth), ih = static_cast<int>(iconHeight);
    auto placement = overlayPlacement(client, icon, origin, aa::overlayEffectPadding(effect_, iw, ih, shape_));
    if (!placement) { hide(); return; }
    const auto now = GetTickCount64();
    const auto frame = now / 50;
    const bool redraw = !drawn_ || width_ != placement->bitmapSize.cx || height_ != placement->bitmapSize.cy ||
                        (effect_ == aa::OverlayEffect::Flames && frame_ != frame);
    if (redraw && !draw(iw, ih, frame * 50)) { hide(); return; }
    const BYTE opacity = aa::overlayEffectOpacity(effect_, now);
    // Pulso troca somente SourceConstantAlpha: não percorre pixels a cada atualização.
    if (redraw || opacity_ != opacity || source_.x != placement->source.x || source_.y != placement->source.y ||
        size_.cx != placement->size.cx || size_.cy != placement->size.cy) {
        BLENDFUNCTION blend{AC_SRC_OVER, 0, opacity, AC_SRC_ALPHA};
        if (!UpdateLayeredWindow(window_, nullptr, &placement->destination, &placement->size, memoryDC_, &placement->source,
            0, &blend, ULW_ALPHA)) {
            drawn_ = false;
            hide();
            return;
        }
        drawn_ = true;
        frame_ = frame;
        source_ = placement->source;
        size_ = placement->size;
        opacity_ = opacity;
    }
    RECT current{};
    const int x = placement->destination.x, y = placement->destination.y;
    const int w = placement->size.cx, h = placement->size.cy;
    const RECT desired{x, y, x + w, y + h};
    if (IsWindowVisible(window_) && GetWindowRect(window_, &current) && EqualRect(&current, &desired)) return;
    // Movimento sem recodificar/retransmitir o DIB; nunca ativar nem encaminhar input.
    if (!SetWindowPos(window_, HWND_TOPMOST, x, y, w, h,
        SWP_NOACTIVATE | SWP_SHOWWINDOW)) hide();
}
