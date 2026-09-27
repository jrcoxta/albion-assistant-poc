#include "skill_reader.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace aa {
bool skillReady(const Image& current, const Image& reference, RegionShape shape) {
    if (!current.valid() || !reference.valid() || current.width != reference.width ||
        current.height != reference.height || current.width < 24 || current.height < 24 ||
        current.width > 256 || current.height > 256 ||
        (shape != RegionShape::Rectangle && shape != RegionShape::Circle) ||
        (shape == RegionShape::Circle && current.width != current.height)) return false;
    // O recorte circular ainda é um quadrado. A faixa exterior de 2 px
    // mistura cenário e borda animada; nas capturas do D pronto, quase todas
    // as diferenças ocorrem aí. Contador e sombra dentro do ícone permanecem
    // na comparação, inclusive o último segundo da recarga observado.
    int count = 0, different = 0, darker = 0, informative = 0;
    for (int y = 0; y < current.height; ++y) {
        for (int x = 0; x < current.width; ++x) {
            if (shape == RegionShape::Circle) {
                const int dx = 2 * x + 1 - current.width;
                const int dy = 2 * y + 1 - current.height;
                const int diameter = current.width - 4;
                if (dx * dx + dy * dy > diameter * diameter) continue;
            }
            const auto i = (static_cast<std::size_t>(y) * current.width + x) * 4;
            int delta = 0, refLight = 0, nowLight = 0;
            for (int c = 0; c < 3; ++c) {
                delta += std::abs(int(current.bgra[i+c]) - int(reference.bgra[i+c]));
                refLight += reference.bgra[i+c]; nowLight += current.bgra[i+c];
            }
            ++count;
            informative += refLight > 120;
            different += delta > 24;
            darker += refLight - nowLight > 36;
        }
    }
    // Um ícone vazio/preto não comprova disponibilidade. A referência precisa
    // ser da habilidade equipada na mesma HUD para que esta comparação valha.
    return informative >= count / 3 &&
           different * 500 <= count && darker * 500 <= count;
}
}
