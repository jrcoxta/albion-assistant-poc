#pragma once
#include "image.h"
#include <cstdint>
#include <optional>

namespace aa {
enum class OverlayEffect { Border = 0, Glow = 1, Pulse = 2, Halo = 3 };

// O bitmap é BGRA premultiplicado; o retângulo inteiro da habilidade fica vazio.
int overlayEffectPadding(OverlayEffect effect, int iconWidth, int iconHeight);
Image renderOverlayEffect(int iconWidth, int iconHeight, OverlayEffect effect, std::uint32_t color);
std::uint8_t overlayEffectOpacity(OverlayEffect effect, std::uint64_t elapsedMs);

// COLORREF: r | (g << 8) | (b << 16). Imagem neutra não inventa uma cor.
std::optional<std::uint32_t> skillAccentColor(const Image& image);
}
