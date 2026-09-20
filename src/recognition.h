#pragma once
#include "image.h"
#include "model.h"
namespace aa {
class Recognizer {
public:
    explicit Recognizer(const std::filesystem::path& assetsDir = {});
    void setReference(const Image& image);
    Detection recognize(const Image& image, int iconSize) const;
private:
    std::vector<Image> references_;
    std::vector<Image> digits_;
};
}
