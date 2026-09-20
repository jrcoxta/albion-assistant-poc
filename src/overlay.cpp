#include "overlay.h"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {
constexpr int padding = 6;

LRESULT CALLBACK overlayProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    // Extras defensivos: o click-through entre processos vem de LAYERED + TRANSPARENT.
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message == WM_ERASEBKGND) return 1;
    return DefWindowProcW(window, message, w, l);
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
    if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        throw std::runtime_error("RegisterClass overlay");
    window_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE |
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST, cls.lpszClassName, L"Albion Assistant Overlay",
        WS_POPUP, 0, 0, 1, 1, nullptr, nullptr, instance, nullptr);
    if (!window_) throw std::runtime_error("CreateWindow overlay");
    // Não permitir falha silenciosa na política de captura selecionada.
    if (!SetWindowDisplayAffinity(window_, captureVisible_ ? WDA_NONE : WDA_EXCLUDEFROMCAPTURE)) {
        DestroyWindow(window_);
        window_ = nullptr;
        throw std::runtime_error("SetWindowDisplayAffinity overlay: nao foi possivel aplicar a politica de captura");
    }
    memoryDC_ = CreateCompatibleDC(nullptr);
    if (!memoryDC_) {
        DestroyWindow(window_);
        window_ = nullptr;
        throw std::runtime_error("CreateCompatibleDC overlay");
    }
}

void Overlay::setCaptureVisible(bool enabled) {
    // Apenas diagnóstico de screenshot do overlay do próprio app;
    // não altera o jogo nem máscaras de captura de outras janelas.
    if (captureVisible_ == enabled) return;
    if (window_ && !SetWindowDisplayAffinity(window_, enabled ? WDA_NONE : WDA_EXCLUDEFROMCAPTURE))
        throw std::runtime_error("SetWindowDisplayAffinity overlay: nao foi possivel alterar a visibilidade na captura");
    captureVisible_ = enabled;
}

void Overlay::hide() {
    if (window_ && IsWindowVisible(window_)) ShowWindow(window_, SW_HIDE);
}

bool Overlay::draw(int width, int height) {
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
    // A ROI sempre começa em (6,6); mudar sua posição não muda os pixels locais.
    auto* pixels = static_cast<std::uint32_t*>(pixels_);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int edge = x;
            if (y < edge) edge = y;
            if (width - 1 - x < edge) edge = width - 1 - x;
            if (height - 1 - y < edge) edge = height - 1 - y;
            const unsigned alpha = edge < 2 ? 32u : edge < 4 ? 96u : edge < padding ? 255u : 0u;
            const unsigned red = (GetRValue(color_) * alpha + 127) / 255;
            const unsigned green = (GetGValue(color_) * alpha + 127) / 255;
            const unsigned blue = (GetBValue(color_) * alpha + 127) / 255;
            *pixels++ = (alpha << 24) | (red << 16) | (green << 8) | blue;
        }
    }
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
    if (!GetClientRect(target, &client) || client.right <= client.left || client.bottom <= client.top ||
        icon.left < client.left || icon.top < client.top || icon.right > client.right ||
        icon.bottom > client.bottom || icon.right <= icon.left || icon.bottom <= icon.top ||
        !ClientToScreen(target, &origin)) {
        hide();
        return;
    }
    // Todas as somas/diferenças são alargadas antes de operar em coordenadas externas.
    const std::int64_t width = static_cast<std::int64_t>(icon.right) - icon.left + 2 * padding;
    const std::int64_t height = static_cast<std::int64_t>(icon.bottom) - icon.top + 2 * padding;
    const std::int64_t x = static_cast<std::int64_t>(origin.x) + icon.left - padding;
    const std::int64_t y = static_cast<std::int64_t>(origin.y) + icon.top - padding;
    constexpr auto maxInt = (std::numeric_limits<int>::max)();
    constexpr auto minLong = (std::numeric_limits<LONG>::min)();
    constexpr auto maxLong = (std::numeric_limits<LONG>::max)();
    if (width > maxInt || height > maxInt || x < minLong || y < minLong ||
        x + width > maxLong || y + height > maxLong ||
        static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height) >
            (std::numeric_limits<DWORD>::max)() / 4u) {
        hide();
        return;
    }
    const int w = static_cast<int>(width), h = static_cast<int>(height);
    if (!drawn_ || width_ != w || height_ != h) {
        if (!draw(w, h)) { hide(); return; }
        POINT destination{static_cast<LONG>(x), static_cast<LONG>(y)};
        POINT source{};
        SIZE size{w, h};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        if (!UpdateLayeredWindow(window_, nullptr, &destination, &size, memoryDC_, &source,
            0, &blend, ULW_ALPHA)) {
            drawn_ = false;
            hide();
            return;
        }
        drawn_ = true;
    }
    RECT current{};
    const RECT desired{static_cast<LONG>(x), static_cast<LONG>(y),
        static_cast<LONG>(x + width), static_cast<LONG>(y + height)};
    if (IsWindowVisible(window_) && GetWindowRect(window_, &current) && EqualRect(&current, &desired)) return;
    // Movimento sem recodificar/retransmitir o DIB; nunca ativar nem encaminhar input.
    if (!SetWindowPos(window_, HWND_TOPMOST, static_cast<int>(x), static_cast<int>(y), w, h,
        SWP_NOACTIVATE | SWP_SHOWWINDOW)) hide();
}
