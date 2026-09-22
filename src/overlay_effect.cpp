#include "overlay_effect.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace aa {
int overlayEffectPadding(OverlayEffect effect, int iconWidth, int iconHeight, RegionShape shape) {
    if (iconWidth <= 0 || iconHeight <= 0 || iconWidth > 16384 || iconHeight > 16384 ||
        (shape != RegionShape::Rectangle && shape != RegionShape::Circle) ||
        (shape == RegionShape::Circle && iconWidth != iconHeight)) return 0;
    switch (effect) {
    case OverlayEffect::Border: return 6;
    case OverlayEffect::Glow:
    case OverlayEffect::Pulse: return 24;
    case OverlayEffect::Halo: return 16;
    }
    return 0;
}

Image renderOverlayEffect(int iconWidth, int iconHeight, OverlayEffect effect, std::uint32_t color, RegionShape shape,
                          std::optional<float> remaining) {
    const int padding = overlayEffectPadding(effect, iconWidth, iconHeight, shape);
    if (!padding) return {};
    const int width = iconWidth + 2 * padding, height = iconHeight + 2 * padding;
    if (static_cast<std::uint64_t>(width) * height > 64000000) return {};
    Image image{width, height, std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4)};
    const double halfWidth = iconWidth / 2.0, halfHeight = iconHeight / 2.0;
    const double radius = std::min({5.0, halfWidth, halfHeight});
    const bool aura = effect == OverlayEffect::Glow || effect == OverlayEffect::Pulse;
    const unsigned red = color & 255u, green = (color >> 8) & 255u, blue = (color >> 16) & 255u;
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const double dx = std::abs(x + 0.5 - padding - halfWidth);
        const double dy = std::abs(y + 0.5 - padding - halfHeight);
        const double qx = dx - (halfWidth - radius), qy = dy - (halfHeight - radius);
        const double distance = shape == RegionShape::Circle ? std::hypot(dx, dy) - halfWidth :
            std::hypot(std::max(qx, 0.0), std::max(qy, 0.0)) + std::min(std::max(qx, qy), 0.0) - radius;
        // Contornos preservam o interior; somente a aura ilumina por cima da habilidade.
        if (!aura && distance < 0) continue;
        double intensity = 0;
        if (effect == OverlayEffect::Halo) {
            const double ring = distance - 3.0;
            if (std::abs(ring) < 11.0)
                intensity = 0.72 * std::exp(-0.5 * ring * ring / (3.8 * 3.8)) +
                            0.25 * std::exp(-0.5 * ring * ring / (1.2 * 1.2));
        } else if (effect == OverlayEffect::Border) {
            intensity = std::clamp(1.6 - std::abs(distance - 2.5), 0.0, 1.0);
        } else {
            // Gradiente contínuo dentro/fora, com centro levemente colorido e pico translúcido.
            const double outside = std::max(distance, 0.0);
            intensity = 0.60 * std::exp(-0.5 * distance * distance / (5.5 * 5.5)) +
                        0.11 * std::exp(-0.5 * distance * distance / (8.0 * 8.0)) +
                        0.06 * std::exp(-0.5 * outside * outside / (8.0 * 8.0));
        }
        const unsigned alpha = static_cast<unsigned>(std::lround(std::clamp(intensity, 0.0, 1.0) * 255));
        const auto i = (static_cast<std::size_t>(y) * width + x) * 4;
        image.bgra[i] = static_cast<std::uint8_t>((blue * alpha + 127) / 255);
        image.bgra[i + 1] = static_cast<std::uint8_t>((green * alpha + 127) / 255);
        image.bgra[i + 2] = static_cast<std::uint8_t>((red * alpha + 127) / 255);
        image.bgra[i + 3] = static_cast<std::uint8_t>(alpha);
    }
    if (!remaining || !std::isfinite(*remaining) || *remaining <= 0 || *remaining > 1) return image;
    const double clearedAngle = (1.0 - *remaining) * 2.0 * std::numbers::pi;
    // Aro sobre o perímetro; retângulos usam a elipse inscrita. O centro e a aura ficam preservados.
    for (int y = padding - 3; y < height - padding + 3; ++y)
    for (int x = padding - 3; x < width - padding + 3; ++x) {
        const double nx = (x + 0.5 - padding - halfWidth) / halfWidth;
        const double ny = (y + 0.5 - padding - halfHeight) / halfHeight;
        const double radial = std::hypot(nx, ny);
        if (radial == 0) continue;
        // Distância aproximada pela normal da elipse mantém a espessura também em áreas alongadas.
        const double distance = std::abs((radial - 1.0) * radial / std::hypot(nx / halfWidth, ny / halfHeight));
        if (distance >= 3.0) continue;
        double angle = std::atan2(nx, -ny);
        if (angle < 0) angle += 2.0 * std::numbers::pi;
        if (angle < clearedAngle) continue;
        const auto i = (static_cast<std::size_t>(y) * width + x) * 4;
        const auto over = [&](unsigned r, unsigned g, unsigned b, double coverage) {
            const unsigned alpha = static_cast<unsigned>(std::lround(coverage * 255));
            image.bgra[i] = static_cast<std::uint8_t>((b * alpha + image.bgra[i] * (255 - alpha) + 127) / 255);
            image.bgra[i + 1] = static_cast<std::uint8_t>((g * alpha + image.bgra[i + 1] * (255 - alpha) + 127) / 255);
            image.bgra[i + 2] = static_cast<std::uint8_t>((r * alpha + image.bgra[i + 2] * (255 - alpha) + 127) / 255);
            image.bgra[i + 3] = static_cast<std::uint8_t>(alpha + (image.bgra[i + 3] * (255 - alpha) + 127) / 255);
        };
        // Contraste escuro e cor levemente iluminada deixam o relógio legível sobre a aura.
        over(0, 0, 0, 0.9 * std::clamp(3.0 - distance, 0.0, 1.0));
        over((3 * red + 255) / 4, (3 * green + 255) / 4, (3 * blue + 255) / 4,
             std::clamp(1.8 - distance, 0.0, 1.0));
    }
    return image;
}

