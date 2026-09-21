#include "overlay_effect.h"
#include "../src/overlay.cpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <objbase.h>

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::size_t at(const aa::Image& image, int x, int y) {
    return (static_cast<std::size_t>(y) * image.width + x) * 4;
}
aa::Image solid(int width, int height, unsigned r, unsigned g, unsigned b) {
    aa::Image image{width, height, std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4)};
    for (std::size_t i = 0; i < image.bgra.size(); i += 4) {
        image.bgra[i] = static_cast<std::uint8_t>(b);
        image.bgra[i + 1] = static_cast<std::uint8_t>(g);
        image.bgra[i + 2] = static_cast<std::uint8_t>(r);
        image.bgra[i + 3] = 255;
    }
    return image;
}
void fill(aa::Image& image, int left, int top, int right, int bottom, unsigned r, unsigned g, unsigned b) {
    for (int y = top; y < bottom; ++y) for (int x = left; x < right; ++x) {
        const auto i = at(image, x, y);
        image.bgra[i] = static_cast<std::uint8_t>(b);
        image.bgra[i + 1] = static_cast<std::uint8_t>(g);
        image.bgra[i + 2] = static_cast<std::uint8_t>(r);
    }
}
void rasterChecks() {
    std::vector<aa::Image> samples;
    for (const auto effect : {aa::OverlayEffect::Border, aa::OverlayEffect::Glow,
                              aa::OverlayEffect::Pulse, aa::OverlayEffect::Halo}) {
        for (const auto dimensions : {SIZE{48, 48}, SIZE{80, 32}, SIZE{24, 96}}) {
            const int w = dimensions.cx, h = dimensions.cy;
            const auto image = aa::renderOverlayEffect(w, h, effect, RGB(255, 96, 32));
            check(image.valid(), "O efeito precisa produzir um bitmap válido");
            const int padding = (image.width - w) / 2;
            check(padding > 0 && image.height == h + 2 * padding, "Margem do efeito inconsistente");
            check(padding == aa::overlayEffectPadding(effect, w, h), "Margem pública não corresponde ao raster");
            int visible = 0, translucent = 0;
            for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
                const auto i = at(image, x, y);
                const unsigned alpha = image.bgra[i + 3];
                if (x >= padding && x < padding + w && y >= padding && y < padding + h)
                    check(alpha == 0, "Efeito cobriu a habilidade");
                if (x == 0 || y == 0 || x == image.width - 1 || y == image.height - 1)
                    check(alpha <= 1, "Efeito foi cortado na borda do bitmap");
                check(image.bgra[i] <= alpha && image.bgra[i + 1] <= alpha && image.bgra[i + 2] <= alpha,
                      "BGRA não está premultiplicado");
                check(image.bgra[i + 2] == alpha, "Canal vermelho foi trocado com azul");
                check(std::abs(static_cast<int>(image.bgra[i]) - static_cast<int>((32 * alpha + 127) / 255)) <= 1,
                      "Cor manual não foi respeitada");
                visible += alpha > 0;
                translucent += alpha > 0 && alpha < 220;
            }
            check(visible > 50, "Efeito não contém contorno visível");
            if (effect != aa::OverlayEffect::Border) check(translucent > 50, "Aura não tem queda suave de intensidade");
            if (w == 48) samples.push_back(image);
        }
    }
    check(samples[0].bgra != samples[1].bgra && samples[1].bgra != samples[3].bgra,
          "Borda, brilho e halo precisam ter desenhos distintos");
    check(samples[1].bgra == samples[2].bgra, "Pulso deve reutilizar o bitmap de brilho");
    for (const auto effect : {aa::OverlayEffect::Border, aa::OverlayEffect::Glow, aa::OverlayEffect::Halo})
        check(aa::overlayEffectOpacity(effect, 0) == 255 && aa::overlayEffectOpacity(effect, 750) == 255,
              "Efeito estático variou opacidade");
    check(aa::overlayEffectOpacity(aa::OverlayEffect::Pulse, 0) > aa::overlayEffectOpacity(aa::OverlayEffect::Pulse, 750),
          "Pulso não respira");
    check(aa::overlayEffectOpacity(aa::OverlayEffect::Pulse, 0) == aa::overlayEffectOpacity(aa::OverlayEffect::Pulse, 1500),
          "Pulso não fecha o ciclo de 1,5 segundo");
    for (std::uint64_t ms = 0; ms < 1500; ms += 50) {
        const int current = aa::overlayEffectOpacity(aa::OverlayEffect::Pulse, ms);
        const int next = aa::overlayEffectOpacity(aa::OverlayEffect::Pulse, ms + 50);
        check(current >= 150 && std::abs(next - current) <= 12, "Pulso apagou ou variou abruptamente");
    }
    check(!aa::renderOverlayEffect(0, 48, aa::OverlayEffect::Glow, 0).valid(), "Raster aceitou geometria vazia");
    check(!aa::renderOverlayEffect(INT_MAX, INT_MAX, aa::OverlayEffect::Halo, 0).valid(), "Raster aceitou overflow");
}
void placementChecks() {
    const auto full = overlayPlacement({0, 0, 100, 80}, {30, 20, 70, 60}, {-200, 150}, 8);
    check(full && full->destination.x == -178 && full->destination.y == 162 && full->source.x == 0 &&
          full->source.y == 0 && full->size.cx == 56 && full->size.cy == 56, "Posição em monitor negativo incorreta");
    const auto edge = overlayPlacement({0, 0, 100, 80}, {0, 0, 24, 24}, {-200, 150}, 12);
    check(edge && edge->destination.x == -200 && edge->destination.y == 150 && edge->source.x == 12 &&
          edge->source.y == 12 && edge->size.cx == 36 && edge->size.cy == 36 && edge->bitmapSize.cx == 48,
          "Recorte superior/esquerdo deslocou a habilidade");
    const auto bottom = overlayPlacement({0, 0, 100, 80}, {76, 56, 100, 80}, {0, 0}, 12);
    check(bottom && bottom->destination.x == 64 && bottom->destination.y == 44 && bottom->source.x == 0 &&
          bottom->source.y == 0 && bottom->size.cx == 36 && bottom->size.cy == 36,
          "Recorte inferior/direito excedeu a área cliente");
    check(!overlayPlacement({0, 0, 100, 80}, {-1, 0, 24, 24}, {}, 12), "Destino fora do cliente foi aceito");
    check(!overlayPlacement({0, 0, 100, 80}, {5, 5, 5, 10}, {}, 12), "Destino vazio foi aceito");
    check(!overlayPlacement({0, 0, 100, 80}, {30, 20, 70, 60}, {LONG_MAX, 0}, 12), "Coordenada absoluta com overflow foi aceita");
}
void colorChecks() {
    check(!aa::skillAccentColor({}), "Imagem inválida inventou uma cor");
    for (const unsigned value : {0U, 18U, 110U, 235U, 255U})
        check(!aa::skillAccentColor(solid(64, 64, value, value, value)), "Imagem neutra inventou uma cor");
    check(!aa::skillAccentColor(solid(64, 64, 38, 8, 8)), "Fundo escuro virou destaque");
    for (const bool blue : {false, true}) {
        auto image = solid(64, 64, 230, 170, 30); // Moldura dourada.
        fill(image, 9, 9, 55, 55, 22, 22, 22);
        fill(image, 17, 13, 47, 46, blue ? 24 : 224, 52, blue ? 224 : 24);
        fill(image, 27, 25, 36, 38, 240, 240, 240); // Reflexo branco.
        fill(image, 34, 46, 56, 59, 40, 245, 40); // Contador colorido.
        const auto color = aa::skillAccentColor(image);
        check(color.has_value(), "Ícone colorido não produziu cor");
        check(blue ? GetBValue(*color) > 180 && GetRValue(*color) < 70 :
                     GetRValue(*color) > 180 && GetBValue(*color) < 70,
              "Moldura, contador ou branco dominou a habilidade");
    }
    auto neutral = solid(64, 64, 120, 120, 120);
    fill(neutral, 0, 0, 64, 8, 240, 160, 20);
    fill(neutral, 0, 56, 64, 64, 240, 160, 20);
    fill(neutral, 0, 8, 8, 56, 240, 160, 20);
    fill(neutral, 56, 8, 64, 56, 240, 160, 20);
    fill(neutral, 34, 46, 56, 59, 250, 40, 40);
    check(!aa::skillAccentColor(neutral), "Moldura e contador deram cor a ícone neutro");
    fill(neutral, 28, 28, 30, 30, 255, 0, 0);
    check(!aa::skillAccentColor(neutral), "Ruído isolado deu cor a ícone neutro");
}
void visibilityChecks() {
    Overlay overlay;
    overlay.initialize(GetModuleHandleW(nullptr));
    HWND window = nullptr;
    EnumThreadWindows(GetCurrentThreadId(), [](HWND candidate, LPARAM result) -> BOOL {
        wchar_t name[80]{};
        GetClassNameW(candidate, name, 80);
        if (lstrcmpW(name, L"AlbionAssistantPocOverlay") == 0) {
            *reinterpret_cast<HWND*>(result) = candidate;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&window));
    check(window != nullptr, "Janela de overlay não foi criada");
    const auto styles = GetWindowLongPtrW(window, GWL_EXSTYLE);
    constexpr LONG_PTR required = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    check((styles & required) == required, "Overlay perdeu estilos de passagem de clique e foco");
    DWORD affinity = 0;
    check(GetWindowDisplayAffinity(window, &affinity) && affinity == WDA_EXCLUDEFROMCAPTURE,
          "Overlay deixou de ser excluído da captura por padrão");
    overlay.setCaptureVisible(true);
    check(GetWindowDisplayAffinity(window, &affinity) && affinity == WDA_NONE,
          "Diagnóstico não liberou a captura do próprio overlay");
    overlay.setCaptureVisible(false);
    check(GetWindowDisplayAffinity(window, &affinity) && affinity == WDA_EXCLUDEFROMCAPTURE,
          "Exclusão da captura não foi restaurada");
    const auto showWithoutFocus = [&] {
        ShowWindow(window, SW_SHOWNOACTIVATE);
        check(IsWindowVisible(window) != FALSE, "Harness não mostrou a janela sintética");
    };
    showWithoutFocus();
    overlay.update(nullptr, {20, 20, 44, 44}, false);
    check(IsWindowVisible(window) == FALSE, "Condição falsa não apagou imediatamente");
    showWithoutFocus();
    overlay.update(nullptr, {20, 20, 44, 44}, true);
    check(IsWindowVisible(window) == FALSE, "Alvo inválido não apagou imediatamente");

    const HWND previousForeground = GetForegroundWindow();
    HWND target = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, L"STATIC", L"Teste de overlay",
        WS_POPUP | WS_VISIBLE, -32000, -32000, 100, 80, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    check(target != nullptr, "Alvo sintético sem ativação não foi criado");
    check(GetForegroundWindow() != target, "Alvo sintético roubou foco");
    showWithoutFocus();
    overlay.update(target, {20, 20, 44, 44}, true);
    check(IsWindowVisible(window) == FALSE, "Foco ausente não apagou imediatamente");
    check(GetForegroundWindow() == previousForeground, "Teste de overlay alterou o foco do desktop");
    DestroyWindow(target);
}

void saveGallery(const std::filesystem::path& directory) {
    auto canvas = solid(1100, 340, 14, 19, 28);
    auto icon = aa::loadImage(std::filesystem::path(__FILE__).parent_path().parent_path() / "assets" / "assassin-none.png");
    const auto color = aa::skillAccentColor(icon).value_or(RGB(210, 100, 255));
    for (std::size_t i = 0; i < icon.bgra.size(); i += 4)
        for (int channel = 0; channel < 3; ++channel)
            icon.bgra[i + channel] = static_cast<std::uint8_t>((icon.bgra[i + channel] * icon.bgra[i + 3] + 127) / 255);
    const auto compose = [&](const aa::Image& source, int left, int top, unsigned opacity) {
        for (int y = 0; y < source.height; ++y) for (int x = 0; x < source.width; ++x) {
            if (left + x < 0 || top + y < 0 || left + x >= canvas.width || top + y >= canvas.height) continue;
            const auto src = at(source, x, y), dst = at(canvas, left + x, top + y);
            const unsigned alpha = (source.bgra[src + 3] * opacity + 127) / 255;
            for (int channel = 0; channel < 3; ++channel)
                canvas.bgra[dst + channel] = static_cast<std::uint8_t>((source.bgra[src + channel] * opacity + 127) / 255 +
                    (canvas.bgra[dst + channel] * (255 - alpha) + 127) / 255);
        }
    };
    const aa::OverlayEffect effects[] = {aa::OverlayEffect::Border, aa::OverlayEffect::Glow,
        aa::OverlayEffect::Pulse, aa::OverlayEffect::Pulse, aa::OverlayEffect::Halo};
    for (int cell = 0; cell < 5; ++cell) {
        const auto effect = aa::renderOverlayEffect(icon.width, icon.height, effects[cell], color);
        compose(icon, 110 + cell * 220 - icon.width / 2, 170 - icon.height / 2, 255);
        compose(effect, 110 + cell * 220 - effect.width / 2, 170 - effect.height / 2,
                aa::overlayEffectOpacity(effects[cell], cell == 3 ? 750 : 0));
    }
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = canvas.width; info.bmiHeader.biHeight = -canvas.height;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    HDC dc = CreateCompatibleDC(nullptr);
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HFONT font = CreateFontW(-20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    check(dc && bitmap && bits && font, "Não foi possível preparar texto da prancha");
    const auto oldBitmap = SelectObject(dc, bitmap), oldFont = SelectObject(dc, font);
    std::memcpy(bits, canvas.bgra.data(), canvas.bgra.size());
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(228, 235, 247));
    RECT heading{20, 20, 1080, 54};
    DrawTextW(dc, L"Prévias do overlay · cor extraída do ícone · centro preservado", -1, &heading, DT_CENTER | DT_SINGLELINE);
    const wchar_t* labels[] = {L"Borda", L"Brilho", L"Pulso · forte", L"Pulso · suave", L"Halo"};
    for (int cell = 0; cell < 5; ++cell) {
        RECT label{cell * 220, 254, (cell + 1) * 220, 288};
        DrawTextW(dc, labels[cell], -1, &label, DT_CENTER | DT_SINGLELINE);
    }
    std::memcpy(canvas.bgra.data(), bits, canvas.bgra.size());
    for (std::size_t i = 3; i < canvas.bgra.size(); i += 4) canvas.bgra[i] = 255;
    SelectObject(dc, oldFont); SelectObject(dc, oldBitmap);
    DeleteObject(font); DeleteObject(bitmap); DeleteDC(dc);
    std::filesystem::create_directories(directory);
    aa::saveImage(canvas, directory / "overlay-effects.png");
}
}
int main(int argc, char** argv) {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    try {
        rasterChecks(); placementChecks(); colorChecks(); visibilityChecks();
        check(overlayProc(nullptr, WM_NCHITTEST, 0, 0) == HTTRANSPARENT, "Overlay deixou de ser click-through");
        check(overlayProc(nullptr, WM_MOUSEACTIVATE, 0, 0) == MA_NOACTIVATE, "Overlay pode roubar foco");
        if (argc > 1) saveGallery(argv[1]);
        std::cout << "overlay: raster, pulso, recorte, cor e guards nativos passaram\n";
        if (SUCCEEDED(com)) CoUninitialize();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        if (SUCCEEDED(com)) CoUninitialize();
        return 1;
    }
}
