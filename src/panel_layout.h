#pragma once
#include <algorithm>

namespace aa {
// Dimensões lógicas do painel; o espaço disponível já desconta a moldura.
inline double panelScale(int availableWidth, int availableHeight, double dpiScale) {
    return std::min({dpiScale, std::max(1, availableWidth) / 860.0,
                     std::max(1, availableHeight) / 700.0});
}
}