std::uint8_t overlayEffectOpacity(OverlayEffect effect, std::uint64_t elapsedMs) {
    if (effect != OverlayEffect::Pulse) return 255;
    const double phase = (elapsedMs % 1500) * (2.0 * std::numbers::pi / 1500.0);
    return static_cast<std::uint8_t>(std::lround(230.0 + 25.0 * std::cos(phase)));
}

std::optional<std::uint32_t> skillAccentColor(const Image& image) {
    if (!image.valid() || image.width < 8 || image.height < 8 || image.width > 16384 || image.height > 16384)
        return std::nullopt;
    struct Bucket { double weight = 0, red = 0, green = 0, blue = 0; int count = 0; };
    std::array<Bucket, 24> palette{};
    int samples = 0, colored = 0;
    double totalWeight = 0;
    const int step = std::max(1, std::min(image.width, image.height) / 128);
    // ponytail: matiz central é heurística; arte sem cor suficiente continua pedindo escolha manual.
    // Recorte conservador remove moldura e a faixa inferior dos números/atalhos.
    for (int y = (image.height * 14 + 99) / 100; y < image.height * 72 / 100; y += step) {
        for (int x = (image.width * 16 + 99) / 100; x < image.width * 84 / 100; x += step) {
            ++samples;
            const auto i = (static_cast<std::size_t>(y) * image.width + x) * 4;
            const double r = image.bgra[i + 2], g = image.bgra[i + 1], b = image.bgra[i];
            const double high = std::max({r, g, b}), low = std::min({r, g, b}), chroma = high - low;
            if (high < 65 || chroma < 38 || chroma / high < 0.32) continue;
            double hue = high == r ? (g - b) / chroma : high == g ? (b - r) / chroma + 2.0 : (r - g) / chroma + 4.0;
            if (hue < 0) hue += 6.0;
            const int bin = std::min(23, static_cast<int>(hue * 4.0));
            const double dx = (x + 0.5) / image.width - 0.5, dy = (y + 0.5) / image.height - 0.43;
            const double weight = (chroma / high) * (chroma / high) * (0.25 + 0.75 * high / 255.0) *
                                  (1.0 - std::min(0.8, 2.0 * std::hypot(dx, dy)));
            auto& bucket = palette[bin];
            bucket.weight += weight; bucket.red += r * weight; bucket.green += g * weight;
            bucket.blue += b * weight; ++bucket.count; ++colored; totalWeight += weight;
        }
    }
    if (colored < std::max(12, samples / 12)) return std::nullopt;
    Bucket best;
    for (int bin = 0; bin < 24; ++bin) {
        Bucket nearby;
        for (int offset = -1; offset <= 1; ++offset) {
            const auto& bucket = palette[(bin + offset + 24) % 24];
            nearby.weight += bucket.weight; nearby.red += bucket.red; nearby.green += bucket.green;
            nearby.blue += bucket.blue; nearby.count += bucket.count;
        }
        if (nearby.weight > best.weight) best = nearby;
    }
    if (best.count < std::max(12, samples / 20) || best.weight < totalWeight * 0.28 || best.weight <= 0)
        return std::nullopt;
    const auto red = static_cast<std::uint32_t>(std::lround(best.red / best.weight));
    const auto green = static_cast<std::uint32_t>(std::lround(best.green / best.weight));
    const auto blue = static_cast<std::uint32_t>(std::lround(best.blue / best.weight));
    return red | (green << 8) | (blue << 16);
}
}
