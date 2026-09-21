#pragma once
#include "image.h"
#include "model.h"
namespace aa {
class Recognizer {
public:
    explicit Recognizer(const std::filesystem::path& assetsDir = {});
    void setReference(const Image& image);
    void clearStackReferences();
    // Recortes de 24..256 px com contador branco, rótulos 1..99; inválidos não alteram amostras.
    bool setStackReference(unsigned value, const Image& image);
    Detection recognize(const Image& image, int iconSize) const;
private:
    struct StackReference { unsigned value; Image image; };
    std::vector<Image> references_;
    std::vector<StackReference> stackReferences_;
};
}
