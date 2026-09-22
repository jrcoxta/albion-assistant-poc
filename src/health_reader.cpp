#include "health_reader.h"
#include <algorithm>
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
    return pixel.red >= 80 && pixel.red > pixel.green + 65 && pixel.red > pixel.blue + 55;
}

bool isFilled(Pixel pixel, const HealthCalibration& calibration) {
    const int minimum = std::max(70, static_cast<int>(std::lround(calibration.red * .55f)));
    return pixel.red >= minimum && pixel.red > pixel.green + 25 && pixel.red > pixel.blue + 25;
}
}

HealthCalibration calibrateHealth(const Image& image) {
    if (!image.valid()) return {};

    int bestStart = 0, bestEnd = 0, bestRow = 0, bestScore = 0;
    const int minimumRun = std::max(24, image.width / 12);
    for (int y = 0; y < image.height; ++y) {
        int runStart = 0;
        int start = image.width, end = 0, score = 0;
        for (int x = 0; x <= image.width; ++x) {
            if (x < image.width && isBrightRed(pixelAt(image, x, y))) continue;
            if (x - runStart >= minimumRun) {
                start = std::min(start, runStart);
                end = x;
                score += x - runStart;
            }
            runStart = x + 1;
        }
        if (score > bestScore) {
            bestStart = start; bestEnd = end; bestRow = y; bestScore = score;
        }
    }

    if (bestEnd - bestStart < std::max(24, image.width / 12)) return {};

    const auto rowMatches = [&](int y) {
        int filled = 0;
        for (int x = bestStart; x < bestEnd; ++x) filled += isBrightRed(pixelAt(image, x, y)) ? 1 : 0;
        return filled * 10 >= (bestEnd - bestStart) * 4;
    };
    int top = bestRow;
    int bottom = bestRow + 1;
    while (top > 0 && rowMatches(top - 1)) --top;
    while (bottom < image.height && rowMatches(bottom)) ++bottom;
    if (bottom - top < 5) return {};

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

    std::vector<float> fractions;
    for (int y = calibration.y; y < calibration.y + calibration.height; ++y) {
        int rightmost = calibration.x - 1;
        int runStart = -1;
        for (int x = calibration.x; x <= calibration.x + calibration.width; ++x) {
            if (x < calibration.x + calibration.width && isFilled(pixelAt(image, x, y), calibration)) {
                if (runStart < 0) runStart = x;
                continue;
            }
            if (runStart < 0) continue;
            if (rightmost < calibration.x ? runStart <= calibration.x + std::max(2, calibration.width / 40) :
                runStart - rightmost - 1 <= calibration.width / 3)
                rightmost = x - 1;
            runStart = -1;
        }
        if (rightmost >= calibration.x)
            fractions.push_back(static_cast<float>(rightmost - calibration.x + 1) / calibration.width);
    }
    if (fractions.size() < static_cast<std::size_t>(std::max(3, calibration.height / 2))) return std::nullopt;
    std::sort(fractions.begin(), fractions.end());
    const float median = fractions[fractions.size() / 2];
    const auto consistent = std::count_if(fractions.begin(), fractions.end(),
        [&](float value) { return std::abs(value - median) <= .08f; });
    if (static_cast<std::size_t>(consistent) * 5 < fractions.size() * 3) return std::nullopt;
    return std::clamp(median, 0.f, 1.f);
}
}
