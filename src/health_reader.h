#pragma once
#include "image.h"
#include <cstdint>
#include <optional>

namespace aa {
struct HealthCalibration {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;

    bool valid() const { return width >= 24 && height >= 3 && red > 0; }
};

HealthCalibration calibrateHealth(const Image& image);
std::optional<float> readHealthFraction(const Image& image, const HealthCalibration& calibration);
}
