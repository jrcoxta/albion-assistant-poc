#pragma once
#include "recognition.h"
#include <optional>

namespace aa {
std::optional<Region> suggestIcon(const Image& image, int x, int y, const Recognizer& recognizer);
Image cropImage(const Image& image, Region region);
}
