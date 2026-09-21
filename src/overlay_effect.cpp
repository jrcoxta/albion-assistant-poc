#include "overlay_effect.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace aa {
int overlayEffectPadding(OverlayEffect effect, int iconWidth, int iconHeight) {
    if (iconWidth <= 0 || iconHeight <= 0 || iconWidth > 16384 || iconHeight > 16384) return 0;
    switch (effect) {
    case OverlayEffect::Border: return 6;
    case OverlayEffect::Glow:
    case OverlayEffect::Pulse: return 18;
    case OverlayEffect::Halo:
        return static_cast<int>(std::ceil(std::max(iconWidth, iconHeight) * 0.208)) + 16;
    }
    return 0;
}

Image renderOverlayEffect(int iconWidth, int iconHeight, OverlayEffect effect, std::uint32_t color) {
    const int padding = overlayEffectPadding(effect, iconWidth, iconHeight);
    if (!padding) return {};
    const int width = iconWidth + 2 * padding, height = iconHeight + 2 * padding;
    if (static_cast<std::uint64_t>(width) * height > 64000000) return {};
    Image image{width, height, std::vector<std::uint8_t>(static_cast<std::size_t>(width) * height * 4)};
    const double halfWidth = iconWidth / 2.0, halfHeight = iconHeight / 2.0;
    const double radius = effect == OverlayEffect::Border ? 5.0 : std::min(halfWidth, halfHeight) * 0.32 + 2.0;
    const double extension = effect == OverlayEffect::Border ? 2.5 : 2.0;
    const double ellipseX = halfWidth * std::numbers::sqrt2 + 3.0;
    const double ellipseY = halfHeight * std::numbers::sqrt2 + 3.0;
    const unsigned red = color & 255u, green = (color >> 8) & 255u, blue = (color >> 16) & 255u;
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        if (x >= padding && x < padding + iconWidth && y >= padding && y < padding + iconHeight) continue;
        const double dx = std::abs(x + 0.5 - padding - halfWidth);
        const double dy = std::abs(y + 0.5 - padding - halfHeight);
        double intensity = 0;
        if (effect == OverlayEffect::Halo) {
            const double normalized = std::hypot(dx / ellipseX, dy / ellipseY);
            const double distance = std::hypot(dx, dy) * std::abs(1.0 - 1.0 / normalized);
            if (distance < 11.0)
                intensity = 0.72 * std::exp(-0.5 * distance * distance / (3.8 * 3.8)) +
                            0.25 * std::exp(-0.5 * distance * distance / (1.2 * 1.2));
        } else {
            // Distância a um retângulo arredondado: cantos suaves sem pintar o ícone.
            const double qx = dx - (halfWidth + extension - radius);
            const double qy = dy - (halfHeight + extension - radius);
            const double distance = std::hypot(std::max(qx, 0.0), std::max(qy, 0.0)) +
                                    std::min(std::max(qx, qy), 0.0) - radius;
            if (effect == OverlayEffect::Border) intensity = std::clamp(1.6 - std::abs(distance), 0.0, 1.0);
            else if (distance < 15.0) {
                const double outside = std::max(distance, 0.0);
                intensity = 0.88 * std::exp(-0.5 * outside * outside / (4.5 * 4.5)) +
                            0.10 * std::exp(-0.5 * outside * outside / (8.0 * 8.0));
            }
        }
        const unsigned alpha = static_cast<unsigned>(std::lround(std::clamp(intensity, 0.0, 1.0) * 255));
        const auto i = (static_cast<std::size_t>(y) * width + x) * 4;
        image.bgra[i] = static_cast<std::uint8_t>((blue * alpha + 127) / 255);
        image.bgra[i + 1] = static_cast<std::uint8_t>((green * alpha + 127) / 255);
        image.bgra[i + 2] = static_cast<std::uint8_t>((red * alpha + 127) / 255);
        image.bgra[i + 3] = static_cast<std::uint8_t>(alpha);
    }
    return image;
}

std::uint8_t overlayEffectOpacity(OverlayEffect effect, std::uint64_t elapsedMs) {
    if (effect != OverlayEffect::Pulse) return 255;
    const double phase = (elapsedMs % 1500) * (2.0 * std::numbers::pi / 1500.0);
    return static_cast<std::uint8_t>(std::lround(207.5 + 47.5 * std::cos(phase)));
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
