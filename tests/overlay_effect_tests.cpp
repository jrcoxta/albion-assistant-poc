#include "overlay_effect.h"
#include "../src/overlay.cpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <numbers>
#include <objbase.h>

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::size_t at(const aa::Image& image, int x, int y) {
    return (static_cast<std::size_t>(y) * image.width + x) * 4;
}
unsigned alphaAt(const aa::Image& image, int x, int y) {
    return image.bgra[at(image, x, y) + 3];
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
    for (const auto shape : {aa::RegionShape::Rectangle, aa::RegionShape::Circle})
    for (const auto effect : {aa::OverlayEffect::Border, aa::OverlayEffect::Glow,
                              aa::OverlayEffect::Pulse, aa::OverlayEffect::Halo}) {
        for (const auto dimensions : {SIZE{48, 48}, SIZE{80, 32}, SIZE{24, 96}}) {
            const int w = dimensions.cx, h = dimensions.cy;
            if (shape == aa::RegionShape::Circle && w != h) continue;
            const bool aura = effect == aa::OverlayEffect::Glow || effect == aa::OverlayEffect::Pulse;
            const auto image = aa::renderOverlayEffect(w, h, effect, RGB(255, 96, 32), shape);
            check(image.valid(), "O efeito precisa produzir um bitmap válido");
            const int padding = (image.width - w) / 2;
            check(padding > 0 && image.height == h + 2 * padding, "Margem do efeito inconsistente");
            check(padding == aa::overlayEffectPadding(effect, w, h, shape), "Margem pública não corresponde ao raster");
            const unsigned center = alphaAt(image, padding + w / 2, padding + h / 2);
            check(aura ? center > 0 && center <= 64 : center == 0,
                  "Centro deve ser translúcido na aura e transparente no contorno");
            int visible = 0, translucent = 0;
            for (int y = 0; y < image.height; ++y) for (int x = 0; x < image.width; ++x) {
                const auto i = at(image, x, y);
                const unsigned alpha = image.bgra[i + 3];
                if (aura) check(alpha < 205, "Aura tornou o ícone opaco");
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
            if (aura) {
                const int midY = padding + h / 2;
                const auto inner = alphaAt(image, padding + w - 4, midY);
                const auto edge = alphaAt(image, padding + w - 1, midY);
                const auto outer = alphaAt(image, padding + w + 3, midY);
                check(edge >= 175 && outer >= 105,
                      "Destaque dourado continua apagado sobre a habilidade");
                check(inner > center && outer > center && edge >= inner,
                      "Aura não ilumina os dois lados do contorno");
                for (int x = padding + w - 8; x < image.width - 1; ++x)
                    check(std::abs(static_cast<int>(alphaAt(image, x, midY)) -
                                   static_cast<int>(alphaAt(image, x + 1, midY))) <= 20,
                          "Aura contém uma borda abrupta de caixa");
            }
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
        check(current >= 200 && std::abs(next - current) <= 12, "Pulso apagou ou variou abruptamente");
    }
    check(!aa::renderOverlayEffect(0, 48, aa::OverlayEffect::Glow, 0).valid(), "Raster aceitou geometria vazia");
    check(!aa::renderOverlayEffect(INT_MAX, INT_MAX, aa::OverlayEffect::Halo, 0).valid(), "Raster aceitou overflow");
    for (const auto effect : {aa::OverlayEffect::Border, aa::OverlayEffect::Glow,
                              aa::OverlayEffect::Pulse, aa::OverlayEffect::Halo}) {
        check(aa::overlayEffectPadding(effect, 80, 32, aa::RegionShape::Circle) == 0 &&
              !aa::renderOverlayEffect(80, 32, effect, 0, aa::RegionShape::Circle).valid(),
              "Círculo aceitou geometria não quadrada");
        check(!aa::renderOverlayEffect(48, 48, effect, 0, static_cast<aa::RegionShape>(9)).valid(),
              "Raster aceitou formato desconhecido");
        const auto circle = aa::renderOverlayEffect(100, 100, effect, RGB(90, 200, 255), aa::RegionShape::Circle);
        const int p = aa::overlayEffectPadding(effect, 100, 100, aa::RegionShape::Circle);
        // 7,5² + 52,5² == 37,5² + 37,5²: mesma distância no eixo e na diagonal.
        check(alphaAt(circle, p + 57, p + 102) > 80 &&
              std::abs(static_cast<int>(alphaAt(circle, p + 57, p + 102)) -
                       static_cast<int>(alphaAt(circle, p + 87, p + 87))) <= 1,
              "Círculo não mantém intensidade ao longo do raio");
        check(alphaAt(circle, p + 99, p + 99) <= 2,
              "Círculo ainda desenha os cantos de uma caixa");
        if (effect == aa::OverlayEffect::Halo) {
            check(alphaAt(circle, p + 103, p + 49) > 100,
                  "Halo circular não acompanha a circunferência selecionada");
            check(p <= 18, "Halo circular foi inflado para circunscrever um quadrado");
        }
    }
}
void remainingRingChecks() {
    // Cada ponto fica no meio de um quadrante: o apagamento parte de 12h e segue no sentido horário.
    const POINT circlePoints[] = {{54, 9}, {54, 54}, {9, 54}, {9, 9}};
    const POINT ellipsePoints[] = {{81, 6}, {81, 41}, {14, 41}, {14, 6}};
    for (const auto shape : {aa::RegionShape::Circle, aa::RegionShape::Rectangle})
    for (const auto effect : {aa::OverlayEffect::Border, aa::OverlayEffect::Glow,
                              aa::OverlayEffect::Pulse, aa::OverlayEffect::Halo}) {
        const int w = shape == aa::RegionShape::Circle ? 64 : 96;
        const int h = shape == aa::RegionShape::Circle ? 64 : 48;
        const auto* points = shape == aa::RegionShape::Circle ? circlePoints : ellipsePoints;
        const auto base = aa::renderOverlayEffect(w, h, effect, RGB(255, 96, 32), shape);
        const int p = (base.width - w) / 2;
        const auto samePixel = [&](const aa::Image& image, int x, int y) {
            const auto i = at(base, x, y);
            return std::equal(base.bgra.begin() + i, base.bgra.begin() + i + 4, image.bgra.begin() + i);
        };
        const auto absent = aa::renderOverlayEffect(w, h, effect, RGB(255, 96, 32), shape, std::nullopt);
        check(absent.bgra == base.bgra, "Tempo desconhecido alterou o efeito existente");
        for (const float invalid : {-0.01f, 1.01f, std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
            check(aa::renderOverlayEffect(w, h, effect, RGB(255, 96, 32), shape, invalid).bgra == base.bgra,
                  "Fração inválida inventou um relógio");
        for (int cleared = 0; cleared <= 4; ++cleared) {
            const float remaining = 1.0f - cleared * 0.25f;
            const auto image = aa::renderOverlayEffect(w, h, effect, RGB(255, 96, 32), shape, remaining);
            check(image.valid() && image.width == base.width && image.height == base.height,
                  "Aro alterou a geometria e o recorte do efeito");
            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                const int x = p + points[quadrant].x, y = p + points[quadrant].y;
                if (quadrant < cleared) check(samePixel(image, x, y), "Aro não esvazia no sentido horário desde 12h");
                else check(alphaAt(image, x, y) > alphaAt(base, x, y) + 30,
                           "Aro restante não é legível sobre o perímetro selecionado");
            }
            check(samePixel(image, p + w / 2, p + h / 2) &&
                  samePixel(image, p + w / 2 + 6, p + h / 2) &&
                  samePixel(image, 2, p + h / 2),
                  "Aro cobriu o ícone ou substituiu a aura existente");
            if (remaining == 0) check(image.bgra == base.bgra, "Tempo zerado deixou um aro colorido");
            for (std::size_t i = 0; i < image.bgra.size(); i += 4)
                check(image.bgra[i] <= image.bgra[i + 3] && image.bgra[i + 1] <= image.bgra[i + 3] &&
                      image.bgra[i + 2] <= image.bgra[i + 3], "Aro perdeu BGRA premultiplicado");
        }
    }
}
void flameChecks() {
    using aa::OverlayEffect;
    for (const auto shape : {aa::RegionShape::Circle, aa::RegionShape::Rectangle})
    for (const SIZE dimensions : {SIZE{48, 48}, SIZE{96, 40}, SIZE{36, 96}}) {
        const int w = dimensions.cx, h = dimensions.cy;
        if (shape == aa::RegionShape::Circle && w != h) continue;
        const auto render = [&](std::uint64_t time, std::optional<float> remaining = {}) {
            return aa::renderOverlayEffect(w, h, OverlayEffect::Flames, RGB(240, 74, 174), shape, remaining, time);
        };
        const auto first = render(0), second = render(800), third = render(1600);
        check(first.valid() && first.width == w + 64 && first.height == h + 64, "Chamas perderam margem");
        check(first.bgra == render(3200).bgra && first.bgra != second.bgra && second.bgra != third.bgra,
              "Chamas não percorrem o contorno em ciclo contínuo");
        check(alphaAt(first, 32 + w / 2, 32 + h / 2) == 0, "Chamas esconderam a habilidade");
        int moving = 0, outside = 0;
        for (int y = 0; y < first.height; ++y) for (int x = 0; x < first.width; ++x) {
            const auto i = at(first, x, y);
            const bool exterior = x < 32 || y < 32 || x >= 32 + w || y >= 32 + h;
            if (exterior && first.bgra[i + 3] > 20) ++outside;
            if (exterior && std::abs(static_cast<int>(first.bgra[i + 3]) - second.bgra[i + 3]) > 20) ++moving;
            for (const auto& frame : {std::cref(first), std::cref(second), std::cref(third)}) {
                const auto& image = frame.get();
                const unsigned alpha = image.bgra[i + 3];
                check(image.bgra[i] <= alpha && image.bgra[i + 1] <= alpha && image.bgra[i + 2] <= alpha,
                      "Chamas não mantêm BGRA premultiplicado");
                if (x == 0 || y == 0 || x == image.width - 1 || y == image.height - 1)
                    check(alpha <= 1, "Faíscas foram cortadas pela margem do bitmap");
            }
        }
        check(outside > 120 && moving > 90, "Faíscas não saem da região ou não se movem visivelmente");
        const auto ring = render(800, 0.5f);
        const auto p = 32;
        check(ring.bgra != second.bgra && alphaAt(ring, p + w / 2, p + h / 2) == 0 &&
              alphaAt(ring, p, p + h / 2) > alphaAt(second, p, p + h / 2),
              "Aro regressivo não permanece legível sobre Chamas");
        check(render(800, std::numeric_limits<float>::quiet_NaN()).bgra == second.bgra,
              "Tempo inválido alterou as Chamas");
        check(aa::overlayEffectOpacity(OverlayEffect::Flames, 800) == 255,
              "Chamas apagaram globalmente enquanto se movem");
        if (shape == aa::RegionShape::Rectangle) {
            // A primeira faísca alcança a quina superior direita; compare quadros adjacentes ao entrar no arco.
            const double perimeter = 2.0 * (w - 10 + h - 10 + 5.0 * std::numbers::pi);
            const auto cornerMs = static_cast<std::uint64_t>(std::lround(3200.0 * (w - 10) / perimeter));
            const auto difference = [&](std::uint64_t start) {
                const auto before = render(start), after = render(start + 2);
                int delta = 0;
                for (int y = 0; y < 38; ++y) for (int x = w + 16; x < w + 64; ++x)
                    delta += std::abs(static_cast<int>(alphaAt(before, x, y)) -
                                      static_cast<int>(alphaAt(after, x, y)));
                return delta;
            };
            const int cornerDelta = difference(cornerMs - 1);
            const int nearbyDelta = std::max(difference(cornerMs - 31), difference(cornerMs + 29));
            check(cornerDelta < nearbyDelta + 1400, "Faísca saltou ao virar o canto do retângulo");
        }
    }
    check(!aa::renderOverlayEffect(96, 40, OverlayEffect::Flames, 0, aa::RegionShape::Circle).valid(),
          "Chamas aceitaram círculo não quadrado");
}
void flameCost() {
    const auto start = std::chrono::steady_clock::now();
    std::uint64_t checksum = 0;
    for (int n = 0; n < 60; ++n) {
        const auto image = aa::renderOverlayEffect(256, 256, aa::OverlayEffect::Flames,
                                                    RGB(240, 74, 174), aa::RegionShape::Circle, {}, n * 50);
        check(image.valid(), "Frame de Chamas inválido na geometria máxima usual");
        checksum += image.bgra[static_cast<std::size_t>(image.width / 2) * 4 + 3];
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::cout << "chamas: 60 quadros 256 px em " << elapsed << " ms (checksum " << checksum << ")\n";
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
    overlay.setRemaining(0.75f);
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

void saveGallery(const std::filesystem::path& directory, bool timer = false, bool gold = false, bool flames = false) {
    auto canvas = solid(1320, 630, 14, 19, 28);
    // Fundo apenas ilustrativo, com variação visível sob a composição translúcida.
    for (int y = 72; y < 566; ++y) for (int x = 20; x < canvas.width - 20; ++x) {
        const int texture = static_cast<int>(8 * std::sin(x / 43.0) + 6 * std::cos(y / 31.0) +
                                             4 * std::sin((x + y) / 17.0));
        const auto i = at(canvas, x, y);
        canvas.bgra[i] = static_cast<std::uint8_t>(33 + texture);
        canvas.bgra[i + 1] = static_cast<std::uint8_t>(51 + texture);
        canvas.bgra[i + 2] = static_cast<std::uint8_t>(46 + texture);
    }
    auto icon = aa::loadImage(std::filesystem::path(__FILE__).parent_path().parent_path() / "assets" / "assassin-none.png");
    const auto color = gold ? RGB(255, 191, 0) : aa::skillAccentColor(icon).value_or(RGB(210, 100, 255));
    // Somente a prancha recorta o fundo antigo: círculo com 1 px de transição, asset preservado.
    const double iconRadius = std::min(icon.width, icon.height) / 2.0;
    for (int y = 0; y < icon.height; ++y) for (int x = 0; x < icon.width; ++x) {
        const double coverage = std::clamp(iconRadius - std::hypot(x + 0.5 - icon.width / 2.0,
                                                                  y + 0.5 - icon.height / 2.0), 0.0, 1.0);
        const auto i = at(icon, x, y);
        icon.bgra[i + 3] = static_cast<std::uint8_t>(std::lround(icon.bgra[i + 3] * coverage));
    }
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
    const aa::OverlayEffect effects[] = {aa::OverlayEffect::Border, aa::OverlayEffect::Border,
        aa::OverlayEffect::Glow, aa::OverlayEffect::Pulse, aa::OverlayEffect::Pulse, aa::OverlayEffect::Halo};
    for (int row = 0; row < 2; ++row) for (int cell = 0; cell < 6; ++cell) {
        const auto shape = row == 0 ? aa::RegionShape::Rectangle : aa::RegionShape::Circle;
        const int centerX = 110 + cell * 220, centerY = 182 + row * 250;
        compose(icon, centerX - icon.width / 2, centerY - icon.height / 2, 255);
        if (cell == 0 && !timer && !flames) continue;
        const auto selectedEffect = flames ? aa::OverlayEffect::Flames : timer ? aa::OverlayEffect::Glow : effects[cell];
        const std::optional<float> remaining = timer && cell > 0 ?
            std::optional<float>(1.0f - (cell - 1) * 0.25f) :
            flames && cell == 5 ? std::optional<float>(0.5f) : std::nullopt;
        const auto effect = aa::renderOverlayEffect(icon.width, icon.height, selectedEffect, color, shape, remaining,
                                                    static_cast<std::uint64_t>(cell) * 450);
        compose(effect, centerX - effect.width / 2, centerY - effect.height / 2,
                aa::overlayEffectOpacity(selectedEffect, cell == 4 ? 750 : 0));
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
    RECT heading{20, 20, 1300, 54};
    DrawTextW(dc, flames ? L"Quadros do efeito Chamas · partículas em movimento · cor da regra" :
              gold ? L"Prévias do overlay · dourado selecionado · aura translúcida" :
              timer ? L"Prévias do aro regressivo · frações simuladas · aura preservada" :
              L"Prévias do overlay · cor extraída do ícone · aura translúcida", -1, &heading, DT_CENTER | DT_SINGLELINE);
    const wchar_t* effectLabels[] = {L"Sem efeito", L"Borda", L"Brilho", L"Pulso · forte", L"Pulso · suave", L"Halo"};
    const wchar_t* timerLabels[] = {L"Sem relógio", L"100%", L"75%", L"50%", L"25%", L"0%"};
    const wchar_t* flameLabels[] = {L"0 ms", L"450 ms", L"900 ms", L"1350 ms", L"1800 ms", L"2250 ms + aro"};
    const auto* labels = flames ? flameLabels : timer ? timerLabels : effectLabels;
    for (int row = 0; row < 2; ++row) {
        RECT label{36, 86 + row * 250, 360, 118 + row * 250};
        DrawTextW(dc, row == 0 ? L"Área retangular" : L"Área circular", -1, &label, DT_LEFT | DT_SINGLELINE);
    }
    for (int row = 0; row < 2; ++row) for (int cell = 0; cell < 6; ++cell) {
        RECT label{cell * 220, 244 + row * 250, (cell + 1) * 220, 278 + row * 250};
        DrawTextW(dc, labels[cell], -1, &label, DT_CENTER | DT_SINGLELINE);
    }
    RECT footer{20, 588, 1300, 620};
    DrawTextW(dc, L"Fundo ilustrativo e ícone real · simulação de composição, não captura do jogo", -1,
              &footer, DT_CENTER | DT_SINGLELINE);
    std::memcpy(canvas.bgra.data(), bits, canvas.bgra.size());
    for (std::size_t i = 3; i < canvas.bgra.size(); i += 4) canvas.bgra[i] = 255;
    SelectObject(dc, oldFont); SelectObject(dc, oldBitmap);
    DeleteObject(font); DeleteObject(bitmap); DeleteDC(dc);
    std::filesystem::create_directories(directory);
    aa::saveImage(canvas, directory / (flames ? (gold ? "overlay-chamas-dourado.png" : "overlay-chamas.png") :
                                       gold ? "overlay-gold.png" :
                                       timer ? "timer-overlay.png" : "overlay-effects.png"));
}
}
int main(int argc, char** argv) {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    try {
        rasterChecks(); remainingRingChecks(); flameChecks(); flameCost(); placementChecks(); colorChecks(); visibilityChecks();
        check(overlayProc(nullptr, WM_NCHITTEST, 0, 0) == HTTRANSPARENT, "Overlay deixou de ser click-through");
        check(overlayProc(nullptr, WM_MOUSEACTIVATE, 0, 0) == MA_NOACTIVATE, "Overlay pode roubar foco");
        if (argc > 1) { saveGallery(argv[1]); saveGallery(argv[1], true); saveGallery(argv[1], false, true);
                        saveGallery(argv[1], false, false, true); saveGallery(argv[1], false, true, true); }
        std::cout << "overlay: raster, aro regressivo, pulso, recorte, cor e guards nativos passaram\n";
        if (SUCCEEDED(com)) CoUninitialize();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        if (SUCCEEDED(com)) CoUninitialize();
        return 1;
    }
}
