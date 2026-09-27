#pragma once
#include "image.h"
#include "model.h"
namespace aa {
class Recognizer {
public:
    explicit Recognizer(const std::filesystem::path& assetsDir = {});
    void setReference(const Image& image);
    // Apenas o recurso nativo do Assassino recebe tolerância extra de ajuste;
    // referências capturadas e outros status mantêm o critério conservador.
    bool setClockReference(const Image& image, bool nativeAssassin = false);
    bool setClockFallback(const Image& image, bool nativeAssassin = false);
    bool clockReady() const;
    void clearStackReferences();
    // Recortes de 24..256 px com contador branco, rótulos 1..99; inválidos não alteram amostras.
    bool setStackReference(unsigned value, const Image& image);
    Detection recognize(const Image& image, int iconSize, RegionShape searchShape=RegionShape::Rectangle) const;
    // Tolerância local para a medição manual; a calibração mantém a busca exata.
    Detection recognizeNearSize(const Image& image, int iconSize, RegionShape searchShape=RegionShape::Rectangle) const;
private:
    struct StackReference { unsigned value; Image image; };
    std::optional<unsigned> readStacks(const Image& image,Region icon) const;
    std::vector<Image> references_;
    std::vector<StackReference> stackReferences_;
    Image clockReference_;
    Image clockFallback_;
    bool nativeClockReference_ = false;
    bool nativeClockFallback_ = false;
};
}
