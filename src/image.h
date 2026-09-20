#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
namespace aa {
struct Image {
    int width = 0, height = 0;
    std::vector<std::uint8_t> bgra;
    bool valid() const { return width > 0 && height > 0 && bgra.size() == static_cast<std::size_t>(width) * height * 4; }
};
Image loadImage(const std::filesystem::path& path);
void saveImage(const Image& image, const std::filesystem::path& path);
}
