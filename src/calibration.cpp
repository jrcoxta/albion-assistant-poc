#include "calibration.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <vector>

namespace aa {
namespace {
Image resizeImage(const Image& source, int width, int height) {
    Image out{width, height, std::vector<std::uint8_t>(std::size_t(width) * height * 4)};
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const auto from = (std::size_t(y * source.height / height) * source.width + x * source.width / width) * 4;
        std::copy_n(source.bgra.data() + from, 4, out.bgra.data() + (std::size_t(y) * width + x) * 4);
    }
    return out;
}
}

Image cropImage(const Image& image, Region region) {
    if (!image.valid() || !region.valid() || region.x > image.width - region.width ||
        region.y > image.height - region.height) {
        throw std::invalid_argument("Recorte fora dos limites da imagem");
    }
    Image out{region.width, region.height,
              std::vector<std::uint8_t>(std::size_t(region.width) * region.height * 4)};
    for (int y = 0; y < region.height; ++y) {
        const auto* source = image.bgra.data() + (std::size_t(region.y + y) * image.width + region.x) * 4;
        auto* target = out.bgra.data() + std::size_t(y) * region.width * 4;
        std::copy_n(source, std::size_t(region.width) * 4, target);
    }
    return out;
}

std::optional<Region> suggestIcon(const Image& image, int x, int y, const Recognizer& recognizer) {
    if (!image.valid() || x < 0 || y < 0 || x >= image.width || y >= image.height) return std::nullopt;

    struct Candidate {
        Region region;
        float confidence;
    };
    std::vector<Candidate> candidates;
    std::map<int, float> scores;
    const int largest = std::min({256, image.width, image.height});

    auto testSize = [&](int size) {
        if (size < 24 || size > largest) return -1.f;
        if (const auto previous = scores.find(size); previous != scores.end()) return previous->second;
        const int radius = size + size / 2;
        const int left = std::max(0, x - radius);
        const int top = std::max(0, y - radius);
        const int right = std::min(image.width, x + radius + 1);
        const int bottom = std::min(image.height, y + radius + 1);
        const Region search{left, top, right - left, bottom - top};
        constexpr int normalizedSize = 64;
        const int normalizedWidth = std::max(normalizedSize,
            static_cast<int>(std::lround(double(search.width) * normalizedSize / size)));
        const int normalizedHeight = std::max(normalizedSize,
            static_cast<int>(std::lround(double(search.height) * normalizedSize / size)));
        const auto normalized = resizeImage(cropImage(image, search), normalizedWidth, normalizedHeight);
        const auto detection = recognizer.recognize(normalized, normalizedSize);
        scores.emplace(size, detection.confidence);
        if (detection.presence == Presence::Present) {
            Region region{search.x + static_cast<int>(std::lround(double(detection.icon.x) * search.width / normalized.width)),
                          search.y + static_cast<int>(std::lround(double(detection.icon.y) * search.height / normalized.height)),
                          size, size};
            if (region.x <= x && x < region.x + region.width && region.y <= y && y < region.y + region.height &&
                region.x >= 0 && region.y >= 0 && region.x <= image.width - region.width &&
                region.y <= image.height - region.height) {
                candidates.push_back({region, detection.confidence});
            }
        }
        return detection.confidence;
    };

    std::vector<std::pair<float, int>> coarse;
    for (int size = 24; size < largest;) {
        coarse.push_back({testSize(size), size});
        const int next = (size * 5 + 3) / 4;
        size = std::max(size + 1, next);
    }
    coarse.push_back({testSize(largest), largest});
    std::sort(coarse.begin(), coarse.end(), std::greater<>());
    for (std::size_t seed = 0; seed < std::min<std::size_t>(2, coarse.size()); ++seed) {
        int current = coarse[seed].second;
        for (int step = std::max(1, current / 8); step >= 1;) {
            const float middle = testSize(current);
            const int lower = std::max(24, current - step);
            const int upper = std::min(largest, current + step);
            const float lowScore = testSize(lower);
            const float highScore = testSize(upper);
            if (lowScore > middle && lowScore >= highScore) current = lower;
            else if (highScore > middle) current = upper;
            else step /= 2;
        }
        for (int size = current - 3; size <= current + 3; ++size) testSize(size);
    }
    if (candidates.empty()) return std::nullopt;

    const auto best = std::max_element(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.confidence < b.confidence;
    });
    for (const auto& candidate : candidates) {
        const bool different = std::abs(candidate.region.x - best->region.x) > best->region.width / 2 ||
                               std::abs(candidate.region.y - best->region.y) > best->region.height / 2;
        if (different && candidate.confidence >= best->confidence - .02f) return std::nullopt;
    }
    return best->region;
}
}
