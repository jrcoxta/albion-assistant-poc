#pragma once
#include "image.h"
#include "model.h"
namespace aa {
class Recognizer {
public:
    explicit Recognizer(const std::filesystem::path& assetsDir = {});
    void setReference(const Image& image);
    bool setClockReference(const Image& image);
    bool clockReady() const;
    void clearStackReferences();
    // Recortes de 24..256 px com contador branco, rótulos 1..99; inválidos não alteram amostras.
    bool setStackReference(unsigned value, const Image& image);
    Detection recognize(const Image& image, int iconSize, RegionShape searchShape=RegionShape::Rectangle) const;
    // Tolerância local para a medição manual; a calibração mantém a busca exata.
    Detection recognizeNearSize(const Image& image, int iconSize, RegionShape searchShape=RegionShape::Rectangle) const;
private:
    struct StackReference { unsigned value; Image image; };
    std::vector<Image> references_;
    std::vector<StackReference> stackReferences_;
    Image clockReference_;
};
}
