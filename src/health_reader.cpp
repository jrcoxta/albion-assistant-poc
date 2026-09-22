#include "health_reader.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace aa {
namespace {
struct Pixel { std::uint8_t blue, green, red; };

Pixel pixelAt(const Image& image, int x, int y) {
    const auto* pixel = image.bgra.data() + (static_cast<std::size_t>(y) * image.width + x) * 4;
    return {pixel[0], pixel[1], pixel[2]};
}

bool isBrightRed(Pixel pixel) {
    return pixel.red >= 80 && pixel.red > pixel.green + 35 && pixel.red > pixel.blue + 35;
}

bool isFilled(Pixel pixel, const HealthCalibration& calibration) {
    const int minimum = std::max(70, static_cast<int>(std::lround(calibration.red * .55f)));
    return pixel.red >= minimum && pixel.red > pixel.green + 25 && pixel.red > pixel.blue + 25;
}
}

HealthCalibration calibrateHealth(const Image& image) {
    if (!image.valid()) return {};

    int bestStart = 0;
    int bestEnd = 0;
    int bestRow = 0;
    for (int y = 0; y < image.height; ++y) {
        int runStart = 0;
        for (int x = 0; x <= image.width; ++x) {
            if (x < image.width && isBrightRed(pixelAt(image, x, y))) continue;
            if (x - runStart > bestEnd - bestStart) {
                bestStart = runStart;
                bestEnd = x;
                bestRow = y;
            }
            runStart = x + 1;
        }
    }

    if (bestEnd - bestStart < std::max(24, image.width / 12)) return {};

    const auto rowMatches = [&](int y) {
        int filled = 0;
        for (int x = bestStart; x < bestEnd; ++x) filled += isBrightRed(pixelAt(image, x, y)) ? 1 : 0;
        return filled * 10 >= (bestEnd - bestStart) * 9;
    };
    int top = bestRow;
    int bottom = bestRow + 1;
    while (top > 0 && rowMatches(top - 1)) --top;
    while (bottom < image.height && rowMatches(bottom)) ++bottom;
    if (bottom - top < 3) return {};

    unsigned red = 0;
    unsigned green = 0;
    unsigned blue = 0;
    unsigned count = 0;
    for (int y = top; y < bottom; ++y) for (int x = bestStart; x < bestEnd; ++x) {
        const auto pixel = pixelAt(image, x, y);
        if (!isBrightRed(pixel)) continue;
        red += pixel.red;
        green += pixel.green;
        blue += pixel.blue;
        ++count;
    }
    if (!count) return {};
    return {bestStart, top, bestEnd - bestStart, bottom - top,
            static_cast<std::uint8_t>(red / count), static_cast<std::uint8_t>(green / count),
            static_cast<std::uint8_t>(blue / count)};
}

std::optional<float> readHealthFraction(const Image& image, const HealthCalibration& calibration) {
    if (!image.valid() || !calibration.valid() || calibration.x < 0 || calibration.y < 0 ||
        calibration.x + calibration.width > image.width || calibration.y + calibration.height > image.height) return std::nullopt;

    std::array<float, 3> fractions{};
    const std::array<int, 3> rows{calibration.y, calibration.y + calibration.height / 2,
                                  calibration.y + calibration.height - 1};
    for (std::size_t sample = 0; sample < rows.size(); ++sample) {
        int rightmost = calibration.x - 1;
        for (int x = calibration.x; x < calibration.x + calibration.width; ++x) {
            if (isFilled(pixelAt(image, x, rows[sample]), calibration)) rightmost = x;
        }
        fractions[sample] = static_cast<float>(rightmost - calibration.x + 1) / calibration.width;
    }
    std::sort(fractions.begin(), fractions.end());
    if (fractions.back() - fractions.front() > .15f) return std::nullopt;
    return std::clamp(fractions[1], 0.f, 1.f);
}
}
