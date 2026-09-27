#pragma once
#include "image.h"
#include "model.h"
#include <cstdint>
#include <optional>

namespace aa {
enum class OverlayEffect { Border = 0, Glow = 1, Pulse = 2, Halo = 3, Flames = 4 };

// BGRA premultiplicado; brilho e pulso também iluminam a habilidade com baixa opacidade.
int overlayEffectPadding(OverlayEffect effect, int iconWidth, int iconHeight, RegionShape shape = RegionShape::Rectangle);
// remaining é fração observada [0,1]; desconhecida/inválida omite o aro. Apaga no sentido horário desde 12h.
Image renderOverlayEffect(int iconWidth, int iconHeight, OverlayEffect effect, std::uint32_t color,
                          RegionShape shape = RegionShape::Rectangle, std::optional<float> remaining = {},
                          std::uint64_t elapsedMs = 0);
std::uint8_t overlayEffectOpacity(OverlayEffect effect, std::uint64_t elapsedMs);

// COLORREF: r | (g << 8) | (b << 16). Imagem neutra não inventa uma cor.
std::optional<std::uint32_t> skillAccentColor(const Image& image);
}
